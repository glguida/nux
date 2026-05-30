/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <inttypes.h>
#include <assert.h>
#include <nux/nux.h>
#include <nux/hal.h>
#include <nux/plt.h>
#include <nux/nux.h>

#include "internal.h"
#include "apic.h"

unsigned ioapics_no;

struct ioapic_desc
{
  void *base;
  unsigned irq;
  unsigned pins;
} *ioapics;
unsigned gsis_no;

struct gsi_desc
{
  unsigned irq;
  unsigned ioapic;
  unsigned pin;
  enum plt_irq_type mode;
} *gsis;

#define IOAPIC_SIZE 0x20

/* Memory Registers */
#define IO_REGSEL 0x00
#define IO_WIN    0x10

/* I/O Registers */
#define IO_ID          0x00
#define IO_VER         0x01
#define IO_ARB         0x02
#define IO_RED_LO(x)   (0x10 + 2*(x))
#define IO_RED_HI(x)   (0x11 + 2*(x))

void
ioapic_init (unsigned no)
{

  ioapics =
    (struct ioapic_desc *) kmem_brkgrow (1,
					 (sizeof (struct ioapic_desc) * no));
  ioapics_no = no;
}

static void
ioapic_write (unsigned i, uint8_t reg, uint32_t val)
{
  volatile uint32_t *regsel = (uint32_t *) (ioapics[i].base + IO_REGSEL);
  volatile uint32_t *win = (uint32_t *) (ioapics[i].base + IO_WIN);

  *regsel = reg;
  *win = val;
}

static uint32_t
ioapic_read (unsigned i, uint8_t reg)
{
  volatile uint32_t *regsel = (uint32_t *) (ioapics[i].base + IO_REGSEL);
  volatile uint32_t *win = (uint32_t *) (ioapics[i].base + IO_WIN);

  *regsel = reg;
  return *win;
}

void
ioapic_add (unsigned num, uint64_t base, unsigned irqbase)
{
  unsigned i;

  ioapics[num].base = kva_physmap (base, IOAPIC_SIZE, HAL_PTE_P | HAL_PTE_W);
  ioapics[num].irq = irqbase;
  ioapics[num].pins = 1 + ((ioapic_read (num, IO_VER) >> 16) & 0xff);

  /* Mask all interrupts */
  for (i = 0; i < ioapics[num].pins; i++)
    {
      ioapic_write (num, IO_RED_LO (i), 0x00010000);
      ioapic_write (num, IO_RED_HI (i), 0x00000000);
    }
  info ("IOAPIC: %02d PA: %08" PRIx64 " VA: %p IRQ:%02d PINS: %02d",
	num, base, ioapics[num].base, irqbase, ioapics[num].pins);
}

unsigned
ioapic_irqs (void)
{
  unsigned i;
  unsigned lirq, maxirq = 0;

  for (i = 0; i < ioapics_no; i++)
    {
      lirq = ioapics[i].irq + ioapics[i].pins;
      if (lirq > maxirq)
	maxirq = lirq;
    }

  if (maxirq >= APIC_VECT_IRQMAX)
    {
      warn
	("Maximum number of IRQs exceeded (%d >= %d).  Some IRQs will not be available.",
	 maxirq, APIC_VECT_IRQMAX);
      maxirq = APIC_VECT_IRQMAX;
    }

  return maxirq;
}

void
gsi_init (void)
{
  unsigned i, irqs = ioapic_irqs ();
  gsis_no = irqs;
  gsis =
    (struct gsi_desc *) kmem_brkgrow (1, sizeof (struct gsi_desc) * irqs);

  /* Setup identity map, edge triggered (this is ISA) */
  for (i = 0; i < 16; i++)
    {
      gsis[i].mode = PLT_IRQ_EDGE;
      gsis[i].irq = i;
    }

  for (; i < gsis_no; i++)
    {
      gsis[i].mode = PLT_IRQ_LVLLO;
      gsis[i].irq = i;
    }
}

