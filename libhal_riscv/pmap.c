#include "internal.h"
#include <nux/hal.h>
#include <nux/nux.h>

bool
hal_kmap_getl1p (unsigned long va, bool alloc, hal_l1p_t * l1popq)
{
  hal_l1p_t l1p;

  if (va < pt_umap_maxaddr ())
    {
      if (l1popq != NULL)
	*l1popq = L1P_INVALID;
      return false;
    }

  l1p = cpumap_get_l1p (va, alloc);

  if (l1popq != NULL)
    *l1popq = l1p;

  return l1p != L1P_INVALID;
}

bool
hal_umap_getl1p (struct hal_umap *umap, unsigned long uaddr, bool alloc,
		 hal_l1p_t * l1popq)
{
  hal_l1p_t l1p;

  if ((uaddr >= pt_umap_maxaddr ()) || (uaddr < pt_umap_minaddr ()))
    {
      if (l1popq != NULL)
	*l1popq = L1P_INVALID;
      return false;
    }

  l1p = umap_get_l1p (umap, uaddr, alloc);
  if (l1popq != NULL)
    *l1popq = l1p;

  return l1p != L1P_INVALID;
}

hal_l1e_t
hal_l1e_get (hal_l1p_t l1popq)
{
  return (hal_l1e_t) get_pte (l1popq);
}

hal_l1e_t
hal_l1e_set (hal_l1p_t l1popq, hal_l1e_t l1e)
{
  hal_l1e_t ol1e;

  ol1e = set_pte ((ptep_t) l1popq, (pte_t) l1e);
  return ol1e;
}


hal_l1e_t
hal_l1e_box (unsigned long pfn, unsigned prot)
{
  hal_l1e_t l1e;

  l1e = (uint64_t) pfn << PTE_PFN_SHIFT;

  if (prot & HAL_PTE_P)
    l1e |= (PTE_V | PTE_R);
  if (prot & HAL_PTE_W)
    l1e |= PTE_W;
  if (prot & HAL_PTE_X)
    l1e |= PTE_X;
  if (prot & HAL_PTE_U)
    l1e |= PTE_U;
  if (prot & HAL_PTE_GLOBAL)
    l1e |= PTE_GLOBAL;
  if (prot & HAL_PTE_A)
    l1e |= PTE_A;
  if (prot & HAL_PTE_D)
    l1e |= PTE_D;
  if (prot & HAL_PTE_AVL0)
    l1e |= PTE_AVL0;
  if (prot & HAL_PTE_AVL1)
    l1e |= PTE_AVL1;

  return l1e;
}

void
hal_l1e_unbox (hal_l1e_t l1e, unsigned long *pfnp, unsigned *protp)
{
  unsigned prot = 0;

  if (l1e & PTE_V)
    {
      if (l1e & PTE_R)
	prot |= HAL_PTE_P;
      if (l1e & PTE_W)
	prot |= HAL_PTE_W;
      if (l1e & PTE_X)
	prot |= HAL_PTE_X;
      if (l1e & PTE_U)
	prot |= HAL_PTE_U;
      if (l1e & PTE_GLOBAL)
	prot |= HAL_PTE_GLOBAL;
      if (l1e & PTE_A)
	prot |= HAL_PTE_A;
      if (l1e & PTE_D)
	prot |= HAL_PTE_D;
      if (l1e & PTE_AVL0)
	prot |= HAL_PTE_AVL0;
      if (l1e & PTE_AVL1)
	prot |= HAL_PTE_AVL1;
    }

  if (pfnp)
    *pfnp = pte_pfn (l1e);
  if (protp)
    *protp = prot;
}

hal_tlbop_t
hal_l1e_tlbop (hal_l1e_t old, hal_l1e_t new)
{
  /* Without SVVPTC always flush. */
  return HAL_TLBOP_FLUSH;
}

uaddr_t
hal_umap_next (struct hal_umap *umap, uaddr_t uaddr, hal_l1p_t * l1p,
	       hal_l1e_t * l1e)
{
  if (uaddr < pt_umap_minaddr ())
    uaddr = pt_umap_minaddr ();
  if (uaddr >= pt_umap_maxaddr ())
    return UADDR_INVALID;

  return pt_umap_next (umap, uaddr, l1p, l1e);
}

void
hal_umap_free (struct hal_umap *umap)
{
  return pt_umap_free (umap);
}
