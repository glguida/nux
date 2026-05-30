/*
  NUX: A kernel Library.
  Copyright (C) 2019 Gianluca Guida, glguida@tlbflush.org

  SPDX-License-Identifier:	BSD-2-Clause
*/

#include <stddef.h>
#include <rbtree.h>
#include <assert.h>

#include "internal.h"
#include <nux/nux.h>

/*
 * VM allocator
 */

static rb_tree_t vmap_rbtree;
static vaddr_t kvabase;
static size_t kvasize;
static size_t vmap_size;


struct vme
{
  struct rb_node rb_node;
    LIST_ENTRY (vme) list;
  vaddr_t addr;
  size_t size;
};

struct vmap
{
  rb_tree_t rbtree;
  size_t size;
};

static void
vmap_remove (struct vme *vme)
{

  /* ASSERT ISA(vme) XXX: */
  rb_tree_remove_node (&vmap_rbtree, (void *) vme);
  vmap_size -= vme->size;
  kmem_free (0, (vaddr_t) vme, sizeof (struct vme));
}

static struct vme *
vmap_find (vaddr_t va)
{
  struct vme;

  /* ASSERT ISA(vme returned) XXX: */
  return rb_tree_find_node (&vmap_rbtree, (void *) &va);
}

static struct vme *
vmap_insert (vaddr_t start, size_t len)
{
  struct vme *vme;

  vme = (struct vme *) kmem_alloc (0, sizeof (struct vme));
  vme->addr = start;
  vme->size = len;
  rb_tree_insert_node (&vmap_rbtree, (void *) vme);
  vmap_size += vme->size;
  return vme;
}

static int
vmap_compare_key (void *ctx, const void *n, const void *key)
{
  const struct vme *vme = n;
  const vaddr_t va = *(const vaddr_t *) key;

  if (va < vme->addr)
    return 1;
  if (va > vme->addr + (vme->size - 1))
    return -1;
  return 0;
}

static int
vmap_compare_nodes (void *ctx, const void *n1, const void *n2)
{
  const struct vme *vmap1 = n1;
  const struct vme *vmap2 = n2;

  /* Assert non overlapping */
  assert (vmap1->addr < vmap2->addr
	  || vmap1->addr > (vmap2->addr + vmap2->size));
  assert (vmap2->addr < vmap1->addr
	  || vmap2->addr > (vmap1->addr + vmap1->size));

  if (vmap1->addr < vmap2->addr)
    return -1;
  if (vmap1->addr > vmap2->addr)
    return 1;
  return 0;
}

static const rb_tree_ops_t vmap_tree_ops = {
  .rbto_compare_nodes = vmap_compare_nodes,
  .rbto_compare_key = vmap_compare_key,
  .rbto_node_offset = offsetof (struct vme, rb_node),
  .rbto_context = NULL
};


/*
 * VM allocator.
 */

#define __ZENTRY vme
#define __ZADDR_T vaddr_t

static void
___get_neighbors (vaddr_t addr, size_t size, struct vme **pv, struct vme **nv,
		  uintptr_t opq)
{
  vaddr_t end = addr + size;
  struct vme *pvme = NULL, *nvme = NULL;

  if (addr == 0)
    goto _next;

  pvme = vmap_find (addr - 1);
  if (pvme != NULL)
    *pv = pvme;

_next:
  nvme = vmap_find (end);
  if (nvme != NULL)
    *nv = nvme;
}

static struct vme *
___mkptr (vaddr_t addr, size_t size, uintptr_t opq)
{

  return vmap_insert (addr, size);
}

static void
___freeptr (struct vme *vme, uintptr_t opq)
{

  vmap_remove (vme);
}

#include "alloc.h"

static lock_t vmap_lock;
static struct zone vmap_zone;

vaddr_t
kva_alloc (size_t size)
{
  size_t pgsz;
  vaddr_t va;

  pgsz = round_page (size);
  spinlock (&vmap_lock);
  va = zone_alloc (&vmap_zone, pgsz);
  spinunlock (&vmap_lock);
  if (va == 0)
    return VADDR_INVALID;

  return va;
}

void
kva_free (vaddr_t va, size_t size)
{

  va = trunc_page (va);
  size = round_page (size);
  spinlock (&vmap_lock);
  zone_free (&vmap_zone, va, size);
  spinunlock (&vmap_lock);
}

void *
kva_map (pfn_t pfn, unsigned prot)
{
  vaddr_t va;

  va = kva_alloc (PAGE_SIZE);
  if (va == VADDR_INVALID)
    return NULL;

  kmap_map (va, pfn, prot);
  kmap_commit ();
  return (void *) va;
}

void *
kva_physmap (paddr_t paddr, size_t size, unsigned prot)
{
  vaddr_t va;
  pfn_t pfn;
  unsigned no, i;

  pfn = paddr >> PAGE_SHIFT;
  no = round_page ((paddr & PAGE_MASK) + size) >> PAGE_SHIFT;

  va = kva_alloc (no * PAGE_SIZE);
  if (va == VADDR_INVALID)
    return NULL;

  for (i = 0; i < no; i++)
    kmap_map (va + i * PAGE_SIZE, pfn + i, prot);
  kmap_commit ();

  return (void *) (uintptr_t) (va + (paddr & PAGE_MASK));
}

void
kva_unmap (void *ptr, size_t size)
{
  unsigned no, i;
  vaddr_t vaddr;

  vaddr = (uintptr_t) ptr;
  no = round_page ((vaddr & PAGE_MASK) + size) >> PAGE_SHIFT;
  vaddr = trunc_page (vaddr);

  for (i = 0; i < no; i++)
    kmap_unmap (vaddr + i * PAGE_SIZE);
  kmap_commit ();

  kva_free (vaddr, no * PAGE_SIZE);
}

void
kvainit (void)
{
  rb_tree_init (&vmap_rbtree, &vmap_tree_ops);
  zone_init (&vmap_zone, 0);
  spinlock_init (&vmap_lock);
  vmap_size = 0;

  kvabase = hal_virtmem_kvabase ();
  kvasize = hal_virtmem_kvasize ();
  kva_free (kvabase, kvasize);
  info ("KVA Area from %lx to %lx", kvabase, kvabase + kvasize);
}
