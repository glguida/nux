# Memory model

NUX memory management is split between APXH boot-time construction, HAL page-table primitives, and `libnux` runtime allocators.

## Core types and address spaces

`include/nux/types.h` defines:

- `paddr_t`: physical address.
- `vaddr_t`: kernel virtual address.
- `uaddr_t`: user virtual address.
- `pfn_t`: physical frame number.
- `umap_t`: a user mapping set containing a CPU mask, pending TLB operation, and HAL-specific page-table root data.

`include/nux/hal.h` describes the virtual address areas exposed by each HAL: user area, direct map, PFN cache area, KVA area, KMEM area, and boot-time user entry.

## Boot-time memory contract

APXH loads ELF program headers and handles APXH-specific segment types (`apxh/src/elf.c`, `apxh/src/project.h`):

- `PHT_APXH_INFO`: boot information page.
- `PHT_APXH_STREE`: S-tree allocation bitmap.
- `PHT_APXH_REGIONS`: physical memory region list.
- `PHT_APXH_PFNMAP`: PFN map.
- `PHT_APXH_PHYSMAP`: direct physical map.
- `PHT_APXH_FRAMEBUF`: framebuffer mapping.
- `PHT_APXH_PTALLOC` and `PHT_APXH_TOPPTALLOC`: page-table allocation regions.
- `PHT_APXH_LINEAR`: linear page-table map.

The HAL linker scripts request these areas in `libhal_x86/i386/exe.ld`, `libhal_x86/amd64/exe.ld`, and `libhal_riscv/exe.ld`. HAL startup code validates and exposes them through `hal_virtmem_*` and `hal_physmem_*` functions (`libhal_x86/x86.c`, `libhal_riscv/riscv.c`).

## x86 pinned MMIO regions

The tracked x86 HAL adds two pinned non-RAM memory regions on top of the APXH-provided boot regions (`libhal_x86/x86.c`): PFN 0 length 1 and PFN `0xa0` length 96, both typed `APXH_REGION_MMIO` from `include/nux/apxh.h`. `hal_physmem_numregions()` includes those pinned entries, `hal_physmem_region()` returns them after the bootloader-provided region list, and `x86_init()` clears pinned non-RAM PFNs from the S-tree allocator.

This is confirmed x86 HAL behavior that Murgia's ACPI/MMIO discovery handoff depends on. Treat it as an x86 NUX/HAL contract to preserve or deliberately change with a tracked fix; it is not evidence of a generic RISC-V memory-region contract.

## PFN allocator

The default physical-page allocator is an S-tree bitmap initialized from APXH (`libnux/pfnalloc.c`).

- `stree_pfninit()` obtains the bitmap from `hal_physmem_stree()`, counts free pages, and initializes a spinlock.
- `pfn_alloc(low)` delegates to the current allocator. The default `stree_pfnalloc(low)` searches the bitmap, clears the selected bit, zeros the page through `pfn_get`, and returns a `pfn_t`.
- `pfn_free(pfn)` delegates to the current free function and increments `free_pages`.
- `nux_set_allocator()` can replace the allocator pair.

The `low` flag is used by existing x86 secondary CPU bootstrap paths that need memory below 1 MiB (`libhal_x86/i386/i386.c`, `libhal_x86/amd64/amd64.c`).

## PFN cache and direct map

`pfn_get(pfn)` gives temporary access to a physical page (`include/nux/nux.h`, `libnux/pfncache.c`).

- If the PFN is inside the HAL direct map, it returns `hal_virtmem_dmapbase() + pfn * PAGE_SIZE`.
- Otherwise it uses the PFN cache area and a small cache to map the page temporarily.
- Every non-direct `pfn_get()` should be paired with `pfn_put()`.
- PFN cache mappings use `kmap_map_noalloc()` because allocating page tables while mapping the PFN cache can deadlock (`libnux/pfncache.c`).

## KMAP

KMAP is the low-level kernel mapping layer (`libnux/kmap.c`). It asks the HAL for a leaf page-table pointer using `hal_kmap_getl1p()`, sets or clears a leaf entry with `hal_l1e_set()`, and marks kernel TLB generations dirty.

Important operations:

- `kmap_map`, `kmap_map_noalloc`, `kmap_unmap`.
- `kmap_ensure` and `kmap_ensure_range` to populate/free pages and change permissions.
- `kmap_commit()` broadcasts a kernel-map update to CPUs through `cpu_kmapupdate_broadcast()`.

The HAL comment in `include/nux/hal.h` states that KMAP mappings are static and shared across CPUs, while UMAP mappings are loadable user mappings.

