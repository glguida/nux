/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <stdio.h>
#include <string.h>

#include "internal.h"
#include <nux/nux.h>

#define LO 0
#define HI 1

static lock_t brklock;
static vaddr_t base[2];
static vaddr_t brk[2];
static vaddr_t maxbrk[2];
#ifdef HAL_PAGED
static int kmem_trim = TRIM_NONE;
#endif

#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#define MAX(a,b) ((a) >= (b) ? (a) : (b))

#ifdef HAL_PAGED
#define KMEM_TYPE "Paged"
#else
#define KMEM_TYPE "Unpaged"
#endif

static int
cmp (vaddr_t a, vaddr_t b)
{
  if (a > b)
    return 1;
  if (a < b)
    return -1;
  return 0;
}

#ifdef HAL_PAGED
static int
_ensure_range (vaddr_t v1, vaddr_t v2, int mapped)
{
  /* Called with bkrlock held. */
  vaddr_t s, e;
  unsigned prot;

  s = MIN (v1, v2);
  e = MAX (v1, v2);
  prot = mapped ? HAL_PTE_P | HAL_PTE_W : 0;

  if (kmap_ensure_range (s, e - s, prot))
    return -1;

  kmap_commit ();
  return 0;
}

static int
_ensure_range_mapped (vaddr_t v1, vaddr_t v2)
{
  return _ensure_range (v1, v2, 1);
}

static int
_ensure_range_unmapped (vaddr_t v1, vaddr_t v2)
{
  return _ensure_range (v1, v2, 0);
}
#endif

int
kmem_brk (int low, vaddr_t vaddr)
{
  int ret = -1;
  int this = low ? LO : HI;
  int other = low ? HI : LO;

  spinlock (&brklock);

  if (low ? vaddr < base[LO] : vaddr > base[HI])
    goto out;

  /* Check if we pass the other brk. */
  if (cmp (vaddr, brk[other]) != cmp (brk[this], brk[other]))
    goto out;

#ifdef HAL_PAGED
  if (_ensure_range_mapped (brk[this], vaddr))
    goto out;
#endif

  brk[this] = vaddr;
  ret = 0;

out:
  spinunlock (&brklock);
  return ret;
}

vaddr_t
kmem_sbrk (int low, long inc)
{
  const int this = low ? LO : HI;
  const int other = low ? HI : LO;
  vaddr_t ret = VADDR_INVALID;
  vaddr_t vaddr;

  spinlock (&brklock);
  if (inc == 0)
    {
      ret = brk[this];
      goto out;
    }

  vaddr = brk[this] + inc;

  if (low ? vaddr < base[LO] : vaddr > base[HI])
    goto out;

  /* Check if we pass the other brk. */
  if (cmp (vaddr, brk[other]) != cmp (brk[this], brk[other]))
    goto out;

#ifdef HAL_PAGED
  if (_ensure_range_mapped (brk[this], vaddr))
    goto out;
#endif

  maxbrk[this] = low ? MAX (maxbrk[this], vaddr) : MIN (maxbrk[this], vaddr);
  ret = brk[this];
  brk[this] = vaddr;

out:
  spinunlock (&brklock);

  return ret;
}

vaddr_t
kmem_brkgrow (int low, unsigned size)
{
  vaddr_t ret;

  if (low)
    ret = kmem_sbrk (low, size);
  else
    {
      ret = kmem_sbrk (low, -(long) size);
      ret = ret != VADDR_INVALID ? ret - size : ret;
    }

  return ret;
}

int
kmem_brkshrink (int low, unsigned size)
{
  vaddr_t va;
  long inc;

  inc = low ? -size : size;
  va = kmem_sbrk (low, inc);

  return va == VADDR_INVALID ? -1 : 0;
}

/*
  KMEM heap allocation.

  Allocate in batches of 64-bytes. zaddr_t is a 64-byte index into the
  KMEM area.
*/

