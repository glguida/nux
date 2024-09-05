#include <cdefs.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <framebuffer.h>

#include <nux/hal.h>
#include <nux/apxh.h>
#include "internal.h"
#include "stree.h"

extern int _info_start;

extern int _stree_start[];
extern int _stree_end[];

extern int _fbuf_start;
extern int _fbuf_end;

extern int _memregs_start;
extern int _memregs_end;

extern uint64_t _riscv64_physmap_start;
extern uint64_t _riscv64_physmap_end;

extern uint64_t _riscv64_pfncache_start;
extern uint64_t _riscv64_pfncache_end;

extern uint64_t _riscv64_kva_start;
extern uint64_t _riscv64_kva_end;

extern uint64_t _riscv64_kmem_start;
extern uint64_t _riscv64_kmem_end;

const struct apxh_bootinfo *bootinfo = (struct apxh_bootinfo *) &_info_start;

struct fbdesc fbdesc;
struct apxh_pltdesc pltdesc;

void *hal_stree_ptr;
unsigned hal_stree_order;

int use_fb;
int nux_initialized = 0;

static void
early_print(const char *s)
{
  char *ptr = (char *)s;

  while (*ptr != '\0')
    hal_putchar (*ptr++);
}


/*
  I/O ops aren't implemented in RISC-V.
*/

unsigned long
hal_cpu_in (uint8_t size, uint32_t port)
{
  return 0;
}

void
hal_cpu_out (uint8_t size, uint32_t port, unsigned long val)
{
}

void
hal_cpu_relax (void)
{
  asm volatile ("nop;");
}

void
hal_cpu_trap (void)
{
  asm volatile ("ebreak;");
}

void
hal_cpu_idle (void)
{
  while (1)
    asm volatile ("csrsi sstatus, 0x2; wfi;");
}

void
hal_cpu_halt (void)
{
  while (1)
    asm volatile ("csrci sstatus, 0x2; 1: j 1b;");

}

void
hal_cpu_tlbop (hal_tlbop_t tlbop)
{
  if (tlbop == HAL_TLBOP_NONE)
    return;

  asm volatile ("sfence.vma x0, x0":::"memory");
}

vaddr_t
hal_virtmem_dmapbase (void)
{
  return _riscv64_physmap_start;
}

const size_t
hal_virtmem_dmapsize (void)
{
  return (size_t) (_riscv64_physmap_end - _riscv64_physmap_start);
}

vaddr_t
hal_virtmem_pfn$base (void)
{
  return _riscv64_pfncache_start;
}

const size_t
hal_virtmem_pfn$size (void)
{
  return (size_t) (_riscv64_pfncache_end - _riscv64_pfncache_start);
}

const vaddr_t
hal_virtmem_userbase (void)
{
  return umap_minaddr ();
}

const size_t
hal_virtmem_usersize (void)
{
  return umap_maxaddr ();
}

const vaddr_t
hal_virtmem_userentry (void)
{
  return (const vaddr_t) bootinfo->uentry;
}

unsigned long
hal_physmem_maxpfn (void)
{
  return (unsigned long) bootinfo->maxpfn;
}

unsigned
hal_physmem_numregions (void)
{

  return (unsigned) bootinfo->numregions;
}

struct apxh_region *
hal_physmem_region (unsigned i)
{
  struct apxh_region *ptr;

  if (i >= hal_physmem_numregions ())
    return NULL;

  ptr = (struct apxh_region *) &_memregs_start + i;
  assert (ptr < (struct apxh_region *) &_memregs_end);

  return ptr;
}

void *
hal_physmem_stree (unsigned *order)
{
  if (order)
    *order = hal_stree_order;
  return hal_stree_ptr;
}

vaddr_t
hal_virtmem_kvabase (void)
{
  return (vaddr_t) _riscv64_kva_start;
}

const size_t
hal_virtmem_kvasize (void)
{
  return (size_t) (_riscv64_kva_end - _riscv64_kva_start);
}

vaddr_t
hal_virtmem_kmembase (void)
{
  return (vaddr_t) _riscv64_kmem_start;
}

const size_t
hal_virtmem_kmemsize (void)
{
  return (size_t) (_riscv64_kmem_end - _riscv64_kmem_start);
}

const struct apxh_pltdesc *
hal_pltinfo (void)
{
  return &pltdesc;
}

void
riscv_init (void)
{
  size_t stree_memsize;
  struct apxh_stree *stree_hdr;

  if (bootinfo->magic != APXH_BOOTINFO_MAGIC)
    {
      /* Only way to let know that things are wrong. */
      hal_cpu_trap ();
    }

  fbdesc = bootinfo->fbdesc;
  fbdesc.addr = (uint64_t) (uintptr_t) & _fbuf_start;
  //  use_fb = framebuffer_init (&fbdesc);
  use_fb = 0;

  /* Check  APXH stree. */
  stree_hdr = (struct apxh_stree *) _stree_start;
  if (stree_hdr->magic != APXH_STREE_MAGIC)
    {
      early_print ("ERROR: Unrecognised stree magic!");
      hal_cpu_halt ();
    }
  if (stree_hdr->size != 8 * STREE_SIZE (stree_hdr->order))
    {
      early_print ("ERROR: stree size doesn't match!");
      hal_cpu_halt ();
    }
  stree_memsize = (size_t) ((void *) _stree_end - (void *) _stree_start);
  if (stree_hdr->size + stree_hdr->offset > stree_memsize)
    {
      early_print ("ERROR: stree doesn't fit in allocated memory!");
      hal_cpu_halt ();
    }
  hal_stree_order = stree_hdr->order;
  hal_stree_ptr = (uint8_t *) stree_hdr + stree_hdr->offset;

  pltdesc = bootinfo->pltdesc;

  early_print ("riscv64 HAL bootinf from APXH.\n");
}


int
hal_putchar (int ch)
{
  asm volatile ("mv a0, %0\n" "li a7, 1\n" "ecall\n"::"r" (ch):"a0", "a7");
  return ch;
}