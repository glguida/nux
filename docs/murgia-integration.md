# Murgia integration

Murgia depends on NUX. This file tracks only concrete requirements or roadmap dependencies that arrive through durable task comments, issue/job handoffs, or explicitly labeled source-inspection notes. Do not invent broader Murgia requirements from this list.

## Current recorded requirements

| Requirement ID | Source | NUX capability needed | Status | Notes |
| --- | --- | --- | --- | --- |
| `MURGIA-MH-001` | Task log comments `2026-05-29T19:19:31Z` and `2026-05-29T19:34:18Z` on task `the-nux-docs-capabilities`; the source note is the untracked base-checkout file `/home/glguida/the_nux/TODO`, not tracked source evidence. | HAL root/leaf page-table abstractions (`ROOTPTE`, `ROOTPTEP`, `LEAFPTE`, `LEAFPTEP`) plus an entry-hook contract where handlers mutate the input frame/return data instead of returning a replacement `uctxt_t *`. | Task-log roadmap item / not implemented. | The note links these changes to enabling a basic Murgia/MH port. Current tracked source still exposes leaf `hal_l1p_t`/`hal_l1e_t` APIs and return-based `entry_*` hooks, so this row is a design dependency, not an implemented capability. Preserve/analyze `/home/glguida/the_nux/PORTING_0_EM` as a base-checkout ELF/binary artifact, not editable documentation. |

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

The row above is the only concrete Murgia/MH roadmap dependency currently recorded. Other NUX capability areas Murgia may need to reference later are:

- Build and toolchain reproducibility (`docs/build-and-run.md`).
- Architecture/boot/platform matrix (`docs/hardware-support.md`).
- User/kernel syscall and page-fault model (`docs/userspace.md`).
- PFN/KVA/KMAP/UMAP memory model (`docs/memory.md`).
- Debugging and panic output (`docs/debugging.md`).

Do not treat the generic areas list as a commitment until Murgia records concrete needs.
