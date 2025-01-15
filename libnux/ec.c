/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <stdio.h>
#include <stdlib.h>
#include <nux/hal.h>
#include <nux/nux.h>

#include "internal.h"

int
putchar (int c)
{
  hal_putchar (c);
  return c;
}

void __dead
nux_panic (const char *message, struct hal_frame *f)
{
  if (nux_status_setfl (NUXST_PANIC) & NUXST_PANIC)
    {
      /* System was already in panic. Just halt the CPU. */
      hal_cpu_halt ();
      /* Unreachable. */
    }

  /* STOP all CPUs except this one. */
  cpu_nmi_allbutself ();

  hal_panic (cpu_try_id (), message, f);
}

void __dead
abort (void)
{
  while (1)
    hal_debug ();
}

void __dead
exit (int status)
{
  if (status == EXIT_HALT)
    {
      hal_cpu_halt ();
    }
  else if (status == EXIT_IDLE)
    {
      cpu_idle ();
    }


  abort ();
}
