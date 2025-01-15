/*
  APXH: An ELF boot-loader.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include "project.h"

static arch_t elf_arch;
static uint8_t boot_pagemap[PAGEMAP_SZ (BOOTMEM)]
  __attribute__((aligned (4096)));
static vaddr_t req_pfnmap_va, req_info_va, req_stree_va, req_region_va,
  ktls_va, utls_va;
static size64_t req_pfnmap_size, req_info_size, req_stree_size,
  req_region_size, ktls_initsize, ktls_size, utls_initsize, utls_size;
static unsigned req_stree_order, req_region_num;
static bool stop_payload_allocation = false;
static uint64_t minramaddr = 0;


uintptr_t
get_payload_page (void)
{
  unsigned pfn;
  uintptr_t page;
  uintptr_t base = minramaddr;

  assert (!stop_payload_allocation);

  page = get_page ();
  assert (page - base < BOOTMEM);
  memset ((void *) page, 0, PAGE_SIZE);

  pfn = page >> PAGE_SHIFT;

  boot_pagemap[(page - base) >> (PAGE_SHIFT + 3)] |= 1 << (pfn & 7);

  return page;
}

unsigned
check_payload_page (unsigned addr)
{
  unsigned i = (addr - minramaddr) >> PAGE_SHIFT;
  unsigned by = i >> 3;
  unsigned bi = (1 << (i & 7));

  assert (by <= PAGEMAP_SZ (BOOTMEM));

  return !!(boot_pagemap[by] & bi);
}

void
init (void)
{
  md_init ();
  minramaddr = md_minrampfn () << PAGE_SHIFT;
}

const char *
get_arch_name (arch_t arch)
{
  switch (arch)
    {
    case ARCH_INVALID:
      return "invalid";
    case ARCH_UNSUPPORTED:
      return "unsupported";
    case ARCH_386:
      return "i386";
    case ARCH_AMD64:
      return "AMD64";
    case ARCH_RISCV64:
      return "RISCV64";
    default:
      return "unknown";
    }
}

void
va_init (void)
{
  switch (elf_arch)
    {
#if (EC_MACHINE_I386) || (EC_MACHINE_AMD64)
    case ARCH_386:
      pae_init ();
      break;
    case ARCH_AMD64:
      pae64_init ();
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_init ();
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_populate (vaddr_t va, size64_t size, int u, int w, int x)
{
  md_verify (va, size);
  va_verify (va, size);

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_populate (va, size, u, w, x);
      break;
    case ARCH_AMD64:
      pae64_populate (va, size, u, w, x);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_populate (va, size, u, w, x);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_memcpy (vaddr_t va, void *addr, size64_t size, int u, int w, int x)
{
  ssize64_t len = size;

  md_verify (va, size);
  va_verify (va, size);

#if 0
  printf ("Copying %08llx <- %p (u: %d, w:%d, x:%d, %d bytes)\n", va, addr, u,
	  w, x, size);
#endif
  va_populate (va, size, u, w, x);

  while (len > 0)
    {
      uintptr_t paddr;
      size64_t clen = PAGE_CEILING (va) - va;

      if (clen > len)
	clen = len;

      paddr = va_getphys (va);

      memcpy ((void *) paddr, addr, clen);

      len -= clen;
      va += clen;
      addr += clen;
    }
}

void
va_memset (vaddr_t va, int c, size64_t size, int u, int w, int x)
{
  ssize64_t len = size;

  md_verify (va, size);
  va_verify (va, size);

  printf ("Setting %08llx <- %d (u:%d, w:%d, x: %d, %d bytes)\n", va, c, u, w,
	  x, size);
  va_populate (va, size, u, w, x);

  while (len > 0)
    {
      uintptr_t paddr;
      size64_t clen = PAGE_CEILING (va) - va;

      paddr = va_getphys (va);

      memset ((void *) paddr, 0, clen);

      len -= clen;
      va += clen;
    }
}

void
va_physmap (vaddr_t va, size64_t size, enum memory_type mt)
{
  md_verify (va, size);
  va_verify (va, size);

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_physmap (va, size, 0, mt);
      break;
    case ARCH_AMD64:
      pae64_physmap (va, size, 0, mt);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_physmap (va, size, 0, mt);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_framebuf (vaddr_t va, size64_t size, enum memory_type mt)
{
  uint64_t pa;
  struct fbdesc *fbptr;

  md_verify (va, size);
  va_verify (va, size);

  fbptr = md_getframebuffer ();
  if (fbptr == NULL || fbptr->type == FB_INVALID)
    return;

  if (fbptr->size > size)
    {
      printf ("ERROR: framebuffer too big. Shrinking int from %lx to %lx\n",
	      fbptr->size, size);
      fbptr->size = size;
    }

  pa = fbptr->addr;

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_physmap (va, size, pa, mt);
      break;
    case ARCH_AMD64:
      pae64_physmap (va, size, pa, mt);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_physmap (va, size, pa, mt);
      break;
#endif
    default:
      (void) pa;
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_linear (vaddr_t va, size64_t size)
{
  md_verify (va, size);
  va_verify (va, size);

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_linear (va, size);
      break;
    case ARCH_AMD64:
      pae64_linear (va, size);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_linear (va, size);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_topptalloc (vaddr_t va, size64_t size)
{
  md_verify (va, size);
  va_verify (va, size);

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_topptalloc (va, size);
      break;
    case ARCH_AMD64:
      pae64_topptalloc (va, size);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_topptalloc (va, size);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_ptalloc (vaddr_t va, size64_t size)
{
  md_verify (va, size);
  va_verify (va, size);

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_ptalloc (va, size);
      break;
    case ARCH_AMD64:
      pae64_ptalloc (va, size);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_ptalloc (va, size);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_info (vaddr_t va, size64_t size)
{
  md_verify (va, size);
  va_verify (va, size);

  va_populate (va, size, 0, 0, 0);

  /* Only save the va and size, we'll have to finish all allocations
     before we can return the proper data. */
  req_info_va = va;
  req_info_size = size;
}

