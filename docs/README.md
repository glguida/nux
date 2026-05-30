# NUX documentation index

This `docs/` tree is the first source-backed documentation baseline for the current `the_nux` repository. It intentionally separates what the tracked source implements today from gaps, TODOs, and roadmap items.

## Main topics

- [Architecture](architecture.md): component boundaries among APXH, HAL, platform libraries, `libnux`, `libec`, `libnux_user`, tools, examples, and generated build files.
- [Build and run](build-and-run.md): bootstrap/configure/build flows, toolchain and submodule requirements, QEMU targets, and verification results from this documentation pass.
- [Porting](porting.md): source and build files to update when adding an architecture, boot path, platform, device, or userspace ABI support.
- [Hardware support](hardware-support.md): current architecture/boot/platform matrix and explicit hardware gaps.
- [Debugging](debugging.md): QEMU debug entry points, logging, panic/crash output, symbol generation, and performance counters.
- [Memory model](memory.md): PFN, PFN cache, KVA, KMAP, KMEM, UMAP, and user address handling.
- [Userspace](userspace.md): boot-time user payloads, syscall wrappers, user contexts, page-fault hooks, and example code.
- [Murgia integration](murgia-integration.md): traceable cross-project requirements, including the current task-log-backed Murgia/MH roadmap, corrected HAL/platform boundary, and follow-up handoff items.
- [Murgia hardware boundary roadmap](murgia-substrate-roadmap.md): source-backed matrix of NUX capabilities, gaps, and the rule that Murgia modern-hardware/AHCI/filesystem work must not depend on NUX exporting ACPI tables or a public platform-fact inventory.
- [NUX PTE and entry-hook contract audit](nux-pte-entry-contracts.md): generic source-backed audit of the current HAL leaf-PTE and return-based entry-hook contracts, with Murgia/MH and ggml-on-NUX-style workloads treated as downstream pressure tests rather than implementation mandates.
- [Backlog](backlog.md): prioritized bugs, TODOs, documentation gaps, hardware gaps, and candidate small implementation slices.

## Evidence policy

Claims in these pages cite tracked repository files such as `configure.ac`, `Makefile.in`, `apxh/*`, `include/nux/*.h`, `libnux/*`, `libhal_x86/*`, `libhal_riscv/*`, `libplt_acpi/*`, `libplt_sbi/*`, `libnux_user/*`, `example/*`, and `tools/*`. When behavior is inferred from source but not verified in this container, it is called out as inferred.