typedef unsigned long zaddr_t;
#define v_to_z(_v) ((_v) >> 6)
#define z_to_v(_z) ((_z) << 6)
#define zsize(_size) (v_to_z((_size) + 63))
#define size_zalign(_size) z_to_v(zsize(_size))

struct kmem_head
{
  unsigned long magic;
    LIST_ENTRY (kmem_head) list;
  vaddr_t addr;
  size_t size;
};

struct kmem_tail
{
  size_t offset;
  unsigned long magic;
};

//#define kmdbg_printf(...) printf(__VA_ARGS__)
#define kmdbg_printf(...)

#define ZONE_HEAD_MAGIC 0x616001DA
#define ZONE_TAIL_MAGIC 0x616001DA
#define __ZENTRY  kmem_head
#define __ZADDR_T zaddr_t

static struct kmem_head *
___mkptr (zaddr_t zaddr, size_t size, uintptr_t opq)
{
  struct kmem_head *ptr;
  struct kmem_tail *tail;
  vaddr_t addr = z_to_v (zaddr);

  ptr = (struct kmem_head *) addr;
  ptr->magic = ZONE_HEAD_MAGIC;
  ptr->addr = zaddr;
  ptr->size = size;

  tail =
    (struct kmem_tail *) ((void *) ptr + z_to_v(size) - sizeof (struct kmem_tail));
  tail->magic = ZONE_TAIL_MAGIC;
  tail->offset = z_to_v(size) - sizeof (struct kmem_tail);

  /* XXX: UNPAGE FREE PAGES IN THE MIDDLE. */

  return ptr;
}

static void
___freeptr (struct kmem_head *ptr, uintptr_t opq)
{
  struct kmem_tail *tail;
  tail =
    (struct kmem_tail *) ((void *) ptr + ptr->size -
			  sizeof (struct kmem_tail));
  memset (ptr, 0, sizeof (*ptr));
  memset (tail, 0, sizeof (*tail));

  /* XXX: ENSURE SECTION IS POPULATED. */
}

static void
___get_neighbors (zaddr_t zaddr, size_t size,
		  struct kmem_head **ph, struct kmem_head **nh, uintptr_t opq)
{

  int low = opq;
  vaddr_t vaddr;
  vaddr_t ptail;
  vaddr_t nhead;
  struct kmem_head *h;
  struct kmem_tail *t;

  vaddr = z_to_v (zaddr);
  ptail = vaddr - sizeof (struct kmem_tail);
  nhead = vaddr + size;

  spinlock (&brklock);
  if (low)
    {
      if (ptail < base[LO])
	ptail = VADDR_INVALID;
      if (nhead + sizeof (struct kmem_head) > brk[LO])
	nhead = VADDR_INVALID;
    }
  else
    {
      if (ptail < brk[HI])
	ptail = VADDR_INVALID;
      if (nhead + sizeof (struct kmem_head) > base[HI])
	nhead = VADDR_INVALID;
    }
  spinunlock (&brklock);

  if (ptail == VADDR_INVALID)
    goto check_next;

  t = (struct kmem_tail *) ptail;
#ifdef HAL_PAGED
  if (!kmap_mapped_range ((vaddr_t) t, sizeof (struct kmem_tail)))
    goto check_next;
#endif

  if (t->magic != ZONE_TAIL_MAGIC)
    goto check_next;

  h = (struct kmem_head *) (ptail - t->offset);
#ifdef HAL_PAGED
  if (!kmap_mapped_range ((vaddr_t) h, sizeof (struct kmem_head)))
    goto check_next;
#endif

  if (h->magic != ZONE_HEAD_MAGIC)
    goto check_next;

  *ph = h;

check_next:

  if (nhead == VADDR_INVALID)
    return;

  h = (struct kmem_head *) nhead;
#ifdef HAL_PAGED
  if (!kmap_mapped_range ((vaddr_t) h, sizeof (struct kmem_head)))
    return;
#endif

