/*
  APXH: An ELF boot-loader.
  Copyright (C) 2019 Gianluca Guida <glguida@tlbflush.org>

  SPDX-License-Identifier:	BSD-2-Clause
*/

#ifndef _PROJECT_H
#define _PROJECT_H

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <framebuffer.h>
#include <nux/apxh.h>

#define BOOTMEM MB(512)		/* We won't be using more than 512Mb to boot. Promise. */

typedef int64_t ssize64_t;
typedef uint64_t size64_t;
typedef uint64_t vaddr_t;

#define BOOTINFO_REGION_UNKNOWN 0	/* Unusable address. */
#define BOOTINFO_REGION_RAM 1	/* Available RAM. */
#define BOOTINFO_REGION_OTHER 2	/* Non-RAM physical address. */
#define BOOTINFO_REGION_BSY 3	/* Boot allocated RAM. */

struct bootinfo_region
{
  int type;
  unsigned len;
  unsigned long pfn;
};

enum memory_type
{
  MEMTYPE_WC,			/* Write Combining. */
  MEMTYPE_WB,			/* Write Back. */
  MEMTYPE_UC,			/* Uncached. */
};

/* APXH ELF extensions. */
#define PHT_APXH_INFO       0xAF100000	/* Info Page. */
#define PHT_APXH_EMPTY      0xAF100001	/* Empty (no page tables). */
#define PHT_APXH_PHYSMAP    0xAF100002	/* 1:1 Memory Map. */
#define PHT_APXH_PFNMAP     0xAF100003	/* PFN Map. */
#define PHT_APXH_STREE      0xAF100004	/* Allocated Pages Bitmap. */
#define PHT_APXH_PTALLOC    0xAF100005	/* Empty (alloc all page tables). */
#define PHT_APXH_FRAMEBUF   0xAF100006	/* Frame Buffer. */
#define PHT_APXH_REGIONS    0xAF100007	/* Region List. */
#define PHT_APXH_TOPPTALLOC 0xAF100008	/* Empty (alloc all top-level PTs). */
#define PHT_APXH_LINEAR     0xAF10FFFF	/* Linear map. */

#define PFNMAP_ENTRY_SIZE 64

#define PAGE_SHIFT 12
#define PAGE_SIZE (1LL << PAGE_SHIFT)
#define PAGE_MASK ~(PAGE_SIZE - 1)

#define PAGE2M_SHIFT 21
#define PAGE2M_SIZE (1LL << PAGE2M_SHIFT)
#define PAGE2M_MASK ~(PAGE2M_SIZE - 1)

#define MB(_x) ((unsigned long)(_x) << 20)
#define BITMAP_SZ(_s) ((_s) >> 3)	// POW2
#define PAGEMAP_SZ(_s) BITMAP_SZ((_s) >> PAGE_SHIFT)	// POW2

#define PAGE_ROUND(_a) (((_a) + (PAGE_SIZE-1)) & PAGE_MASK)
#define PAGE_CEILING(_a) (((_a) + PAGE_SIZE) & PAGE_MASK)

typedef enum
{
  ARCH_INVALID,
  ARCH_UNSUPPORTED,
  ARCH_386,
  ARCH_AMD64,
  ARCH_RISCV64,
} arch_t;

void md_init (void);
uint64_t md_maxpfn (void);
uint64_t md_minrampfn (void);
uint64_t md_maxrampfn (void);
unsigned md_memregions (void);
struct bootinfo_region *md_getmemregion (unsigned i);
struct fbdesc *md_getframebuffer (void);
struct apxh_pltdesc *md_getpltdesc (void);
void md_verify (vaddr_t va, size64_t size);
void md_entry (arch_t arch, vaddr_t pt, vaddr_t entry);

void *payload_get (unsigned i, size_t *size);

typedef enum
{
  PAYLOAD_KERNEL,
  PAYLOAD_USER,
} plid_t;

void *get_payload_start (int argc, char *argv[], plid_t id);
size_t get_payload_size (plid_t id);

arch_t get_elf_arch (void *elf);
vaddr_t load_elf32 (void *elf, int u);
vaddr_t load_elf64 (void *elf, int u);

