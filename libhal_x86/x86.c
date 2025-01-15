/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <cdefs.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <framebuffer.h>

#include <nux/hal.h>
#include <nux/apxh.h>
#include <nux/symbol.h>

#include "internal.h"
#include "stree.h"


extern int _info_start;

extern int _physmap_start;
extern int _physmap_end;

extern int _pfncache_start;
extern int _pfncache_end;

extern int _kva_start;
extern int _kva_end;

extern int _kmem_start;
extern int _kmem_end;

extern int _stree_start[];
extern int _stree_end[];

extern int _fbuf_start;
extern int _fbuf_end;

extern int _memregs_start;
extern int _memregs_end;

/*
  Pin areas of memory to a fixed memory region type.
*/
struct apxh_region _memregs_pinned[] = {
  /*
     Remove me after getting ACPI pointer from kernel .
   */
  {.type = APXH_REGION_MMIO,.pfn = 0,.len = 1,},
  /*

     Mark the whole 0xA0000-0x100000 area as MMIO. */
  {.type = APXH_REGION_MMIO,.pfn = 0xa0,.len = 96,},
};

#define PINNED_MEMREGS (sizeof(_memregs_pinned)/sizeof(struct apxh_region))

const struct apxh_bootinfo *bootinfo = (struct apxh_bootinfo *) &_info_start;

struct fbdesc fbdesc;
struct apxh_pltdesc pltdesc;

void *hal_stree_ptr;
unsigned hal_stree_order;

int use_fb;
int nux_initialized = 0;

static inline __dead void
__halt (void)
{
  while (1)
    asm volatile ("cli; hlt");
}

uint64_t
rdmsr (uint32_t ecx)
{
  uint32_t edx, eax;


  asm volatile ("rdmsr\n":"=d" (edx), "=a" (eax):"c" (ecx));

  return ((uint64_t) edx << 32) | eax;
}

void
wrmsr (uint32_t ecx, uint64_t val)
{
  uint32_t edx, eax;

  eax = (uint32_t) val;
  edx = (uint32_t) (val >> 32);

  asm volatile ("wrmsr\n"::"a" (eax), "d" (edx), "c" (ecx));
}

unsigned long
read_cr4 (void)
{
  unsigned long r;

  asm volatile ("mov %%cr4, %0\n":"=r" (r));

  return r;
}

void
write_cr4 (unsigned long r)
{
  asm volatile ("mov %0, %%cr4\n"::"r" (r));
}

unsigned long
read_cr3 (void)
{
  unsigned long r;

  asm volatile ("mov %%cr3, %0\n":"=r" (r));

  return r;
}

void
write_cr3 (unsigned long r)
{
  asm volatile ("mov %0, %%cr3\n"::"r" (r));
}

int
inb (int port)
{
  int ret;

  asm volatile ("xor %%eax, %%eax; inb %%dx, %%al":"=a" (ret):"d" (port));
  return ret;
}

int
inw (unsigned port)
{
  int ret;

  asm volatile ("xor %%eax, %%eax; inw %%dx, %%ax":"=a" (ret):"d" (port));
  return ret;
}

int
inl (unsigned port)
{
  int ret;

  asm volatile ("inl %%dx, %%eax":"=a" (ret):"d" (port));
  return ret;
}

void
outb (int port, int val)
{
  asm volatile ("outb %%al, %%dx"::"d" (port), "a" (val));
}

void
outw (unsigned port, int val)
{
  asm volatile ("outw %%ax, %%dx"::"d" (port), "a" (val));
}

void
outl (unsigned port, int val)
{
  asm volatile ("outl %%eax, %%dx"::"d" (port), "a" (val));
}

void
tlbflush_global (void)
{
  unsigned long r;

  r = read_cr4 ();
  write_cr4 (r ^ (1 << 7));
  write_cr4 (r);
}

void
tlbflush_local (void)
{
  unsigned long r;

  r = read_cr3 ();
  write_cr3 (r);

  asm volatile ("":::"memory");
}


int
hal_putchar (int c)
{

  if (use_fb)
    framebuffer_putc (c, 0xe0e0e0);
  else
    vga_putchar (c);

  /* Always use serial. */
  serial_putchar (c);

  return c;
}

unsigned long
hal_cpu_in (uint8_t size, uint32_t port)
{
  unsigned long val;

  switch (size)
    {
    case 1:
      val = inb (port);
      break;
    case 2:
      val = inw (port);
      break;
    case 4:
      val = inl (port);
      break;
    default:
      //      halwarn ("Invalid I/O port size %d", size);
      val = (unsigned long) -1;
    }

  return val;
}

void
hal_cpu_out (uint8_t size, uint32_t port, unsigned long val)
{
  switch (size)
    {
    case 1:
      outb (port, val);
      break;
    case 2:
      outw (port, val);
      break;
    case 4:
      outl (port, val);
      break;
    default:
      //      halwarn ("Invalid I/O port size %d", size);
      break;
    }
}

void
hal_cpu_relax (void)
{
  asm volatile ("pause");
}

void
hal_cpu_trap (void)
{
  asm volatile ("ud2");
}

