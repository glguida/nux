#include <stdio.h>
//#include <nux/hal.h>

#include "internal.h"

static void
init_mem (void)
{

  /*
     Initialise Page Allocator.
   */
  pginit();

  /*
    Initialise KVA Allocator.
  */
  kvainit();

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

void __attribute__((constructor(0)))
_nux_sysinit (void)
{
  banner ();

  /* Initialise memory management */
  init_mem ();

#if 0
  /* Init CPUs operations */
  cpu_init ();

  /* Start the platform. This will discover CPUs and set up interrupt
     controllers. */
  plt_platform_start ();

  /* Start CPUs operations. */
  cpu_start ();
#endif
}

void
_nux_apinit (void)
{
#if 0
  mmap_enter ();
  cpu_enter ();
  exit (main_ap ());
#endif
}
