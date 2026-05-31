# NUX PTE and entry-hook contract audit

This note is a source-backed audit of the current NUX page-table-entry and
entry-hook contracts. It is framed as generic NUX architecture/API work:
Murgia/MH is an important downstream consumer and pressure test, but not the
only purpose of these interfaces. Other kernels and workloads, including
compute experiments such as ggml-on-NUX, should be able to rely on the same
small NUX contracts.

Two historical TODO ideas motivated this audit:

- add `ROOTPTE`/`ROOTPTEP` and `LEAFPTE`/`LEAFPTEP` names for page-table
  entries; and
- change entry hooks so handlers mutate the supplied frame/return data instead
  of returning a replacement `uctxt_t *`.

Those TODOs are treated here as uncertain historical thinking, not as an
implementation mandate. The tracked source remains authoritative. If the
current interfaces are simple and adequate for generic NUX use, the preferred
outcome is no API change. Any future change must demonstrate a generic NUX
problem or make an interface smaller, clearer, or easier to verify; it must not
be routed merely to satisfy old roadmap wording or a Murgia-specific porting
wish.

This audit also preserves the corrected HAL/platform boundary. APXH passes a
typed `struct apxh_pltdesc` to the selected HAL/platform implementation, and
that code consumes only the boot data it needs internally. This note does
**not** add or propose raw ACPI table export, ACPI table inventories,
PCIe/MCFG facts, DMAR/IVRS facts, IOMMU facts, AHCI support, filesystem work,
disk-image work, or a public platform-fact substrate. ACPI/device policy above
that boundary belongs to kernels and userspace such as Murgia.

## Source inventory

The current contract below is based on tracked source in:

- public HAL and NUX APIs: `include/nux/hal.h`, `include/nux/nux.h`,
  `include/nux/types.h`;
- KMAP/UMAP callers: `libnux/kmap.c`, `libnux/umap.c`;
- x86 page tables: `libhal_x86/pmap.c`, `libhal_x86/internal.h`,
  `libhal_x86/include/nux/hal_config*.h`, `libhal_x86/i386/pae32.c`,
  `libhal_x86/amd64/pae64.c`;
- RISC-V page tables: `libhal_riscv/pmap.c`, `libhal_riscv/internal.h`,
  `libhal_riscv/include/nux/hal_config.h`, `libhal_riscv/sv48.c`;
- entry dispatch and user contexts: `libnux/entry.c`, `libnux/uctxt.c`,
  `libnux/internal.h`, `example/kern/main.c`;
- architecture entry paths: `libhal_x86/i386/sys_entry.c`,
  `libhal_x86/amd64/frame.c`, `libhal_riscv/riscv.c`,
  `libhal_riscv/frame.c`, `libplt_acpi/plt.c`, and `libplt_sbi/sbi.c`.

## Current HAL PTE contract

### Public API shape

`include/nux/hal.h` describes two virtual-memory domains:

- KMAP: static/shared kernel mappings; and
- UMAP: user mappings that can be loaded into a CPU page table.

The public page-table API intentionally exposes the leaf/data mapping level:

- `hal_l1p_t` is a handle to a leaf PTE slot; `L1P_INVALID` is the invalid
  handle in HAL config headers.
- `hal_l1e_t` is the leaf PTE value.
- `hal_kmap_getl1p(va, alloc, &l1p)` and `hal_umap_getl1p(umap, uaddr, alloc,
  &l1p)` walk to a leaf slot, allocating intermediate page-table pages only
  when requested.
- `hal_l1e_box(pfn, flags)` and `hal_l1e_unbox(l1e, &pfn, &flags)` translate
  between NUX leaf data-page semantics and architecture PTE bits.
- `hal_l1e_get()`, `hal_l1e_set()`, and `hal_l1e_tlbop()` read/write leaf
  slots and report the TLB action needed by a change.
- `hal_umap_next()` enumerates the next non-zero user leaf entry.
- `hal_umap_free()` frees page-table memory associated with a UMAP; it does not
  free the data pages mapped by that UMAP.

The shared `HAL_PTE_*` vocabulary describes data-page leaf permissions:
present, writable, executable, user, global, accessed, dirty, and AVL bits. It
is not a root-table ownership or sharing language.

### libnux callers

`libnux/kmap.c` and `libnux/umap.c` use the API as a leaf-only contract:

- mapping paths box a data PFN plus leaf flags, write a leaf slot, and mark or
  accumulate the returned TLB operation;
- unmap paths write a zero/invalid leaf and return the previously mapped data
  PFN only if the old entry had `HAL_PTE_P`;
- `kmap_getpfn()` and `umap_unmap()` depend on `hal_l1e_unbox()` describing
  data-page presence;
