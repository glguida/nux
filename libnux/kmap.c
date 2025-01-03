/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <assert.h>
#include <nux/hal.h>
#include <nux/nux.h>

#include "internal.h"

/*
  Low level routines to handle kernel mappings.

  Unlocked, unflushing, use with care.
*/

void
kmapinit (void)
{
}

static pfn_t
_kmap_map (vaddr_t va, pfn_t pfn, unsigned prot, const int alloc)
{
  hal_l1p_t l1p;
  hal_l1e_t l1e, oldl1e;
  pfn_t oldpfn;
  unsigned oldprot;

  l1e = hal_l1e_box (pfn, prot);

  assert (hal_kmap_getl1p (va, alloc, &l1p));
  oldl1e = hal_l1e_set (l1p, l1e);
  ktlbgen_markdirty (hal_l1e_tlbop (oldl1e, l1e));

  hal_l1e_unbox (oldl1e, &oldpfn, &oldprot);

  return oldprot & HAL_PTE_P ? oldpfn : PFN_INVALID;
}

pfn_t
kmap_getpfn (vaddr_t va)
{
  pfn_t pfn;
  unsigned flags;
  hal_l1e_t l1e;
  hal_l1p_t l1p;

  if (!hal_kmap_getl1p (va, false, &l1p))
    return PFN_INVALID;

  l1e = hal_l1e_get (l1p);
  hal_l1e_unbox (l1e, &pfn, &flags);

  return flags & HAL_PTE_P ? pfn : PFN_INVALID;
}

pfn_t
kmap_map (vaddr_t va, pfn_t pfn, unsigned prot)
{
  return _kmap_map (va, pfn, prot, 1);
}

/*
  Only map a VA if no pagetable allocations are needed.
*/
pfn_t
kmap_map_noalloc (vaddr_t va, pfn_t pfn, unsigned prot)
{
  return _kmap_map (va, pfn, prot, 0);
}

/*
  Unmap a page previously mapped with kmap_map.
*/
pfn_t
kmap_unmap (vaddr_t va)
{
  hal_l1p_t l1p;
  hal_l1e_t l1e, oldl1e;
  pfn_t oldpfn;
  unsigned oldprot;

  l1e = hal_l1e_box (0, 0);
  if (hal_kmap_getl1p (va, 0, &l1p))
    {
      oldl1e = hal_l1e_set (l1p, l1e);
      ktlbgen_markdirty (hal_l1e_tlbop (oldl1e, l1e));

      hal_l1e_unbox (oldl1e, &oldpfn, &oldprot);

      return oldprot & HAL_PTE_P ? oldpfn : PFN_INVALID;
    }

  return PFN_INVALID;
}


/*
  Check if va is mapped.
*/
int
kmap_mapped (vaddr_t va)
{

  return hal_kmap_getl1p (va, 0, NULL);
}

int
kmap_mapped_range (vaddr_t va, size_t size)
{
  vaddr_t i, s, e;

  s = trunc_page (va);
  e = va + size;

  for (i = s; i < e; i += PAGE_SIZE)
    if (!kmap_mapped (i))
      return 0;

  return 1;
}

int
kmap_ensure (vaddr_t va, unsigned reqprot)
{
  int ret = -1;
  hal_l1p_t l1p = L1P_INVALID;
  hal_l1e_t oldl1e, l1e;
  pfn_t pfn;
  unsigned prot;

  if (hal_kmap_getl1p (va, 0, &l1p))
    {
      l1e = hal_l1e_get (l1p);
      hal_l1e_unbox (l1e, &pfn, &prot);
    }
  else
    {
      pfn = PFN_INVALID;
      prot = 0;
    }

  if (!(reqprot ^ prot))
    {
      /* same, exit */
      ret = 0;
      goto out;
    }

  /* Check present bit. If we are adding a P bit allocate, if we are
     removing it free the page. */
  if ((reqprot & HAL_PTE_P) != (prot & HAL_PTE_P))
    {
      if (reqprot & HAL_PTE_P)
	{
	  /* Ensure pagetable populated. */
	  if (l1p == L1P_INVALID)
	    assert (hal_kmap_getl1p (va, 1, &l1p));
	  /* Populate page. */
	  pfn = pfn_alloc (0);
	  if (pfn == PFN_INVALID)
	    goto out;
	}
      else
	{
	  /* Freeing page. */
	  pfn_free (pfn);
	  pfn = PFN_INVALID;
	}
    }
  l1e = hal_l1e_box (pfn, reqprot);
  oldl1e = hal_l1e_set (l1p, l1e);
  ktlbgen_markdirty (hal_l1e_tlbop (oldl1e, l1e));
  ret = 0;

out:
  return ret;
}

int
kmap_ensure_range (vaddr_t va, size_t size, unsigned reqprot)
{
  vaddr_t i, s, e;

  s = trunc_page (va);
  e = va + size;

  for (i = s; i < e; i += PAGE_SIZE)
    if (kmap_ensure (i, reqprot))
      return -1;

  return 0;
}

void
kmap_commit (void)
{
  /* 
     This is extremely slow, but guarantees KMAP to be aligned in all
     CPUs.
   */
  cpu_kmapupdate_broadcast ();
}