uintptr_t get_page (void);
uintptr_t get_payload_page (void);

void va_init (void);
uintptr_t va_getphys (vaddr_t va);
void va_verify (vaddr_t va, size64_t size);
void va_populate (vaddr_t va, size64_t size, int u, int w, int x);
void va_memcpy (vaddr_t va, void *addr, size64_t size, int u, int w, int x);
void va_memset (vaddr_t va, int c, size64_t size, int u, int w, int x);
void va_physmap (vaddr_t va, size64_t size, enum memory_type);
void va_linear (vaddr_t va, size64_t size);
void va_info (vaddr_t va, size64_t size);
void va_pfnmap (vaddr_t va, size64_t size);
void va_stree (vaddr_t va, size64_t size);
void va_topptalloc (vaddr_t va, size64_t size);
void va_ptalloc (vaddr_t va, size64_t size);
void va_framebuf (vaddr_t va, size64_t size, enum memory_type);
void va_regions (vaddr_t va, size64_t size);
void va_ktls (vaddr_t va, size64_t initsize, size64_t size);
void va_utls (vaddr_t va, size64_t initsize, size64_t size);
void va_entry (vaddr_t entry);

void pae_init (void);
uintptr_t pae_getphys (vaddr_t va);
void pae_verify (vaddr_t va, size64_t size);
void pae_populate (vaddr_t va, size64_t size, int u, int w, int x);
void pae_physmap (vaddr_t va, size64_t size, uint64_t pa, enum memory_type);
void pae_ptalloc (vaddr_t va, size64_t size);
void pae_topptalloc (vaddr_t va, size64_t size);
void pae_linear (vaddr_t va, size64_t size);
void pae_entry (vaddr_t entry);

/* Internal PAE functions. */
void pae_directmap (void *pt, uint64_t pa, vaddr_t va, size64_t size,
		    enum memory_type, int payload, int x);
void pae_map_page (void *pt, vaddr_t va, uintptr_t pa, int payload, int w,
		   int x);

void pae64_init (void);
uintptr_t pae64_getphys (vaddr_t va);
void pae64_verify (vaddr_t va, size64_t size);
void pae64_populate (vaddr_t va, size64_t size, int u, int w, int x);
void pae64_physmap (vaddr_t va, size64_t size, uint64_t pa, enum memory_type);
void pae64_ptalloc (vaddr_t va, size64_t size);
void pae64_topptalloc (vaddr_t va, size64_t size);
void pae64_linear (vaddr_t va, size64_t size);
void pae64_entry (vaddr_t entry);

/* Internal PAE64 functions. */
void pae64_directmap (void *pt, uint64_t pa, vaddr_t va, size64_t size,
		      enum memory_type, int payload, int x);
void pae64_map_page (void *pt, vaddr_t va, uintptr_t pa, int payload, int w,
		     int x);

void sv48_init (void);
uintptr_t sv48_getphys (vaddr_t va);
void sv48_verify (vaddr_t va, size64_t size);
void sv48_populate (vaddr_t va, size64_t size, int u, int w, int x);
void sv48_physmap (vaddr_t va, size64_t size, uint64_t pa, enum memory_type);
void sv48_ptalloc (vaddr_t va, size64_t size);
void sv48_topptalloc (vaddr_t va, size64_t size);
void sv48_linear (vaddr_t va, size64_t size);
void sv48_entry (vaddr_t entry);

/* Internal SV48 functions. */
void sv48_directmap (void *pt, uint64_t pa, vaddr_t va, size64_t size,
		     enum memory_type, int payload, int x);
void sv48_map_page (void *pt, vaddr_t va, uintptr_t pa, int payload, int w,
		    int x);


#define info(...) do { printf (__VA_ARGS__); putchar('\n'); } while (0)
#define debug(...) do { printf (__VA_ARGS__); putchar('\n'); } while (0)
#define warn(...) do { printf ("Warning: "); printf (__VA_ARGS__); putchar('\n'); } while (0)
#define fatal(...) do { printf ("Fatal: "); printf (__VA_ARGS__); putchar('\n'); exit(-1); } while (0)

#endif
