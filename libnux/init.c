/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <stdio.h>
#include <nux/plt.h>
#include <nux/nux.h>

#include "internal.h"

/* Set and cleared by HAL. If this is on, we're still in initialisation mode. */
volatile uint32_t _nux_apbooting = 0;

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

bool
nux_status_okcpu (void)
{
  if (__predict_false (!(nux_status () & NUXST_OKCPU))
      || __predict_false (_nux_apbooting))
    return false;
  else
    return true;

}


static void
init_mem (void)
{

  /*
     Initialise Page Allocator.
   */
  _pfncache_bootstrap ();
  stree_pfninit ();

  /*
     Initialise KMEM.
   */
  kmeminit ();

  /*
     Initialise KVA Allocator.
   */
  kvainit ();


  /*
     Initialise PFN Cache.
   */
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
  printf ("Copyright (C) %d Gianluca Guida\n\n", COPYRIGHT_YEAR);
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

  printf ("Waiting for APs to boot..");
  while (_nux_apbooting)
    hal_cpu_relax ();
  printf ("done.\n");

  /* Signal HAL that we're done initialising. */
  hal_init_done ();

  nux_status_setfl (NUXST_RUNNING);
}

void
hal_main_ap (void)
{
  cpu_enter ();
  __atomic_sub_fetch (&_nux_apbooting, 1, __ATOMIC_ACQ_REL);
  exit (main_ap ());
}

#include <nux/nuxperf.h>
#undef NUXPERF
#undef NUXPERF_DECLARE
#define NUXPERF_DEFINE
#include "perf.h"
