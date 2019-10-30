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

#include <cdefs.h>
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include <nux/hal.h>
#include <nux/plt.h>

#include "i386.h"
#include "../internal.h"

static pfn_t pcpu_stackpfn[MAXCPUS];
static void *pcpu_stackva[MAXCPUS];
static int _enter_called = 0;

uint16_t
_i386_fs (void)
{
  if (_enter_called)
    return (5 + 4 * plt_pcpu_id() + 1) << 3;
  else
    return 0;
}

void
hal_pcpu_init (unsigned pcpuid, struct hal_cpu *pcpu)
{
  pfn_t pfn;
  void *va;
  void _set_tss (unsigned, void *);
  void _set_fs (unsigned, void *);

  assert (pcpuid < MAXCPUS);

  /* Allocate PCPU stack. Use KVA. */
  pfn = pfn_alloc (1);
  assert (pfn != PFN_INVALID);
  /* This is tricky. The hope is that is low enough to be addressed by 16 bit. */
  assert (pfn < (1 << 8) && "Can't allocate Memory below 1MB!");

  va = kva_map (1, pfn, 1, HAL_PTE_W|HAL_PTE_P);
  assert (va != NULL);

  pcpu_stackpfn[pcpuid] = pfn;
  pcpu_stackva[pcpuid] = va;

  pcpu->tss.esp0 = (unsigned long)pcpu_stackva[pcpuid] + 4096;
  pcpu->tss.ss0 = KDS;
  pcpu->tss.iomap = 108;
  pcpu->data = NULL;
  _set_tss (pcpuid, &pcpu->tss);
  _set_fs (pcpuid, &pcpu->data);
}

void
hal_pcpu_enter (unsigned pcpuid)
{
  uint16_t tss = (5 + 4 * pcpuid) << 3;
  uint16_t fs = (5 + 4 * pcpuid + 1) << 3;

  assert (pcpuid < MAXCPUS);

  asm volatile ("ltr %%ax"::"a" (tss));
  asm volatile ("mov %%ax, %%fs"::"a" (fs));

  _enter_called = 1;
}

uint64_t
hal_pcpu_prepare (unsigned pcpu)
{
  extern volatile long _ap_stackpage;
  extern char *_ap_start, *_ap_end;
  paddr_t pstart;
  void *start;
  volatile uint16_t *reset;

  if (pcpu >= MAXCPUS)
    return PADDR_INVALID;

  if (pcpu_stackva[pcpu] == NULL)
    return PADDR_INVALID;

  pstart = pcpu_stackpfn[pcpu] << PAGE_SHIFT;
  start = pcpu_stackva[pcpu];
  printf ("preparing %p (%lx)\n", pstart, start);

  reset = kva_physmap (0, 0x467, 2, HAL_PTE_P|HAL_PTE_W|HAL_PTE_X);
  *reset = pstart & 0xf;
  *(reset + 1) = pstart >> 4;
  kva_unmap ((void *)reset);

  /* Setup AP bootstrap page */
  memcpy (start, &_ap_start,
	  (size_t) ((void *) &_ap_end - (void *) &_ap_start));

  asm volatile ("":::"memory");

  /* Setup AP Stack. */
  void *ptr = start + ((void *)&_ap_stackpage - (void *)&_ap_start);
  *(void **)ptr = start + PAGE_SIZE;
  return pstart;
}

void
hal_cpu_setdata (void *data)
{
  asm volatile ("movl %0, %%fs:0\n"::"r" (data));
}

void *
hal_cpu_getdata (void)
{
  void *data;

  asm volatile ("movl %%fs:0, %0\n":"=r" (data));
  return data;
}

unsigned
hal_vect_ipibase (void)
{
  return VECT_IPI0;
}

unsigned
hal_vect_irqbase (void)
{
  return VECT_IRQ0;
}

unsigned
hal_vect_max (void)
{
  return 255;
}

void
i386_init (void)
{
}