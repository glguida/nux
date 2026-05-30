/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <stddef.h>
#include <nux/hal.h>
#include <nux/nux.h>
#include <nux/apxh.h>

#include "internal.h"

#define PLTACPI_INVALID_IRQ ((unsigned)-1)
unsigned pltacpi_hpet_irq = PLTACPI_INVALID_IRQ;

void
plt_init (void)
{
  const struct apxh_pltdesc *desc;

  desc = hal_pltinfo ();
  if (desc == NULL)
    fatal ("Invalid PLT Boot Table.");

  if (desc->type != PLT_ACPI)
    fatal ("No ACPI RSDP found.");

  printf ("RSDP: %llx\n", desc->pltptr);

  acpi_init (desc->pltptr);
  acpi_madt_scan ();
  gsi_start ();

  acpi_hpet_scan ();
}

void
plt_hw_putc (int c)
{
  /* Only HAL putc for now. */
  hal_putchar (c);
}


#include "apic.h"

struct hal_frame *
plt_interrupt (unsigned vect, struct hal_frame *f)
{
  struct hal_frame *r;

  if (vect >= APIC_VECT_MAX)
    {
      /* Something wrong here. */
      warn ("HAL vector %d outside of bounds.", vect);
      r = f;
    }
  else if (vect >= APIC_VECT_IPIBASE)
    {
      r = hal_entry_ipi (f);
    }
  else if (vect >= APIC_VECT_IRQBASE)
    {
      unsigned irq = vect - APIC_VECT_IRQBASE;
      if (irq == pltacpi_hpet_irq)
	{
	  hpet_doirq ();
	  r = hal_entry_timer (f);
	}
      else
	{
	  r = hal_entry_irq (f, irq, plt_irq_islevel (irq));
	}
    }
  else
    {
      /* Something wrong here. */
      warn ("HAL vector %d outside of bounds", vect);
      r = f;
    }

  return r;
}

void
plt_eoi_ipi (void)
{
  lapic_eoi ();
}

void
plt_eoi_irq (unsigned irq)
{
  lapic_eoi ();
}

void
plt_eoi_timer (void)
{
  lapic_eoi ();
}