static void
va_info_copy (uint64_t uentry, uint64_t num_regions)
{
  vaddr_t va = req_info_va;
  size64_t size = req_info_size;
#define MIN(x,y) ((x < y) ? x : y)
  struct apxh_bootinfo i;
  struct fbdesc *fbptr;
  struct apxh_pltdesc *pltdesc;

  if (va == 0)
    {
      /* No INFO. Skip. */
      return;
    }

  i.magic = APXH_BOOTINFO_MAGIC;
  i.maxpfn = md_maxpfn ();
  i.maxrampfn = md_maxrampfn ();
  i.numregions = num_regions;
  i.uentry = uentry;

  pltdesc = md_getpltdesc ();
  if (pltdesc != NULL)
    i.pltdesc = *pltdesc;
  else
    i.pltdesc = (struct apxh_pltdesc)
    {.type = PLT_UNKNOWN,.pltptr = 0 };

  fbptr = md_getframebuffer ();
  if (fbptr != NULL)
    i.fbdesc = *fbptr;
  else
    i.fbdesc.type = FB_INVALID;

  i.ktls.initvaddr = ktls_va;
  i.ktls.initsize = ktls_initsize;
  i.ktls.size = ktls_size;

  i.utls.initvaddr = utls_va;
  i.utls.initsize = utls_initsize;
  i.utls.size = utls_size;

  va_memcpy (va, &i, MIN (size, sizeof (struct apxh_bootinfo)), 0, 0, 0);
#undef MIN
}


#define OR_WORD(p, x) ((*(uint64_t *)va_getphys(req_stree_va + (vaddr_t)(uintptr_t)(p))) |= (x))
#define MASK_WORD(p,x) ((*(uint64_t *)va_getphys(req_stree_va + (vaddr_t)(uintptr_t)(p))) &= (x))
#define GET_WORD(p) (*(uint64_t *)va_getphys(req_stree_va + (vaddr_t)(uintptr_t)(p)))
#define SET_WORD(p,x) (*(uint64_t *)va_getphys(req_stree_va + (vaddr_t)(uintptr_t)(p)) = x)
#include <stree.h>

void
va_stree (vaddr_t va, size64_t size)
{
  size64_t s;
  int i, order;
  struct apxh_stree hdr;
  struct bootinfo_region *reg;
  unsigned regions = md_memregions ();
  unsigned maxframe = md_maxrampfn ();

  md_verify (va, size);
  va_verify (va, size);

  order = stree_order (maxframe);
  s = 8 * STREE_SIZE (order);
  s += sizeof (struct apxh_stree);

  if (s > size)
    {
      printf ("Can't create PFN S-Tree of order %d: "
	      "required %d bytes, %d available.\n", order, s, size);
    }

  size = s;
  printf ("Populating size %d (order: %d)\n", size, order);
  va_populate (va, size, 0, 1, 0);

  /* Copy the header. */
  hdr.magic = APXH_STREE_MAGIC;
  hdr.version = APXH_STREE_VERSION;
  hdr.order = order;
  hdr.offset = sizeof (hdr);
  hdr.size = 8 * STREE_SIZE (order);
  va_memcpy (va, &hdr, sizeof (hdr), 0, 1, 0);

  /* Fill the S-Tree with all RAM regions. */
  req_stree_va = va + sizeof (hdr);

  for (i = 0; i < regions; i++)
    {
      unsigned j;

      reg = md_getmemregion (i);

      if (reg->type != BOOTINFO_REGION_RAM)
	continue;


      for (j = 0; j < reg->len; j++)
	{
	  unsigned frame = reg->pfn + j;

	  if (frame > maxframe)
	    {
	      printf ("Maximum reached.\n");
	      break;
	    }

	  stree_setbit ((WORD_T *) 0, order, frame);
	}
    }

  /* Clear in case of overlapping non-ram regions. */
  for (i = 0; i < regions; i++)
    {
      unsigned j;

      reg = md_getmemregion (i);

      if (reg->type == BOOTINFO_REGION_RAM)
	continue;


      for (j = 0; j < reg->len; j++)
	{
	  unsigned frame = reg->pfn + j;

	  if (frame > maxframe)
	    {
	      printf ("Maximum reached.\n");
	      break;
	    }

	  stree_clrbit ((WORD_T *) 0, order, frame);
	}
    }


  /* We'll need to continue to update allocated pages. */
  req_stree_order = order;
  req_stree_size = size;
}

