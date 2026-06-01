/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
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

paddr_t pcpu_pstart;
vaddr_t pcpu_haldata[MAXCPUS];

/*
  CPU kernel stack allocation:

   CPUs at boot allocate a stack by incrementing (with a spinlock)
   'kstackno' and using the page pointed at 'pcpu_kstack'.

   Importantly, pcpu_kstack[pcpu_id] doesn't mean is the stack of PCPU ID.
*/
uint64_t pcpu_kstackno = 0;
uint64_t pcpu_kstackcnt = 0;
uint64_t pcpu_kstack[MAXCPUS];

static int bsp_enter_called = 0;
static unsigned bsp_pcpuid;

static vaddr_t smp_oldva;
static hal_l1e_t smp_oldl1e;

uint16_t
_i386_fs (void)
{
  if (bsp_enter_called)
    return (5 + 4 * plt_pcpu_id () + 1) << 3;
  else
    return 0;
}

void
hal_pcpu_init (void)
{
  pfn_t pfn;
  void *start, *ptr;
  hal_l1p_t l1p;
  paddr_t pstart;
  volatile uint16_t *reset;
  extern char *_ap_start, *_ap_end;

  /* Allocate PCPU bootstrap code. Use KVA. *//* TODO: USE KVA? Not needed, not a long term mapping. */
  pfn = pfn_alloc (1);
  assert (pfn != PFN_INVALID);
  /* This is tricky. The hope is that is low enough to be addressed by 16 bit. */
  assert (pfn < (1 << 8) && "Can't allocate Memory below 1MB!");

  /* Map and prepare the bootstrap code page. */
  start = pfn_get (pfn);
  size_t apbootsz = (size_t) ((void *) &_ap_end - (void *) &_ap_start);
  assert (apbootsz <= PAGE_SIZE);
  memcpy (start, &_ap_start, apbootsz);
  pstart = (paddr_t) pfn << PAGE_SHIFT;

  /*
     The following is trampoline dependent code, and configures the
     trampoline to use the page just selected as bootstrap page.
   */
  extern char _ap_gdtreg, _ap_ljmp, _ap_cr3;
  extern uint32_t _bsp_cr3;

  /* Copy BSP CR3 into AP */
  ptr = start + ((void *) &_ap_cr3 - (void *) &_ap_start);
  *(uint32_t *) ptr = _bsp_cr3;

  /* Setup temporary GDT register. */
  ptr = start + ((void *) &_ap_gdtreg - (void *) &_ap_start);
  *(uint32_t *) (ptr + 2) += (uint32_t) pstart;

  /* Setup trampoline 1 */
  ptr = start + ((void *) &_ap_ljmp - (void *) &_ap_start);
  *(uint32_t *) ptr += (uint32_t) pstart;

  pfn_put (pfn, start);

  /* Set reset vector through a supervisor KVA mapping, not a user PTE. */
  reset = kva_physmap (0x467, 2, HAL_PTE_P | HAL_PTE_W | HAL_PTE_X);
  *reset = pstart & 0xf;
  *(reset + 1) = pstart >> 4;
  kva_unmap ((void *) reset, 2);

  /*
     pstart is below pt_umap_maxaddr(), so use UMAP page-table machinery to
     reach the temporary AP trampoline leaf.  The executable trampoline mapping
     itself must stay supervisor-only for SMEP: no PTE_U is set here.
   */
  l1p = umap_get_l1p (NULL, pstart, true);
  assert (l1p != L1P_INVALID);
  /* Save the l1e we're abou to overwrite. We'll restore it after init is done. */
  smp_oldl1e = hal_l1e_get (l1p);
  smp_oldva = pstart;
  hal_l1e_set (l1p, (pstart & ~PAGE_MASK) | PTE_P | PTE_W);
  pcpu_pstart = pstart;

  bsp_pcpuid = plt_pcpu_id ();
}

void
hal_pcpu_add (unsigned pcpuid, struct hal_cpu *haldata)
{
  pfn_t pfn;
  void *va;
  void _set_tss (unsigned, void *);
  void _set_fs (unsigned, void *);

  assert (pcpuid < MAXCPUS);

  if (pcpuid == bsp_pcpuid)
    {
      /* Adding the BSP PCPU: Initialize TSS */
      extern char _bsp_stacktop;
      haldata->tss.ss0 = KDS;
      haldata->tss.esp0 = (uintptr_t) & _bsp_stacktop;
      haldata->tss.iomap = 108;
    }
  else
    {
      /* Adding secondary CPU: Allocate one PCPU kernel stack. */
      pfn = pfn_alloc (1);
      assert (pfn != PFN_INVALID);
      va = kva_map (pfn, HAL_PTE_W | HAL_PTE_P);
      assert (va != NULL);
      pcpu_kstack[pcpu_kstackno++] = (uint64_t) (uintptr_t) va + PAGE_SIZE;
    }
  _set_tss (pcpuid, &haldata->tss);
  _set_fs (pcpuid, &haldata->data);

  pcpu_haldata[pcpuid] = (vaddr_t) (uintptr_t) haldata;
}

uint64_t
hal_pcpu_startaddr (unsigned pcpu)
{
  if (pcpu >= MAXCPUS)
    return PADDR_INVALID;

  return pcpu_pstart;
}

void
hal_pcpu_enter (unsigned pcpuid)
{
  uint16_t tss = (5 + 4 * pcpuid) << 3;
  uint16_t fs = (5 + 4 * pcpuid + 1) << 3;

  assert (pcpuid < MAXCPUS);

  asm volatile ("ltr %%ax"::"a" (tss));
  asm volatile ("mov %%ax, %%fs"::"a" (fs));

  x86_supervisor_hardening_init ();

  bsp_enter_called = 1;
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
hal_vect_max (void)
{
  return 255;
}

void
i386_init_ap (uintptr_t esp)
{
  unsigned pcpu = plt_pcpu_id ();
  struct hal_cpu *haldata = (struct hal_cpu *) (uintptr_t) pcpu_haldata[pcpu];

  haldata->tss.ss0 = KDS;
  haldata->tss.esp0 = esp;
  haldata->tss.iomap = 108;

  pae32_init_ap ();

  hal_main_ap ();
}

static void
remove_bootmappings (void)
{
  hal_l1p_t l1p;

  /* Restore the mapping created for boostrapping secondary CPUS. */
  l1p = umap_get_l1p (NULL, smp_oldva, false);
  assert (l1p != L1P_INVALID);
  hal_l1e_set (l1p, smp_oldl1e);
}

void
i386_init_done (void)
{

  remove_bootmappings ();
}
