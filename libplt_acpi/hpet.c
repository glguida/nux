/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <sys/bitops.h>

#include "internal.h"
#include <nux/nux.h>

#define HPET_SIZE 0x1000

#define REG_GENCAP 0x00
#define LEG_RT_CAP (1L << 15)

#define REG_GENCFG 0x10
#define LEG_RT_CNF (1L << 1)
#define ENABLE_CNF (1L << 0)

#define REG_GENISR 0x20

#define REG_COUNTER 0xF0

#define REG_TMRCAP(_n) ((0x20 * (_n)) + 0x100)
#define INT_TYPE_CNF (1LL << 1)
#define INT_ENB_CNF (1LL << 2)
#define REG_TMRCMP(_n) ((0x20 * (_n)) + 0x108)

#define TMR 0

static volatile void *hpet_base;
static uint64_t period = 0;
static uint64_t gencfg;
static uint64_t tmrcfg;

static int irqlvl = 0;
static int irqno;

static uint64_t
hpet_read (uint16_t offset)
{
  uint32_t hi1, hi2, lo;
  volatile uint32_t *ptr = hpet_base + offset;

  do
    {
      hi1 = *(ptr + 1);
      lo = *ptr;
      hi2 = *(ptr + 1);
    }
  while (hi1 != hi2);

  return (uint64_t) hi1 << 32 | lo;
}

static void
hpet_write (uint16_t offset, uint64_t val)
{
  volatile uint32_t *ptr = hpet_base + offset;
  *(ptr + 1) = val >> 32;
  *ptr = (uint32_t) val;
}

static void
hpet_pause (void)
{
  hpet_write (REG_GENCFG, gencfg);
}

static void
hpet_resume (void)
{
  hpet_write (REG_GENCFG, ENABLE_CNF | gencfg);
}

void
hpet_doirq (void)
{
  if (irqlvl)
    hpet_write (REG_GENISR, hpet_read (REG_GENISR) | 1);
}

bool
hpet_init (paddr_t hpetpa)
{
  uint64_t freq;
  uint32_t period_femto;
  uint64_t gencap;
  int num;

  hpet_base = kva_physmap (hpetpa, HPET_SIZE, HAL_PTE_P | HAL_PTE_W);
  gencap = hpet_read (REG_GENCAP);
  gencfg = 0;
  period_femto = gencap >> 32;
  num = 1 + ((gencap >> 8) & 0xf);

  info ("HPET Found at %" PRIx64 ", mapped at %p", hpetpa, hpet_base);
  info ("HPET period: %" PRIx32 "x, counters: %d", period_femto, num);

  if (period_femto == 0)
    {
      error ("HPET period invalid.");
      return false;
    }
  freq = (1000000000000000LL / period_femto);
  info ("HPET counter frequency: %" PRId64 " Hz", freq);
  period = period_femto;

  hpet_pause ();		/* Stop, in case it's running */

  /* Find IRQ of first counter. */
  if (gencap & LEG_RT_CAP && (TMR < 2))
    {
      debug ("Using Legacy Routing.\n");
      gencfg |= LEG_RT_CNF;
      if (TMR == 0)
	{
	  irqno = 2;
	}
      else
	{
	  irqno = 8;
	}
    }
  else
    {
      uint32_t irqcap = hpet_read (REG_TMRCAP (TMR)) >> 32;
      if (irqcap == 0)
	{
	  error ("No IRQ available, can't use counter %d", TMR);
	  return false;
	}
      irqno = ffs32 (irqcap) - 1;
      tmrcfg |= (irqno << 9);
      debug ("Using Interrupt Routing (%d - %x).\n", irqno, irqcap);
    }

  if (plt_irq_islevel (irqno))
    {
      debug ("Using Level Interrupt");
      /* Reset ISR just in case. */
      hpet_write (REG_GENISR, hpet_read (REG_GENISR) | 1);
      irqlvl = 1;
      tmrcfg |= INT_TYPE_CNF;
    }

  /* Register HPET irq no. */
  pltacpi_hpet_irq = irqno;

  /* Start Time of Boot. */
  hpet_write (REG_COUNTER, 0);

  /* Setup Counter 0 */
  hpet_write (REG_TMRCAP (TMR), tmrcfg);

  /* Enable HPET */
  hpet_resume ();

  plt_irq_enable (irqno);
  return true;
}

uint64_t
plt_tmr_ctr (void)
{
  return hpet_read (0xf0);
}

void
plt_tmr_setctr (uint64_t ctr)
{
  hpet_pause ();
  hpet_write (0xf0, ctr);
  hpet_resume ();
}

uint64_t
plt_tmr_period (void)
{
  return period;
}

void
plt_tmr_setalm (uint64_t alm)
{
  if (alm == 0)
    alm = 1;
  hpet_pause ();
  hpet_write (REG_TMRCMP (TMR), plt_tmr_ctr () + alm);
  hpet_write (REG_TMRCAP (TMR), tmrcfg | INT_ENB_CNF);
  hpet_resume ();
}

void
plt_tmr_clralm (void)
{
  hpet_write (REG_TMRCAP (TMR), tmrcfg & ~INT_ENB_CNF);
}