## KVA

KVA is a virtual-address allocator for kernel mappings (`libnux/kva.c`). It manages a free/allocated virtual range using a red-black tree plus the generic zone allocator. The red-black tree entries are `struct vme` metadata nodes allocated from high KMEM with `kmem_alloc(0, sizeof(struct vme))`; removal must return those nodes with the matching `kmem_free()` call after unlinking them.

- `kva_alloc(size)` reserves page-rounded virtual address space.
- `kva_map(pfn, prot)` reserves one page, maps it through KMAP, commits, and returns a pointer.
- `kva_physmap(paddr, size, prot)` maps a physical range and returns a pointer with the original offset preserved.
- `kva_unmap(ptr, size)` removes mappings, commits, and frees the KVA range.

## KMEM

KMEM is the kernel heap/brk area (`libnux/kmem.c`). It has low and high growing brk pointers inside the HAL-provided KMEM range.

- `kmem_brkgrow(low, size)` grows from the low or high side.
- `kmem_alloc`/`kmem_free` allocate 64-byte aligned blocks using zone free lists and brk growth.
- `kmem_trim_setmode` and `kmem_trim_one` can unmap free pages when trimming is enabled.

`libnux/init.c` initializes memory in this order: PFN boot cache, S-tree PFN allocator, KMEM, KVA, then full PFN cache.

## UMAP and user memory

UMAP represents a user page-table set (`include/nux/types.h`, `libnux/umap.c`).

- `umap_init()` initializes a new HAL UMAP.
- `umap_bootstrap()` captures boot-time user mappings created by APXH.
- `umap_map`, `umap_unmap`, and `umap_chflags` edit user mappings and record a pending TLB operation.
- `umap_commit()` clears the pending op and flushes CPUs in the UMAP CPU mask.
- `cpu_umap_enter()` loads a UMAP into the current CPU with `hal_umap_load()`; `cpu_umap_exit()` unloads user mappings.

Callers are responsible for synchronizing concurrent access to the same UMAP; `libnux/umap.c` explicitly says it does not lock UMAP access.

`uaddr_valid()` validates one address against the HAL user interval `[hal_virtmem_userbase(), hal_virtmem_userbase() + hal_virtmem_usersize())` (`libnux/uaddr.c`). `uaddr_validrange()` validates the half-open byte range `[a, a + size)`: non-empty ranges must have their last accessed byte inside the user interval without unsigned overflow, and zero-length user-copy no-ops are accepted at addresses from the user base through one-past-user-end inclusive. User-copy helpers in `libnux/cpu.c` enable HAL user access, use `setjmp`/`longjmp` to recover from page faults, and call an optional page-fault handler.

## Current virtual layout highlights

| Architecture | User UMAP | KVA | KMEM | Direct map | PFN cache | Evidence |
| --- | --- | --- | --- | --- | --- | --- |
| i386 | 0 to `3 << L3_SHIFT` (3 GiB with PAE) | 256 MiB | 512 MiB | first 1 MiB | 1 MiB | `libhal_x86/i386/pae32.c`, `libhal_x86/i386/exe.ld`, `hal_config_i386.h` |
| amd64 | 42-bit user VA by default (`UMAP_LOG2_L4PTES=3`) | 512 GiB | 512 GiB | 512 GiB | 256 MiB | `libhal_x86/amd64/pae64.c`, `libhal_x86/amd64/exe.ld`, `hal_config_amd64.h` |
| riscv64 | 42-bit user VA by default (`UMAP_LOG2_L4PTES=3`) | 512 GiB | 512 GiB | 512 GiB | 256 MiB | `libhal_riscv/sv48.c`, `libhal_riscv/exe.ld`, `hal_config.h` |

## Memory-related backlog flags

- The historical `uctxt_seta2()` setter bug was fixed in `libnux/uctxt.c`; the helper now calls `hal_frame_seta2()` and is covered by the example kernel/user `UCTXT_SETA2` smoke check.
- The historical `libnux/uaddr.c` range-check bug is fixed: stale malformed macros were removed, `uaddr_validrange()` now checks half-open ranges without overflowing, and the example i386 smoke prints `UADDR_VALIDRANGE test passed.` after covering boundary/zero-length/overflow cases.
- The historical `libnux/kva.c` metadata-removal bug is fixed: `vmap_remove()` now frees removed `struct vme` nodes with `kmem_free()` instead of allocating another node, and the example i386 smoke prints `KVA_ALLOC_FREE test passed.` after repeated balanced KVA allocation/free churn.
- `libnux/framebuffer.c` marks RGB masks and bounds handling as incomplete.
