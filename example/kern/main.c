/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <assert.h>
#include <stdio.h>
#include <nux/nux.h>
#include <nux/nuxperf.h>

#include <nux/hal.h>

uctxt_t u_init;
struct hal_umap umap;

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
      cpu_ipi (cpu_id ());
    }


  hal_l1p_t l1p;
  hal_l1e_t l1e;
  hal_umap_bootstrap (&umap);
  uctxt_print (&u_init);

  for (uint64_t i = 0;; i += (1 << 12))
    {
      uint64_t x = hal_umap_next (&umap, i, &l1p, &l1e);
      if (x == UADDR_INVALID)
	break;
      printf ("%lx - %lx(%lx)\n", x, l1p, l1e);
      i = x;
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
	    unsigned long a4, unsigned long a5, unsigned long a6,
	    unsigned long a7)
{
  switch (a1)
    {
    case 0:
      info ("SYSC%ld test passed.", a1);
      break;
    case 1:
      assert (a2 == 1);
      info ("SYSC%ld test passed.", a1);
      break;
    case 2:
      assert (a2 == 1);
      assert (a3 == 2);
      info ("SYSC%ld test passed.", a1);
      break;
    case 3:
      assert (a2 == 1);
      assert (a3 == 2);
      assert (a4 == 3);
      info ("SYSC%ld test passed.", a1);
      break;
    case 4:
      assert (a2 == 1);
      assert (a3 == 2);
      assert (a4 == 3);
      assert (a5 == 4);
      info ("SYSC%ld test passed.", a1);
      break;
    case 5:
      assert (a2 == 1);
      assert (a3 == 2);
      assert (a4 == 3);
      assert (a5 == 4);
      assert (a6 == 5);
      info ("SYSC%ld test passed.", a1);
      break;
    case 6:
      assert (a2 == 1);
      assert (a3 == 2);
      assert (a4 == 3);
      assert (a5 == 4);
      assert (a6 == 5);
      assert (a7 == 6);
      info ("SYSC%ld test passed.", a1);
      break;
    case 4096:
      putchar (a2);
      break;
    case 4097:
      info ("User exited with error code: %ld", a2);
      hal_umap_load (NULL);
      hal_umap_free (&umap);
      return UCTXT_IDLE;

    default:
      info ("Received unknown syscall %ld %ld %ld %ld %ld %ld %ld\n",
	    a1, a2, a3, a4, a5, a6, a7);
      break;
    }
  return u;
}

uctxt_t *
entry_ipi (uctxt_t * uctxt)
{
  info ("IPI!");
  return &u_init;
}

static void
_print_perfctr (void *unused, nuxperf_t *ctr)
{
  printf ("ctr: %-20s\t%20ld\n", ctr->name, ctr->val);
}

uctxt_t *
entry_alarm (uctxt_t * uctxt)
{
  timer_alarm (1 * 1000 * 1000 * 1000);
  info ("TMR: %" PRIu64 " us", timer_gettime ());
  uctxt_print (uctxt);

  nuxperf_foreach (_print_perfctr, NULL);

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