- `kmap_ensure()` allocates/frees data pages based on the leaf present bit; and
- `umap_free()` delegates page-table memory release to the HAL after asserting
  that no CPU has the UMAP loaded.

No current `libnux` caller needs a public operation that names or edits a
root/intermediate page-table entry independently from a leaf data mapping.

### x86 behavior

`libhal_x86/pmap.c` implements the exported leaf operations for both i386 PAE
and amd64 PAE64:

- KMAP rejects addresses below the UMAP maximum; UMAP rejects addresses outside
  the user range.
- `hal_l1e_box()` maps NUX leaf permission bits onto x86 PTE bits. Executable
  permission is represented by absence of NX when the entry is present.
- `hal_l1e_unbox()` maps x86 PTE bits back to the shared leaf flags.
- `hal_l1e_tlbop()` skips flushing when the old entry was not present; present
  PFN changes or permission restrictions require a flush, and global entries
  require a global flush.

`libhal_x86/i386/pae32.c` walks three levels. It can return direct linear-map
leaf pointers or encoded foreign pointers for page-table pages that are not
currently mapped linearly; foreign pointers are resolved through
`pfn_get()`/`pfn_put()`. Missing intermediate tables are allocated on demand.
Root/intermediate entries are asserted not to be reserved or large-page mappings
before a leaf pointer is returned. `pt_umap_free()` frees page-table pages but
not mapped data pages.

`libhal_x86/amd64/pae64.c` walks four levels. `struct hal_umap` stores a fixed
set of user L4 entries (`UMAP_L4PTES`, currently eight, for a 42-bit user VA
window). `hal_umap_init()` allocates a table for each saved user L4 slot, so an
empty initialized UMAP may already contain present root entries before any user
data page is mapped. Load/free operations replace or release those table trees.

### RISC-V behavior

`libhal_riscv/pmap.c` and `libhal_riscv/sv48.c` implement Sv48-style page
tables:

- `hal_l1e_box()` maps `HAL_PTE_P` to a valid readable leaf (`PTE_V | PTE_R`),
  then applies write, execute, user, global, accessed, dirty, and software AVL
  bits.
- `hal_l1e_unbox()` reports a present data mapping only when the PTE is valid
  and readable. A table entry (`PTE_V` with no R/W/X bits) is therefore not a
  present data leaf.
- `hal_l1e_tlbop()` conservatively returns `HAL_TLBOP_FLUSH`.
- Sv48 walkers allocate table pages with table PTEs and assert that
  root/intermediate entries are not leaf mappings during free/bootstrap.
- As noted by source inspection, the range check in `libhal_riscv/pmap.c` uses
  `&&` between the high and low out-of-range conditions. The Sv48 walkers still
  assert the L4 offset is inside `UMAP_L4PTES`, but the shared x86-style
  boundary rule should be made explicit if that code is revisited.

### Adequacy assessment

The current leaf-only HAL API is small, source-backed, and adequate for the
callers that exist today. It keeps the public KMAP/UMAP contract focused on data
mappings and leaves architecture-specific root/intermediate table details inside
HAL walkers. That is a reasonable NUX design unless a future generic user shows
a concrete need to inspect, share, detach, or trim page-table roots.

The historical `ROOTPTE`/`LEAFPTE` wording is therefore not enough, by itself,
to justify a code slice. A future change is justified only if it solves a real
NUX problem such as:

- a generic KMAP/UMAP operation needs to distinguish table links from data-page
  leaves without reusing leaf permission semantics;
- UMAP clone/share/detach behavior needs an explicit ownership contract for
  root/intermediate page-table pages;
- partial unmap/trim needs to free empty page-table pages safely without freeing
  mapped data pages; or
- renaming/wrapping the public leaf API demonstrably reduces confusion for new
  ports without adding churn or a second parallel API.

Until such evidence exists, new ports and kernels should implement and use the
tracked `hal_l1p_t`/`hal_l1e_t` API as the authoritative contract.

## Conditional PTE design space, not an authorized migration

If a future review demonstrates one of the generic problems above, the smallest
safe design would separate data leaves from table links:

| Concept | Possible C name | Responsibility |
| --- | --- | --- |
| Leaf PTE value | `hal_leafpte_t` | Data-page mapping value with the existing `HAL_PTE_*` leaf permissions. |
| Leaf PTE pointer | `hal_leafptep_t` | Handle to a writable leaf slot; direct or foreign as the HAL requires. |
| Root/intermediate PTE value | `hal_rootpte_t` | Non-present root, page-table-page link, or explicit unsupported root state. |
| Root/intermediate PTE pointer | `hal_rootptep_t` | Handle to a writable root/intermediate slot. |