void
va_stree_copy (void)
{
  vaddr_t va = req_stree_va;
  unsigned order = req_stree_order;
  uint64_t pa;
  vaddr_t maxframe;

  if (va == 0)
    {
      /* No STREE. Skip. */
      return;
    }

  maxframe = md_maxrampfn ();

  for (pa = minramaddr; pa < BOOTMEM + minramaddr; pa += PAGE_SIZE)
    {
      unsigned frame = pa >> PAGE_SHIFT;

      if (frame > maxframe)
	break;

      if (check_payload_page (pa))
	{
	  /* Page is allocated. Mark as BSY. */
	  stree_clrbit ((WORD_T *) 0, order, frame);
	}
    }
}

void
va_regions (vaddr_t va, size64_t size)
{
  unsigned maxregion;
  unsigned regions = md_memregions ();

  md_verify (va, size);
  va_verify (va, size);

  maxregion = size / sizeof (struct apxh_region);

  if (maxregion > regions)
    maxregion = regions;

  size = regions * sizeof (struct apxh_region);

  printf ("Size of area: %lld = %ld * %d\n", size, regions,
	  sizeof (struct apxh_region));
  va_populate (va, size, 0, 0, 0);

  req_region_va = va;
  req_region_size = size;
  req_region_num = maxregion;
}

static void
va_regions_copy (void)
{
  vaddr_t va = req_region_va;
  unsigned long size = req_region_size;
  unsigned i, regions;
  struct apxh_region apxhreg;
  struct bootinfo_region *reg;

  if (va == 0)
    {
      /* No REGIONS. Skip. */
      return;
    }

  regions = size / sizeof (struct apxh_region);

  for (i = 0; i < regions; i++)
    {
      reg = md_getmemregion (i);
      apxhreg.type = reg->type;
      apxhreg.pfn = reg->pfn;
      apxhreg.len = reg->len;
#if 0
      printf ("Copying %d %d %d\n", apxhreg.type, apxhreg.pfn, apxhreg.len);
#endif
      va_memcpy (va + i * sizeof (struct apxh_region), &apxhreg,
	       sizeof (struct apxh_region), 0, 0, 0);
    }
}

void
va_pfnmap (vaddr_t va, size64_t size)
{
  unsigned i, maxframe;
  struct bootinfo_region *reg;
  unsigned regions = md_memregions ();

  md_verify (va, size);
  va_verify (va, size);

  maxframe = size / PFNMAP_ENTRY_SIZE;

  if (maxframe > md_maxpfn ())
    {
      maxframe = md_maxpfn ();
      size = maxframe * PFNMAP_ENTRY_SIZE;
    }

  va_populate (va, size, 0, 1, 0);

  for (i = 0; i < regions; i++)
    {
      unsigned j;

      reg = md_getmemregion (i);

      printf ("Reg: %d Type %02d, PA: %016llx (%ld)\n", i, reg->type,
	      (uint64_t) reg->pfn << PAGE_SHIFT, reg->len);


      for (j = 0; j < reg->len; j++)
	{
	  unsigned frame = reg->pfn + j;
	  uint8_t *ptr;

	  if (frame > maxframe)
	    {
	      printf ("Maximum reached.\n");
	      break;
	    }

	  ptr = (uint8_t *) va_getphys (va + frame * PFNMAP_ENTRY_SIZE);
	  assert (ptr != NULL);

	  /* There's  a  priority  in  numbering of  regions.  RAM  is
	     lowest, overwritten most easily. */
	  if (*ptr < reg->type)
	    *ptr = reg->type;
	}
    }

  req_pfnmap_va = va;
  req_pfnmap_size = size;
}

