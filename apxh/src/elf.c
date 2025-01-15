/*
  APXH: An ELF boot-loader.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include "project.h"

struct elf32ph
{
  uint32_t type;
  uint32_t off;
  uint32_t va;
  uint32_t pa;
  uint32_t fsize;
  uint32_t msize;
  uint32_t flags;
  uint32_t align;
};

struct elf64ph
{
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t va;
  uint64_t pa;
  uint64_t fsize;
  uint64_t msize;
  uint64_t align;
};

struct elf32sh
{
  uint32_t name;
  uint32_t type;
  uint32_t flags;
  uint32_t addr;
  uint32_t off;
  uint32_t size;
  uint32_t lnk;
  uint32_t info;
  uint32_t align;
  uint32_t shent_size;
};

struct elf64sh
{
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  uint64_t addr;
  uint64_t off;
  uint64_t size;
  uint32_t lnk;
  uint32_t info;
  uint64_t align;
  uint64_t shent_size;
};

struct elf32hdr
{
  uint8_t id[16];
  uint16_t type;
  uint16_t mach;
  uint32_t ver;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t eh_size;
  uint16_t phent_size;
  uint16_t phs;
  uint16_t shent_size;
  uint16_t shs;
  uint16_t shstrndx;
} __packed;

struct elf64hdr
{
  uint8_t id[16];
  uint16_t type;
  uint16_t mach;
  uint32_t ver;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t eh_size;
  uint16_t phent_size;
  uint16_t phs;
  uint16_t shent_size;
  uint16_t shs;
  uint16_t shstrndx;
} __packed;

#define ET_EXEC		2
#define EM_386		3
#define EM_X86_64	62
#define EM_RISCV	0xf3
#define EV_CURRENT	1

#define SHT_PROGBITS 	1
#define SHT_NOBITS	8

#define PHT_NULL	0
#define PHT_LOAD	1
#define PHT_DYNAMIC	2
#define PHT_INTERP	3
#define PHT_NOTE	4
#define PHT_SHLIB	5
#define PHT_PHDR	6
#define PHT_TLS		7

#define PHF_X		1
#define PHF_W		2
#define PHF_R		4

#define ELFOFF(_o) ((void *)(uintptr_t)(elfimg + (_o)))

void
ph_uload (void *elfimg, uint32_t type, uint32_t flags,
	  uint64_t va, uint64_t msize, uint64_t off, uint64_t fsize)
{
  switch (type)
    {
    case PHT_LOAD:
      /* Normal load segment. */
      if (va + msize < va)
	{
	  printf ("size of PH too big.");
	  exit (-1);
	}

      if (fsize)
	{
	  /*
	     memcpy() to user and populate on fault.
	   */
	  va_memcpy (va, ELFOFF (off), fsize, 1,
		   !!(flags & PHF_W), !!(flags & PHF_X));
	}

      if (msize - fsize > 0)
	{
	  /*
	     memset() to user and populate on fault.
	   */
	  va_memset (va + fsize, 0, msize - fsize, 1,
		     !!(flags & PHF_W), !!(flags & PHF_X));
	}
      break;
    case PHT_TLS:
      /* User Thread Local Storage. */
      if (msize != 0)
	{
	  printf ("USER TLS area at %08" PRIx64 " (initsize: %" PRId64
		  " size: %" PRId64 ").\n", va, fsize, msize);

	  if (va + msize < va)
	    {
	      printf ("size of PH too big.");
	      exit (-1);
	    }

	  if (fsize != 0)
	    {
	      va_memcpy (va, ELFOFF (off), fsize, 0,
		       !!(flags & PHF_W), !!(flags & PHF_X));
	    }
	  va_utls (va, fsize, msize);
	}
    default:
      printf ("Ignored segment type %08lx.\n", type);
      break;
    }
}

