# Murgia integration

Murgia depends on NUX. This file tracks only concrete requirements or roadmap dependencies that arrive through durable task comments, issue/job handoffs, or explicitly labeled source-inspection notes. Do not invent broader Murgia requirements from this list.

## Current recorded requirements

| Requirement ID | Source | NUX capability needed | Status |
| --- | --- | --- | --- |
| `MURGIA-MH-001` | Task log comments `2026-05-29T19:19:31Z` and `2026-05-29T19:34:18Z` on task `the-nux-docs-capabilities`; the source note is the untracked base-checkout file `/home/glguida/the_nux/TODO`, not tracked source evidence. | HAL root/leaf page-table abstractions (`ROOTPTE`, `ROOTPTEP`, `LEAFPTE`, `LEAFPTEP`) plus an entry-hook contract where handlers mutate the input frame/return data instead of returning a replacement `uctxt_t *`. | Task-log roadmap item / not implemented. |
| `MURGIA-SYSC-002` | Durable Murgia handoff in task log comment `2026-05-29T19:53:34Z`; tracked NUX evidence in `include/nux/nux.h`, `libnux/entry.c`, and `example/kern/main.c`. | Keep the current `entry_sysc` ABI explicit: it returns `uctxt_t *` and takes `uctxt_t *` followed by seven `unsigned long` syscall words `a1` through `a7`. | Confirmed tracked NUX behavior / API contract to preserve or migrate deliberately. |
| `MURGIA-MMIO-003` | Durable Murgia handoff in task log comment `2026-05-29T19:53:34Z`; tracked NUX evidence in `include/nux/apxh.h`, `libhal_x86/x86.c`, and x86 ACPI table mapping code in `libplt_acpi/*`. | Keep x86 PFN-0/MMIO-region behavior visible as a NUX/HAL contract for ACPI/MMIO discovery, or track a NUX fix if the behavior changes. | Confirmed x86 HAL behavior; possible public contract. |
| `MURGIA-UIOMAP-004` | Durable Murgia handoff in task log comment `2026-05-29T19:53:34Z`; tracked NUX evidence is currently negative/adjacent: no `UIOMAP`/`IOUNMAP` API in this repository, kernel-defined syscall policy in `docs/userspace.md`, and UMAP helpers in `libnux/umap.c`. | Track Murgia's absent/non-IOMAP mapping errno semantics as a Murgia-side dependency candidate with possible NUX/HAL implications. | Murgia-side concern / NUX impact not yet specified. |
| `MURGIA-BUILD-005` | Root coordinator priority in task log comment `2026-05-29T20:24:17Z`; reviewed i386 QEMU smoke comments `2026-05-29T20:32:04Z` and `2026-05-29T20:35:00Z`; stable-path verification in the i386 docs/toolchain follow-up. | Keep a repeatable NUX i386 configure/build/QEMU smoke path usable for downstream Murgia build/run work. The documented current path prepends `TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin` and uses the installed x86 QEMU package. | Verified in this container; environmental build/run capability only, not a substitute for the source/API roadmap items above. |

### Notes on recorded requirements

- `MURGIA-MH-001`: The note links these changes to enabling a basic Murgia/MH port. Current tracked source still exposes leaf `hal_l1p_t`/`hal_l1e_t` APIs and return-based `entry_*` hooks, so this row is a design dependency, not an implemented capability. Preserve/analyze `/home/glguida/the_nux/PORTING_0_EM` as a base-checkout ELF/binary artifact, not editable documentation.
- `MURGIA-SYSC-002`: In the tracked header, the seven syscall words are after the `uctxt_t *` parameter. `libnux/entry.c` passes all seven to the kernel hook; the example kernel treats `a1` as a syscall number and `a2` through `a7` as up to six syscall arguments. If NUX changes this signature, Murgia needs an explicit migration path rather than another implicit arity mismatch.
- `MURGIA-MMIO-003`: `libhal_x86/x86.c` appends pinned non-RAM regions to the x86 HAL memory-region list: PFN 0 length 1 and PFN `0xa0` length 96, both typed `APXH_REGION_MMIO`. During x86 initialization those pinned non-RAM regions are also cleared from the S-tree allocator. This is x86-specific tracked behavior, not a generic RISC-V contract.
- `MURGIA-UIOMAP-004`: The tracked NUX source has UMAP/KMAP primitives and kernel-defined syscall handlers, but no global errno policy for `UIOMAP`/`IOUNMAP`. Do not claim NUX implements those errno semantics until Murgia supplies concrete NUX-side acceptance criteria or NUX adds a matching API.
- `MURGIA-BUILD-005`: The i386 path is now a practical downstream enabler: stable `TOOLBIN` resolves the reviewed i686 target toolchain, `/usr/bin/qemu-system-i386` is installed, and a bounded QEMU smoke reaches userspace and exits with the expected code before the idle timeout. Murgia jobs should still treat this as a container/task-workspace path and should not assume the target toolchain is committed into NUX.

## How to record Murgia requirements

When Murgia needs a NUX capability, record it in a durable handoff with:

- source project/instance and task ID,
- requester and date,
- required NUX behavior,
- target architecture/platform/hardware,
- whether it is needed for build, boot, memory, userspace, syscalls, interrupts, timers, debugging, or performance,
- acceptance criteria and suggested verification,
- links or paths to Murgia code that depends on the capability,
- whether the requirement is blocking Murgia or only a roadmap item.

Then add a row to the table above and create or update NUX backlog items in `docs/backlog.md`.

## Current NUX areas likely relevant to Murgia

The table above is the current concrete Murgia/MH requirement set recorded in this task. It now includes both implemented/observed contracts (`entry_sysc` arity and x86 pinned MMIO regions) and dependency candidates that still need NUX-side acceptance criteria (`UIOMAP`/`IOUNMAP` errno semantics and the broader Murgia/MH page-table/entry-hook roadmap). Other NUX capability areas Murgia may need to reference later are:

- Build and toolchain reproducibility (`docs/build-and-run.md`), including the current stable i386 `TOOLBIN` and QEMU smoke path for downstream Murgia build/run work.
- Architecture/boot/platform matrix (`docs/hardware-support.md`).
- User/kernel syscall and page-fault model (`docs/userspace.md`).
- PFN/KVA/KMAP/UMAP memory model (`docs/memory.md`).
- Debugging and panic output (`docs/debugging.md`).

Do not treat the generic areas list as a commitment until Murgia records concrete needs.