static void
va_pfnmap_copy (void)
{
  vaddr_t va = req_pfnmap_va;
  unsigned long size = req_pfnmap_size;
  unsigned maxframe = size / PFNMAP_ENTRY_SIZE;
#define MIN(x,y) ((x < y) ? x : y)
  unsigned long pa;

  if (va == 0)
    {
      /* No PFNMAP. Skip. */
      return;
    }

  for (pa = minramaddr; pa < BOOTMEM + minramaddr; pa += PAGE_SIZE)
    {
      unsigned frame = pa >> PAGE_SHIFT;

      if (frame > maxframe)
	break;

      if (check_payload_page (pa))
	{
	  /* Page is allocated. Mark as BSY. */

	  uint8_t *ptr =
	    (uint8_t *) va_getphys (va + frame * PFNMAP_ENTRY_SIZE);
	  assert (ptr != NULL);

	  *ptr = BOOTINFO_REGION_BSY;
	}
    }
#undef MIN
}

void
va_ktls (vaddr_t va, size64_t initsize, size64_t size)
{
  ktls_va = va;
  ktls_initsize = initsize;
  ktls_size = size;
}

void
va_utls (vaddr_t va, size64_t initsize, size64_t size)
{
  utls_va = va;
  utls_initsize = initsize;
  utls_size = size;
}

void
va_verify (vaddr_t va, size64_t size)
{
  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_verify (va, size);
      break;
    case ARCH_AMD64:
      pae64_verify (va, size);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_verify (va, size);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

uintptr_t
va_getphys (vaddr_t va)
{
  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      return pae_getphys (va);
      break;
    case ARCH_AMD64:
      return pae64_getphys (va);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      return sv48_getphys (va);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }
}

void
va_entry (vaddr_t entry)
{
  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      pae_entry (entry);
      break;
    case ARCH_AMD64:
      pae64_entry (entry);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      sv48_entry (entry);
      break;
#endif
    default:
      printf ("Unsupported VM architecture.\n");
      exit (-1);
    }

  printf ("Returned from entry!");
  exit (-1);
}

int
main (int argc, char *argv[])
{
  void *elf_start;
  size64_t elf_size;
  uint64_t kentry, uentry;

  printf ("\nAPXH started.\n\n");

  init ();

  /*
     Load kernel.
   */
  elf_start = get_payload_start (argc, argv, PAYLOAD_KERNEL);
  elf_size = get_payload_size (PAYLOAD_KERNEL);
  elf_arch = get_elf_arch (elf_start);
  printf ("Kernel payload %s ELF at addr %p (%d bytes)\n",
	  get_arch_name (elf_arch), elf_start, elf_size);

  va_init ();

  switch (elf_arch)
    {
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
    case ARCH_386:
      kentry = load_elf32 (elf_start, 0);
      break;
    case ARCH_AMD64:
      kentry = load_elf64 (elf_start, 0);
      break;
#endif
#if EC_MACHINE_RISCV64
    case ARCH_RISCV64:
      kentry = load_elf64 (elf_start, 0);
      break;
#endif
    default:
      printf ("Unsupported ELF architecture");
      exit (-1);
    }

  printf ("Kernel entry: %llx\n", kentry);

  /*
     Load user if it exists.
   */
  elf_start = get_payload_start (argc, argv, PAYLOAD_USER);
  elf_size = get_payload_size (PAYLOAD_USER);
  if (elf_start != NULL && elf_size != 0)
    {
      elf_arch = get_elf_arch (elf_start);
      printf ("User payload %s ELF at addr %p (%d bytes)\n",
	      get_arch_name (elf_arch), elf_start, elf_size);

      switch (elf_arch)
	{
#if EC_MACHINE_I386 || EC_MACHINE_AMD64
	case ARCH_386:
	  uentry = load_elf32 (elf_start, 1);
	  break;
	case ARCH_AMD64:
	  uentry = load_elf64 (elf_start, 1);
	  break;
#endif
#if EC_MACHINE_RISCV64
	case ARCH_RISCV64:
	  uentry = load_elf64 (elf_start, 1);
	  break;
#endif
	default:
	  printf ("Unsupported ELF architecture");
	  exit (-1);
	}
      printf ("User entry: %llx\n", uentry);
    }
  else
    {
      uentry = 0;
    }

  /* Stop allocations as we're copying boot-time allocation. */
  stop_payload_allocation = true;
  va_info_copy (uentry, req_region_num);
  va_pfnmap_copy ();
  va_stree_copy ();
  va_regions_copy ();

  va_entry (kentry);
  return 0;
}
