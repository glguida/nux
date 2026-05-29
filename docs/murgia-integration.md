# Murgia integration

Murgia depends on NUX, but this task did not include concrete Murgia requirements beyond that dependency. This file is therefore a traceability placeholder, not an invented requirements list.

## Current recorded requirements

| Requirement ID | Source | NUX capability needed | Status | Notes |
| --- | --- | --- | --- | --- |
| _None yet_ | _No Murgia-specific requirement was present in this task_ | _N/A_ | _N/A_ | Add rows when requirements arrive through durable task comments, issues, or handoff jobs. |

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

These are not Murgia requirements; they are NUX capability areas Murgia may need to reference later:

- Build and toolchain reproducibility (`docs/build-and-run.md`).
- Architecture/boot/platform matrix (`docs/hardware-support.md`).
- User/kernel syscall and page-fault model (`docs/userspace.md`).
- PFN/KVA/KMAP/UMAP memory model (`docs/memory.md`).
- Debugging and panic output (`docs/debugging.md`).

Do not treat this list as a commitment until Murgia records concrete needs.