void
ph_kload (void *elfimg, uint32_t type, uint32_t flags,
	  uint64_t va, uint64_t msize, uint64_t off, uint64_t fsize)
{
  switch (type)
    {
    case PHT_LOAD:
      /* Normal load segment. */
      if (va + msize < va)
	{
	  printf ("size of PH too big.");
	  exit (-1);
	}

      if (fsize)
	{
	  /*
	     memcpy() to user and populate on fault.
	   */
	  va_memcpy (va, ELFOFF (off), fsize, 0,
		   !!(flags & PHF_W), !!(flags & PHF_X));
	}

      if (msize - fsize > 0)
	{
	  /*
	     memset() to user and populate on fault.
	   */
	  va_memset (va + fsize, 0, msize - fsize, 0,
		     !!(flags & PHF_W), !!(flags & PHF_X));
	}
      break;
    case PHT_TLS:
      /* Thread Local Storage. */
      if (msize != 0)
	{
	  printf ("TLS area at %08" PRIx64 " (initsize: %" PRId64 " size: %"
		  PRId64 ").\n", va, fsize, msize);
	  if (va + msize < va)
	    {
	      printf ("size of PH too big.");
	      exit (-1);
	    }
	  if (fsize != 0)
	    {
	      va_memcpy (va, ELFOFF (off), fsize, 0,
		       !!(flags & PHF_W), !!(flags & PHF_X));
	    }
	  va_ktls (va, fsize, msize);
	}
      break;

    case PHT_APXH_INFO:
      /* Boot Information segment. */
      printf ("Boot Information area at %" PRIx64 " (size: %" PRId64 "d).\n",
	      va, msize);
      va_info (va, msize);
      break;
    case PHT_APXH_PHYSMAP:
      /* Direct 1:1 PA mapping. */
      printf ("Physmap VA area at %" PRIx64 " (size: %" PRId64 ").\n", va,
	      msize);
      va_physmap (va, msize, MEMTYPE_WB);
      break;
    case PHT_APXH_EMPTY:
      printf ("Empty VA area at %" PRIx64 " (size: %" PRId64 ").\n", va,
	      msize);
      /* Just VA allocation. Leave it. */
      break;
    case PHT_APXH_PTALLOC:
      printf ("PT Alloc VA area at %" PRIx64 " (size: %" PRId64 ").\n", va,
	      msize);
      va_ptalloc (va, msize);
      break;
    case PHT_APXH_PFNMAP:
      printf ("PFN Map at %" PRIx64 " (size: %" PRId64 ").\n", va, msize);
      va_pfnmap (va, msize);
      break;
    case PHT_APXH_STREE:
      printf ("S-Tree at %" PRIx64 " (size: %" PRId64 ").\n", va, msize);
      va_stree (va, msize);
      break;
    case PHT_APXH_LINEAR:
      printf ("Linear Map at %" PRIx64 " (size: %" PRId64 ").\n", va, msize);
      va_linear (va, msize);
      break;
    case PHT_APXH_FRAMEBUF:
      printf ("Framebuffer Map at %" PRIx64 " (size: %" PRId64 ").\n", va,
	      msize);
      va_framebuf (va, msize, MEMTYPE_WC);
      break;
    case PHT_APXH_REGIONS:
      printf ("Region Map at %" PRIx64 " (size: %" PRId64 ").\n", va, msize);
      va_regions (va, msize);
      break;
    case PHT_APXH_TOPPTALLOC:
      printf ("TOP PT Alloc VA area at %" PRIx64 " (size: %" PRId64 ").\n",
	      va, msize);
      va_topptalloc (va, msize);
      break;
    default:
      printf ("Ignored segment type %08lx.\n", type);
      break;
    }
}

vaddr_t
load_elf32 (void *elfimg, int u)
{
  int i;

  char elfid[] = { 0x7f, 'E', 'L', 'F', };
  struct elf32hdr *hdr = (struct elf32hdr *) elfimg;
  struct elf32ph *ph = (struct elf32ph *) ELFOFF (hdr->phoff);

  if (memcmp (hdr->id, elfid, 4) != 0)
    return (uintptr_t) - 1;

  if (hdr->type != ET_EXEC || hdr->ver != EV_CURRENT)
    return (uintptr_t) - 1;

  for (i = 0; i < hdr->phs; i++, ph++)
    {
      if (u)
	ph_uload (elfimg, ph->type, ph->flags, ph->va, ph->msize, ph->off,
		  ph->fsize);
      else
	ph_kload (elfimg, ph->type, ph->flags, ph->va, ph->msize, ph->off,
		  ph->fsize);
    }

  return (vaddr_t) hdr->entry;
}

vaddr_t
load_elf64 (void *elfimg, int u)
{
  int i;

  char elfid[] = { 0x7f, 'E', 'L', 'F', };
  struct elf64hdr *hdr = (struct elf64hdr *) elfimg;
  struct elf64ph *ph = (struct elf64ph *) ELFOFF (hdr->phoff);

  if (memcmp (hdr->id, elfid, 4) != 0)
    return (uintptr_t) - 1;

  if (hdr->type != ET_EXEC || hdr->ver != EV_CURRENT)
    return (uintptr_t) - 1;

  for (i = 0; i < hdr->phs; i++, ph++)
    {
      if (u)
	ph_uload (elfimg, ph->type, ph->flags, ph->va, ph->msize, ph->off,
		  ph->fsize);
      else
	ph_kload (elfimg, ph->type, ph->flags, ph->va, ph->msize, ph->off,
		  ph->fsize);
    }

  return (vaddr_t) hdr->entry;
}


arch_t
get_elf_arch (void *elfimg)
{
  char elfid[] = { 0x7f, 'E', 'L', 'F', };
  struct elf32hdr *hdr = (struct elf32hdr *) elfimg;

  if (memcmp (hdr->id, elfid, 4) != 0)
    return ARCH_INVALID;

  if (hdr->mach == EM_386)
    return ARCH_386;

  if (hdr->mach == EM_X86_64)
    return ARCH_AMD64;

  if (hdr->mach == EM_RISCV)
    return ARCH_RISCV64;

  return ARCH_UNSUPPORTED;
}
