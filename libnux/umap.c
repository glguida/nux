/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include "internal.h"
#include <assert.h>
#include <nux/nux.h>

/*
  Low level routines to handle user mappings.

  NUX doesn't lock UMAP access, caller is responsible to be sure no
  concurrent calls on the same umap are done.
*/

static bool
_umap_setl1e (struct umap *umap, vaddr_t va, hal_l1e_t l1e, bool alloc,
	      pfn_t * opfn)
{
  hal_l1p_t l1p;
  hal_l1e_t oldl1e;
  pfn_t oldpfn;
  unsigned oldprot;
  struct hal_umap *hal;

  hal = (umap == cpu_umap_current ()) ? NULL : &umap->hal;
  if (!hal_umap_getl1p (hal, va, alloc, &l1p))
    {
      if (opfn)
	*opfn = PFN_INVALID;
      return false;
    }
  oldl1e = hal_l1e_set (l1p, l1e);
  __atomic_or_fetch (&umap->tlbop, hal_l1e_tlbop (oldl1e, l1e),
		     __ATOMIC_RELEASE);

  hal_l1e_unbox (oldl1e, &oldpfn, &oldprot);
  if (opfn != NULL)
    *opfn = oldprot & HAL_PTE_P ? oldpfn : PFN_INVALID;

  return true;
}

unsigned
umap_chflags (struct umap *umap, vaddr_t va,
	      unsigned prot_set, unsigned prot_clr)
{
  hal_l1p_t l1p;
  hal_l1e_t oldl1e, l1e;
  pfn_t pfn;
  unsigned oldflags, flags;

  if (!hal_umap_getl1p (&umap->hal, va, false, &l1p))
    return 0;

  l1e = hal_l1e_get (l1p);
  hal_l1e_unbox (l1e, &pfn, &oldflags);
  flags = oldflags | prot_set;
  flags &= ~prot_clr;
  l1e = hal_l1e_box (pfn, flags);
  oldl1e = hal_l1e_set (l1p, l1e);
  __atomic_or_fetch (&umap->tlbop, hal_l1e_tlbop (oldl1e, l1e),
		     __ATOMIC_RELEASE);
  return oldflags;
}

bool
umap_map (struct umap *umap, vaddr_t va, pfn_t pfn, unsigned prot,
	  pfn_t * opfn)
{
  hal_l1e_t l1e;

  l1e = hal_l1e_box (pfn, prot);
  return _umap_setl1e (umap, va, l1e, true, opfn);
}

pfn_t
umap_unmap (struct umap *umap, vaddr_t va)
{
  hal_l1p_t l1p;
  hal_l1e_t l1e, oldl1e;
  pfn_t oldpfn;
  unsigned oldprot;

  if (!hal_umap_getl1p (&umap->hal, va, false, &l1p))
    {
      return PFN_INVALID;
    }

  l1e = hal_l1e_box (PFN_INVALID, 0);
  oldl1e = hal_l1e_set (l1p, l1e);
  __atomic_or_fetch (&umap->tlbop, hal_l1e_tlbop (oldl1e, l1e),
		     __ATOMIC_RELEASE);

  hal_l1e_unbox (oldl1e, &oldpfn, &oldprot);
  return oldprot & HAL_PTE_P ? oldpfn : PFN_INVALID;
}

void
umap_commit (struct umap *umap)
{
  __atomic_clear (&umap->tlbop, __ATOMIC_RELEASE);
  cpu_tlbflush_mask (umap->cpumask);
}

void
umap_bootstrap (struct umap *umap)
{
  umap->tlbop = 0;
  umap->cpumask = 0;
  hal_umap_bootstrap (&umap->hal);
}

void
umap_free (struct umap *umap)
{
  assert (umap->cpumask == 0);
  hal_umap_free (&umap->hal);
}

void
umap_init (struct umap *umap)
{
  umap->tlbop = 0;
  umap->cpumask = 0;
  hal_umap_init (&umap->hal);
}