void
gsi_setup (unsigned i, unsigned irq, enum plt_irq_type mode)
{
  if (i >= gsis_no)
    {
      warn ("Warning: GSI %d bigger than existing I/O APIC GSIs", i);
      return;
    }
  gsis[i].irq = irq;
  gsis[i].mode = mode;
}

static bool
irqresolve (unsigned gsi)
{
  unsigned i, start, end;

  for (i = 0; i < ioapics_no; i++)
    {
      start = ioapics[i].irq;
      end = start + ioapics[i].pins;

      if ((gsi >= start) && (gsi < end))
	{
	  gsis[gsi].ioapic = i;
	  gsis[gsi].pin = gsi - start;
	  return true;
	}
    }
  warn ("GSI not found in IOAPIC: %d", gsi);
  return false;
}

static void
gsi_set_irqtype (unsigned irq, enum plt_irq_type mode)
{
  uint32_t lo;

  lo = ioapic_read (gsis[irq].ioapic, IO_RED_LO (gsis[irq].pin));
  lo &= ~((1L << 13) | (1L << 15));

  gsis[irq].mode = mode;

  /* Setup Masked IOAPIC entry with no vector information */
  switch (gsis[irq].mode)
    {
    default:
      warn ("Warning: GSI table corrupted. " "Setting GSI %d to EDGE", irq);
    case PLT_IRQ_EDGE:
      break;
    case PLT_IRQ_LVLHI:
      lo |= (1L << 15);
      break;
    case PLT_IRQ_LVLLO:
      lo |= ((1L << 15) | (1L << 13));
      break;
    }

  ioapic_write (gsis[irq].ioapic, IO_RED_LO (gsis[irq].pin), lo);
}

static void
gsi_register (unsigned gsi, unsigned vect)
{
  uint32_t lo;

  assert (gsi < gsis_no);
  assert (vect < 256);

  lo = ioapic_read (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin));
  ioapic_write (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin), lo | vect);
}

void
gsi_start (void)
{
  unsigned i;
  for (i = 0; i < gsis_no; i++)
    {
      /* Now that we have the proper GSI to IRQ mapping, resolve the
       * IOAPIC/PIN of the GSI. */
      if (irqresolve (i))
	gsi_set_irqtype (i, gsis[i].mode);
    }

  /* 1:1 map GSI <-> Kernel IRQ */
  for (i = 0; i < gsis_no; i++)
    gsi_register (i, APIC_VECT_IRQBASE + i);
}

void
gsi_dump (void)
{
  unsigned i;

  for (i = 0; i < gsis_no; i++)
    {
      info ("GSI: %02d IRQ: %02d MODE: %5s APIC: %02d PIN: %02d", i,
	    gsis[i].irq,
	    gsis[i].mode == PLT_IRQ_EDGE ? "EDGE" : gsis[i].mode ==
	    PLT_IRQ_LVLHI ? "LVLHI" : "LVLLO", gsis[i].ioapic, gsis[i].pin);
    }
}

unsigned
plt_irq_no (void)
{
  return gsis_no;
}

void
plt_irq_setvector (unsigned gsi, unsigned vect)
{
  if (gsi < gsis_no)
    gsi_register (gsi, vect);
  else
    warn ("gsi requested non existent: %d", gsi);
}

enum plt_irq_type
plt_irq_type (unsigned gsi)
{
  if (gsi < gsis_no)
    return gsis[gsi].mode;
  else
    return PLT_IRQ_INVALID;
}

void
plt_irq_enable (unsigned gsi)
{
  uint32_t lo;

  lo = ioapic_read (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin));
  lo &= ~0x10000L;		/* UNMASK */
  ioapic_write (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin), lo);;
}

void
plt_irq_disable (unsigned gsi)
{
  uint32_t lo;

  lo = ioapic_read (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin));
  lo |= 0x10000L;		/* MASK */
  ioapic_write (gsis[gsi].ioapic, IO_RED_LO (gsis[gsi].pin), lo);;
}

unsigned
plt_irq_max (void)
{
  return APIC_VECT_IRQMAX;
}