Important constraints for any such future design:

- leaf wrappers, if added, must preserve current `hal_l1e_*` behavior;
- root entries must not be boxed/unboxed as data pages;
- large-page/root-leaf states that current walkers reject should remain rejected
  or be represented as explicit unsupported states, not silently converted;
- ownership information for UMAP table pages should be explicit side metadata
  or an explicit clone/detach API, not hidden in architecture bits already used
  for other purposes;
- partial trim must clear parent roots before freeing owned child table pages,
  must never free mapped data pages, and must report the needed TLB action; and
- amd64 and riscv64 must be considered first-class with i386. amd64 is
  especially important because its UMAP root array already illustrates present
  root entries before data leaves, and riscv64 table PTEs make the
  table-vs-leaf distinction architecturally visible.

No PTE code change is authorized by this audit. If future evidence justifies a
change, route one minimal slice at a time and keep it independent from entry-hook
work.

## Current entry-hook contract

### Public API and libnux dispatch

`include/nux/nux.h` declares all kernel event hooks as returning `uctxt_t *`:

- `entry_sysc(uctxt_t *, a1, ..., a7)`;
- `entry_pf(uctxt_t *, va, hal_pfinfo_t)`;
- `entry_ex(uctxt_t *, ex)`;
- `entry_alarm(uctxt_t *)`;
- `entry_ipi(uctxt_t *)`; and
- `entry_irq(uctxt_t *, irq, level)`.

The documented rule is simple: the input context is the interrupted user
context, or `NULL`/`UCTXT_IDLE` when the event woke an idle CPU; the returned
context is the user context to restore, or `NULL`/`UCTXT_IDLE` to idle the CPU.

`libnux/entry.c` enforces this return-based contract:

- syscalls must originate from a user frame, pass seven explicit syscall words
  to `entry_sysc()`, and return `uctxt_frame(returned_uctxt)`;
- user page faults and exceptions call the kernel hook, while unexpected kernel
  faults panic and kernel user-access faults can longjmp through
  `cpu_useraccess_checkpf()` first;
- timer, IRQ, and IPI entries use `uctxt_getuser()`, allow idle-origin events,
  call the kernel hook, perform the corresponding platform EOI, and return
  `uctxt_frame()`; and
- `uctxt_frame()` returns a user frame pointer or enters `cpu_idle()` for
  `UCTXT_IDLE`.

`libnux/uctxt.c` already provides mutating helpers (`uctxt_setret()`,
`uctxt_seta0()`, `uctxt_seta1()`, `uctxt_seta2()`, `uctxt_settls()`) for the
current frame. The return-based contract therefore does not prevent handlers
from mutating return registers; it only uses the returned `uctxt_t *` to select
which context, if any, will be resumed.

### Architecture entry behavior

The architecture entry paths all expect a `struct hal_frame *` back from
`hal_entry_*`:

- i386 passes syscall number/arguments from `eax, edi, esi, ecx, edx, ebx,
  ebp`, routes page faults and other exceptions through `hal_entry_pf()` and
  `hal_entry_xcpt()`, and routes platform vectors through `plt_interrupt()`;
- amd64 passes `rax, rdi, rsi, rdx, rbx, r8, r9` and checks the returned
  RIP for canonical form before returning to user mode;
- RISC-V increments `pc` before syscall dispatch, passes `a0` through `a6`,
  derives page-fault information from `scause` and the current PTE, and checks
  pending emulated IPIs before returning; and
- ACPI and SBI platform code map timer/IPI/IRQ events to the generic
  `hal_entry_*` dispatch points, with RISC-V external interrupt dispatch still
  tracked as a separate platform gap.

### Example behavior

`example/kern/main.c` shows why the current contract is useful and small:

- normal syscalls mutate the same context and return it;
- syscall `7` writes a regression value through `uctxt_seta2()` before resuming
  the user frame;
- syscall `4097` unloads/frees the example UMAP and returns `UCTXT_IDLE`;
- `entry_ipi()` returns `&u_init`, which can switch an idle CPU to the boot user
  context; and
- alarm/IRQ hooks return the input context while page-fault/exception hooks can
  print diagnostics and idle.

### Adequacy assessment

The current entry-hook API is a compact event contract. It supports three common
outcomes with one return value: resume the interrupted context, resume another
saved context, or idle. It also already permits mutation of return registers via
`uctxt_*` helpers. The audit did not find a generic NUX bug that requires
changing this API now.

The historical mutate-input-frame TODO is therefore not enough, by itself, to
authorize an ABI change. A future action/mutate model could be reasonable only
if it demonstrably simplifies generic NUX entry handling, makes context-switch
semantics safer, or reduces architecture-specific return-path complexity without
regressing current behavior.

