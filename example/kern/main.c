/*
 * NUX: A kernel Library. Copyright (C) 2019 Gianluca Guida,
 * glguida@tlbflush.org
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * See COPYING file for the full license.
 * 
 * SPDX-License-Identifier:	GPL2.0+
 */

#include <stdio.h>
#include <nux/nux.h>

uctxt_t u_init;

int
main (int argc, char *argv[])
{
  printf ("Hello, %s (%" PRIx64 ")!", argv[1], timer_gettime ());

  timer_alarm (1 * 1000 * 1000 * 1000);

  kmem_trim_setmode (TRIM_BRK);

  for (int i = 0; i < 2; i++)
    {
      uintptr_t x1, y1, x2, y2;

      x1 = kmem_alloc (0, 61234);
      y1 = kmem_alloc (1, 5123);
      x2 = kmem_alloc (0, 61234);
      y2 = kmem_alloc (1, 5123);

      kmem_free (1, y2, 5123);
      kmem_free (0, x2, 61234);
      kmem_free (1, y1, 5123);
      kmem_free (0, x1, 61234);

      kmem_trim_one (TRIM_BRK);
    }

  if (!uctxt_bootstrap (&u_init))
    {
      printf ("NO USER PROCESS.");
    }
  else
    {
      cpu_ipi (cpu_id (), cpu_ipi_base () + 0);
    }
  return EXIT_IDLE;
}

int
main_ap (void)
{
  printf ("%d: %" PRIx64 "\n", cpu_id (), timer_gettime ());
  return EXIT_IDLE;
}

uctxt_t *
entry_sysc (uctxt_t * u,
	    unsigned long a1, unsigned long a2, unsigned long a3,
	    unsigned long a4, unsigned long a5, unsigned long a6)
{
  switch (a1)
    {
    case 4096:
      putchar (a2);
      break;
    case 0:
      info ("User exited with error code: %ld", a2);
      return UCTXT_IDLE;
    default:
      error ("Unknown syscall");
      break;
    }
  return u;
}

uctxt_t *
entry_ipi (uctxt_t * uctxt, unsigned ipi)
{
  info ("IPI!");
  return &u_init;
}

uctxt_t *
entry_alarm (uctxt_t * uctxt)
{
  timer_alarm (1 * 1000 * 1000 * 1000);
  info ("TMR: %" PRIu64 " us", timer_gettime ());
  uctxt_print (uctxt);
  return uctxt;
}

uctxt_t *
entry_ex (uctxt_t * uctxt, unsigned ex)
{
  info ("Exception %d", ex);
  uctxt_print (uctxt);
  return UCTXT_IDLE;
}

uctxt_t *
entry_pf (uctxt_t * uctxt, vaddr_t va, hal_pfinfo_t pfi)
{
  info ("CPU #%d Pagefault at %08lx (%d)", cpu_id (), va, pfi);
  uctxt_print (uctxt);
  return UCTXT_IDLE;
}

uctxt_t *
entry_irq (uctxt_t * uctxt, unsigned irq, bool lvl)
{
  info ("IRQ %d", irq);
  return uctxt;

}
