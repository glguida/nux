#include <stddef.h>
#include <nux/hal.h>
#include <nux/nux.h>

#include "internal.h"

#define PLTACPI_INVALID_VECT ((unsigned)-1)
unsigned pltacpi_hpet_vect = PLTACPI_INVALID_VECT;

void
plt_init (void)
{
  const struct hal_pltinfo_desc *desc;

  desc = hal_pltinfo ();
  if (desc == NULL)
    fatal ("Invalid PLT Boot Table.");

  if (desc->acpi_rsdp == 0)
    fatal ("No ACPI RSDP found.");

  printf ("RSDP: %llx\n", desc->acpi_rsdp);

  acpi_init(desc->acpi_rsdp);
  acpi_madt_scan();
  gsi_start();

  acpi_hpet_scan ();
}

void
plt_hw_putc (int c)
{
  /* Only HAL putc for now. */
  hal_putchar (c);
}

/*
  Process PLT IRQs

  Returns 'true' if timer alarm expired. 'false' otherwhise.
*/
bool
plt_vect_process (unsigned vect)
{
  if (pltacpi_hpet_vect != PLTACPI_INVALID_VECT && vect == pltacpi_hpet_vect)
    {
      return hpet_doirq ();
    }
  return false;
}