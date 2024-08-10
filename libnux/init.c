/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  See COPYING file for the full license.

  SPDX-License-Identifier:	GPL2.0+
*/

#include <stdio.h>
#include <nux/plt.h>
#include <nux/nux.h>

#include "internal.h"

volatile uint8_t _nux_stflags = 0;

uint8_t
nux_status (void)
{
  uint8_t st;
  __atomic_load (&_nux_stflags, &st, __ATOMIC_ACQUIRE);
  return st;
}

uint8_t
nux_status_setfl (uint8_t flags)
{
  return __atomic_fetch_or (&_nux_stflags, flags, __ATOMIC_ACQ_REL);
}


static void
init_mem (void)
{

  /*
     Initialise Page Allocator.
   */
  _pfncache_bootstrap ();
  pfninit ();

  /*
     Initialise KMEM.
   */
  kmeminit ();

  /*
    Initialise KVA Allocator.
   */
  kvainit ();



  pfncacheinit ();

#if 0
  /*
     Step 1: Initialise PFN Database.
   */
  fmap_init ();


  pginit ();


  /*
     Step 3: Enable heap.
   */
  heap_init ();


  /*
     Step 4: Initialise Slab Allocator. 
   */
  slab_init ();
#endif
}

#define PACKAGE "NUX library"
#define PACKAGE_NAME "nux"
#define VERSION "0.0"
#define COPYRIGHT_YEAR 2019

static void
banner (void)
{
  printf ("%s (%s) %s\n", PACKAGE, PACKAGE_NAME, VERSION);

  printf ("\
Copyright (C) %d Gianluca Guida\n\
\n\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or (at\n\
your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful, but\n\
WITHOUT ANY WARRANTY; see the GNU General Public License for more\n\
details.\n\n", COPYRIGHT_YEAR);
}

void klog_start (void);

void __attribute__((constructor (0))) _nux_sysinit (void)
{
  banner ();

  /* Initialise memory management */
  init_mem ();

  /* Start the platform. This will discover CPUs and set up interrupt
     controllers. */
  plt_init ();

  nux_status_setfl (NUXST_OKPLT);

  /* Init CPUs operations */
  cpu_init ();

  /* Now safe to use CPU operations. */
  nux_status_setfl (NUXST_OKCPU);

  /* Start all CPUs. */
  cpu_startall ();

  /* Signal HAL that we're done initialising. */
  hal_init_done ();

  nux_status_setfl (NUXST_RUNNING);
}

void
hal_main_ap (void)
{
  cpu_enter ();
  exit (main_ap ());

#if 0
  mmap_enter ();
  cpu_enter ();
  exit (main_ap ());
#endif
}