Any future proposal must preserve:

- the seven-word syscall argument contract visible to kernels;
- syscall return-register behavior and the `UCTXT_SETA2` regression path;
- user-access page-fault recovery before kernel hooks are called;
- panic behavior for unexpected kernel faults;
- timer/IPI/IRQ EOI ordering;
- idle-origin timer/IPI/IRQ handling; and
- i386, amd64, and riscv64 entry paths as first-class targets, with amd64 not
  treated as an afterthought.

## Conditional entry-hook design space, not an authorized migration

If future evidence justifies changing the entry ABI, the smallest candidate
shape is an explicit result/action value:

- `NUX_ENTRY_RESUME`: return using the supplied/mutated frame;
- `NUX_ENTRY_IDLE`: enter CPU idle after normal platform EOI/bookkeeping; and
- panic/fatal paths: do not return.

A context switch would copy or install a selected saved context into the
supplied return frame and return `NUX_ENTRY_RESUME`; idling would return
`NUX_ENTRY_IDLE`. Such a design would likely need a reviewed helper such as
`uctxt_copy(dst, src)`/`uctxt_install(dst, src)` and a clear way to tell a hook
whether the event originated from an idle CPU.

This is only a possible future simplification path. It should not be implemented
unless a planner/reviewer accepts a generic NUX problem statement and a tiny
slice. If it is ever implemented, migrate one entry path at a time and do not
combine it with PTE work. Syscall is the safest first candidate for argument and
return-register verification; IPI/timer is the candidate only if idle wake and
context-install semantics are the demonstrated problem.

## Current recommendation

For the current NUX source, the recommendation is conservative:

1. Treat the tracked `hal_l1p_t`/`hal_l1e_t` leaf API as the authoritative PTE
   contract for KMAP/UMAP and new ports.
2. Treat the tracked return-based `entry_* -> uctxt_t *` hooks as the
   authoritative entry contract for kernels and examples.
3. Do not start ROOTPTE/LEAFPTE alias work, root-helper work, page-table trim
   work, or entry mutate/action work just because historical TODO text exists.
4. If a future generic NUX workload demonstrates a real problem, write a small
   source-backed design for that problem first, then route one independent code
   slice with review.
5. Keep Murgia/MH requirements traceable as downstream pressure, but evaluate
   every NUX API change against generic NUX users such as other kernels and
   ggml-on-NUX-style experiments.

## Future trigger checklist

A future code slice may be worth planning only when the proposal answers all of
these questions with source-backed evidence:

- What current NUX caller or generic workload is blocked or made unsafe by the
  existing interface?
- Why is a documentation clarification or local helper insufficient?
- Why does the proposed interface reduce complexity rather than add parallel
  names and migration churn?
- Which architectures are affected, and how are i386, amd64, and riscv64 kept
  in view?
- What exact behavior must stay unchanged for current examples and userspace?
- What small verification command proves the slice, and what remains unverified?

Examples of possible future triggers, if proven, include safe UMAP
clone/share/detach semantics, a partial page-table trim operation with clear
ownership rules, or an entry return path where returning a replacement context
creates an architecture-independent correctness problem. None of those triggers
is currently proven by the historical TODO alone.

## Verification plan

For this documentation/audit slice:

```sh
git status --short --branch --untracked-files=no
git diff --check
./configure --help
```

No QEMU smoke is required for docs-only changes.

For any future code slice that changes PTE or entry behavior, keep the existing
i386 smoke green:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
BUILD=/tmp/the-nux-pte-entry-smoke-i386 \
./tools/qemu-smoke-i386.sh
```

The i386 smoke must keep the APXH/NUX/userspace markers, syscall arity markers
through `SYSC6`, `UCTXT_SETA2 test passed.`,
`UCTXT_SETA2 user test passed.`, `UADDR_MEMSET test passed.`,
`UADDR_MEMSET user test passed.`, `UADDR_VALIDRANGE test passed.`,
`KVA_ALLOC_FREE test passed.`, and `User exited with error code: 42`.

Because NUX is not an i386-only substrate, any future implementation plan must
also explain the amd64 and riscv64 verification path. If target toolchains or
QEMU paths are not yet reviewed for those architectures, record that gap rather
than treating i386-only smoke as full architectural coverage.

## Non-goals

- No Murgia repository changes.
- No raw ACPI/DTB export or platform-fact API.
- No ACPI table inventories, PCIe/MCFG, DMAR/IVRS, IOMMU, AHCI, filesystem, or
  disk-image work.
- No PTE API change unless a generic NUX problem is demonstrated.
- No entry-hook ABI change unless a generic NUX problem is demonstrated.
- No combined PTE/entry migration slice.