uint64_t
hal_cpu_cycles (void)
{
  uint32_t hi, lo;
  asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

void __dead
hal_cpu_idle (void)
{
  while (1)
    {
      asm volatile ("sti; hlt");
    }
}

__dead void
hal_cpu_halt (void)
{
  __halt ();
}

void
hal_cpu_tlbop (hal_tlbop_t tlbop)
{
  if (tlbop == HAL_TLBOP_NONE)
    return;

  if (tlbop == HAL_TLBOP_FLUSHALL)
    tlbflush_global ();
  else
    tlbflush_local ();
}

void
hal_useraccess_start (void)
{
  /* TODO: SMEP */
}

void
hal_useraccess_end (void)
{
  /* TODO: SMEP */
}

vaddr_t
hal_virtmem_dmapbase (void)
{
  return (uint64_t) (uintptr_t) & _physmap_start;
}

size_t
hal_virtmem_dmapsize (void)
{
  return (size_t) ((void *) &_physmap_end - (void *) &_physmap_start);
}

vaddr_t
hal_virtmem_pfn$base (void)
{
  return (uint64_t) (uintptr_t) & _pfncache_start;
}

size_t
hal_virtmem_pfn$size (void)
{
  return (size_t) ((void *) &_pfncache_end - (void *) &_pfncache_start);
}

vaddr_t
hal_virtmem_userbase (void)
{
  return pt_umap_minaddr ();
}

size_t
hal_virtmem_usersize (void)
{
  return pt_umap_maxaddr ();
}

vaddr_t
hal_virtmem_userentry (void)
{
  return (vaddr_t) bootinfo->uentry;
}

unsigned long
hal_physmem_maxpfn (void)
{
  return (unsigned long) bootinfo->maxpfn;
}

unsigned long
hal_physmem_maxrampfn (void)
{
  return (unsigned long) bootinfo->maxrampfn;
}

unsigned
hal_physmem_numregions (void)
{

  return (unsigned) bootinfo->numregions + PINNED_MEMREGS;
}

struct apxh_region *
hal_physmem_region (unsigned i)
{
  struct apxh_region *ptr;

  if (i >= hal_physmem_numregions ())
    return NULL;

  if (i < (unsigned) bootinfo->numregions)
    {
      ptr = (struct apxh_region *) &_memregs_start + i;
      assert (ptr < (struct apxh_region *) &_memregs_end);
    }
  else
    {
      ptr = _memregs_pinned + i - bootinfo->numregions;
    }

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
  return (vaddr_t) & _kva_start;
}

size_t
hal_virtmem_kvasize (void)
{
  return (size_t) ((void *) &_kva_end - (void *) &_kva_start);
}

vaddr_t
hal_virtmem_kmembase (void)
{
  return (vaddr_t) & _kmem_start;
}

size_t
hal_virtmem_kmemsize (void)
{
  return (size_t) ((void *) &_kmem_end - (void *) &_kmem_start);
}

static void
early_print (const char *str)
{
  size_t i;
  size_t len = strlen (str);
  for (i = 0; i < len; i++)
    hal_putchar (str[i]);
}

const struct apxh_pltdesc *
hal_pltinfo (void)
{
  return &pltdesc;
}

void
x86_init (void)
{
  size_t stree_memsize;
  struct apxh_stree *stree_hdr;

  if (bootinfo->magic != APXH_BOOTINFO_MAGIC)
    {
      /* Only way to let know that things are wrong. */
      hal_cpu_trap ();
    }

  serial_init ();

  fbdesc = bootinfo->fbdesc;
  fbdesc.addr = (uint64_t) (uintptr_t) & _fbuf_start;
  use_fb = framebuffer_init (&fbdesc);

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

  /* Do not allow allocation in non-RAM pinned memory regions. */
  for (int i = 0; i < PINNED_MEMREGS; i++)
    {
      struct apxh_region *r = _memregs_pinned + i;
      if (r->type != APXH_REGION_RAM)
	for (int j = 0; j < r->len; j++)
	  stree_clrbit (hal_stree_ptr, hal_stree_order, r->pfn + j);
    }

  pltdesc = bootinfo->pltdesc;

  pmap_init ();

#ifdef __i386__
  early_print ("i386 HAL booting from APXH.\n");
#endif
#ifdef __amd64__
  early_print ("AMD64 HAL booting from APXH.\n");
  amd64_init ();
#endif
}

void
hal_init_done (void)
{
#ifdef __i386__
  i386_init_done ();
#endif
#ifdef __amd64__
  amd64_init_done ();
#endif
  nux_initialized = 1;
}

struct stackframe {
  struct stackframe *rbp;
  unsigned long ra;
};

void
stackframe (unsigned long rbp)
{
  struct stackframe *sf = (struct stackframe *)rbp;
  unsigned i = 1;

  while (sf != NULL && i < 32 && ((unsigned long)sf % (sizeof(void *)) == 0))
    {
      printf ("    [%d]: %lx <%s>\n", i, sf->ra, nux_symresolve(sf->ra));

      if (sf->rbp <= sf)
	break;
      sf = sf->rbp;
      i++;
    }
}

__dead void
hal_panic (unsigned cpu, const char *error, struct hal_frame *f)
{
  if (use_fb)
    {
      /*
         Reset frame buffer. This will unlock in case any CPU was
         holding the spinlock.
       */
      framebuffer_reset ();
    }

  printf ("\n"
	  "----------------------------------------"
	  "---------------------------------------\n"
	  "Fatal error on CPU%d: %s\n", cpu, error);
  if (f != NULL)
    {
      hal_frame_print (f);
    }
  printf ("\n");
  printf ("Stack Trace:\n\n");
  printf ("    [0]: %lx <%s>\n", hal_frame_getip(f), nux_symresolve(hal_frame_getip(f)));
  stackframe (frame_bp(f));
  printf ("\n");
  printf ("PTE Walk for CR2 [%lx]\n", frame_cr2(f));
  printf ("\n");
  pt_umap_debugwalk (NULL, frame_cr2(f));
  printf ("\n");
  printf ("----------------------------------------"
	  "---------------------------------------\n");

  __halt ();
}