  if (h->magic != ZONE_HEAD_MAGIC)
    return;

  *nh = h;
}

#include "alloc.h"

static lock_t lockz[2];
static struct zone kmemz[2];

vaddr_t
kmem_alloc (int low, size_t size)
{
  zaddr_t zr;
  vaddr_t r;
  lock_t *l;
  struct zone *z;
  size_t size_64b;

  size_64b = size_zalign (size);

  z = low ? kmemz + LO : kmemz + HI;
  l = low ? lockz + LO : lockz + HI;

  spinlock (l);
  zr = zone_alloc (z, zsize (size_64b));
  spinunlock (l);
  if (zr != (zaddr_t) - 1)
    return z_to_v (zr);

  r = kmem_brkgrow (low, size_64b);
  return r;
}

void
kmem_free (int low, vaddr_t vaddr, size_t size)
{
  unsigned this;
  vaddr_t base;
  vaddr_t limit;
  struct zone *z;
  lock_t *l;
  size_t size_64b;

  size_64b = size_zalign (size);

  this = low ? LO : HI;
  base = low ? vaddr : vaddr + size_64b;
  limit = low ? vaddr + size_64b : vaddr;

  /*
     If we're freeing up to the BRK, reduce BRK allocation.
   */
  spinlock (&brklock);
  kmdbg_printf ("(BRK) %lx == %lx ? ", brk[this], limit);
  if (brk[this] == limit)
    {
      kmdbg_printf ("BRK set to %lx\n", base);
      brk[this] = base;

      if (kmem_trim >= TRIM_BRK)
	{
	  /*
	     If in TRIM mode, unmap and free unneeded pages.
	   */
	  vaddr_t v1 = low ? round_page (base) : trunc_page (base);
	  vaddr_t v2 = low ? round_page (limit) : trunc_page (limit);

	  kmdbg_printf ("Unmapping from [%lx-%lx] ", v1, v2);
	  _ensure_range_unmapped (v1, v2);
	  kmdbg_printf (" done\n");
	}
      spinunlock (&brklock);
      goto out;
    }
  spinunlock (&brklock);

  /*
     Free using allocator.
   */
  z = kmemz + this;
  l = lockz + this;
  spinlock (l);
  zone_free (z, v_to_z (vaddr), zsize (size_64b));
  spinunlock (l);

out:
  return;
}

void
kmem_trim_one (unsigned trim_mode)
{
  spinlock (&brklock);
  if (trim_mode >= TRIM_BRK)
    {
      /* Unmap all pages between the BRKs. */
      _ensure_range_unmapped (round_page (brk[LO]), round_page (maxbrk[LO]));
      maxbrk[LO] = brk[LO];
      _ensure_range_unmapped (trunc_page (brk[HI]), trunc_page (maxbrk[HI]));
      maxbrk[HI] = brk[HI];
    }
  spinunlock (&brklock);
}

void
kmem_trim_setmode (unsigned trim_mode)
{
  kmdbg_printf ("Setting TRIM mode to %d (%s).\n",
		trim_mode,
		trim_mode == TRIM_NONE ? "off" :
		trim_mode == TRIM_BRK ? "BRK" : "unknown");

  spinlock (&brklock);
  kmem_trim = trim_mode;
  spinunlock (&brklock);
}

void
kmeminit (void)
{
  base[LO] = hal_virtmem_kmembase ();
  base[HI] = hal_virtmem_kmembase () + hal_virtmem_kmemsize ();

  brk[LO] = base[LO];
  brk[HI] = base[HI];

  maxbrk[LO] = base[LO];
  maxbrk[HI] = base[HI];

  zone_init (kmemz + LO, 0);
  spinlock_init (lockz + LO);
  zone_init (kmemz + HI, 0);
  spinlock_init (lockz + HI);

  kmdbg_printf ("%s KMEM from %08lx to %08lx\n", KMEM_TYPE, brk[LO], brk[HI]);
}
