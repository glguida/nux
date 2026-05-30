# Backlog and capability roadmap

This backlog is source-inspected only unless a verification result, task-log item, or base-checkout artifact is explicitly labeled.

## P0: build and documentation blockers

1. **Host build-tool baseline for this container is resolved.**
   - Historical evidence: the initial documentation pass failed `../configure ARCH=i386` in `build-docs-check` with `no acceptable C compiler found in $PATH`.
   - Current reviewed status: task-log comments `2026-05-29T19:56:57Z` and `2026-05-29T19:58:36Z` record apt installation and review of the normal host baseline (`build-essential`, `autoconf`, `automake`, `file`, plus GCC/G++, Make, and binutils dependencies). `./configure --help` passes.
   - Follow-up trigger: if this container is rebuilt or the host baseline disappears, reinstall/reverify those packages. Do not treat the old missing-host-compiler result as the current first blocker.
2. **Stable `i686-unknown-elf` target toolchain path is resolved for this task workspace.**
   - Historical blocker: after host tools were installed, fresh `/tmp` `ARCH=i386` configure probes passed host compiler checks and failed at `i686-unknown-elf-gcc not found`.
   - Provenance: task-log comments `2026-05-29T20:15:25Z`, `2026-05-29T20:18:36Z`, and `2026-05-29T20:20:57Z` record that the README-recommended `gcc_toolchain_build` source at commit `eecef0929616a96517a83dab988a8429ad8c62d8` produced `i686-unknown-elf-{gcc,ld,ar,objcopy}` under `/tmp/the-nux-i386-target-toolchain-gcc_toolchain_build/install/bin`. With that temporary path prepended to `PATH`, fresh i386 configure/build passed.
   - Current reviewed status: that reviewed install tree was copied into stable task path `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf`; the documented `TOOLBIN` is `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin`. Stable `TOOLBIN` verification found GCC 14.2.0 and Binutils 2.43.1 `ld`/`ar`/`objcopy`.
   - Follow-up trigger: if the task-workspace toolchain is removed, corrupted, or needs to be recreated outside this instance, rebuild or recopy from the documented `gcc_toolchain_build` commit. Do not commit the toolchain into this repository.
3. **i386 QEMU runtime smoke blocker is resolved in this container.**
   - Historical blocker: the reviewed target-toolchain slice said `make qemu` failed at `qemu-system-i386: No such file or directory`; `qemu-system-x86_64` was also absent.
   - Current reviewed status: apt package `qemu-system-x86` is installed with `--no-install-recommends`, providing `/usr/bin/qemu-system-i386` and `/usr/bin/qemu-system-x86_64` at QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`.
   - Verification: fresh `/tmp/the-nux-i386-qemu-stable-path-build-i386` with stable `TOOLBIN` prepended to `PATH` passed `ARCH=i386` configure, `make -j"$(nproc)"`, and a bounded `timeout --foreground 20s make qemu`. The command returned rc 124 only after serial success markers appeared: `APXH started.`, `NUX library (nux)`, userspace hello, `SYSC0`/`SYSC6` passed, and `User exited with error code: 42`. Later source fixes repeated the same flow and added regression markers: the `uctxt_seta2()` fix added `UCTXT_SETA2 test passed.` plus `UCTXT_SETA2 user test passed.`, the `uaddr_validrange()` fix added `UADDR_VALIDRANGE test passed.`, and the KVA metadata-removal fix added `KVA_ALLOC_FREE test passed.`.
   - Current harness: `tools/qemu-smoke-i386.sh` now runs the reviewed out-of-tree i386 configure/build/QEMU flow with a configurable `TOOLBIN`, captures serial output, and treats timeout rc 124 as a pass only after the required APXH/NUX/userspace/regression markers appear.
   - Follow-up trigger: integrate the checked-in harness into CI or extend it for other architectures after their target-toolchain/QEMU paths are reviewed.
4. **Submodules are initialized and verified for the i386 smoke path in this task workspace.**
   - Evidence: `.gitmodules` lists `contrib/gnu-efi`, `contrib/binutils`, and `contrib/dtc`; the `uctxt_seta2()` worktree initially showed them uninitialized with leading `-`.
   - Current i386 status: `git submodule update --init --recursive` in `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/worktree-uctxt-seta2` checked out the pinned submodule commits, after which the fresh i386 configure/build/QEMU smoke passed with the stable `TOOLBIN`.
   - Follow-up trigger: verify amd64/riscv64 submodule-dependent paths separately. The current build invokes `contrib/binutils` configure/make in the source submodule, so dedicated verification worktrees may need cleanup of generated files after builds; do not clean the dirty base checkout without authorization.
5. **APXH configure architecture selection is fixed.**
   - Historical evidence: `apxh/configure.ac` was missing a separator between the `amd64` and `riscv64` `AS_CASE` branches; generated `apxh/configure` showed a malformed amd64 branch and had an invalid-architecture error message that omitted `riscv64`.
   - Current status: `apxh/configure.ac` and the regenerated `apxh/configure` select `multiboot` for `i386`, `multiboot efi` for `amd64`, and `sbi efi` for `riscv64`; invalid architecture messages list `i386, amd64, riscv64`.
   - Follow-up trigger: full `amd64` and `riscv64` configure/build/QEMU verification still needs real target-toolchain paths; this fix only verifies APXH subdir selection with generated-script probes.
6. **`--disable-werror` handling in configure inputs is fixed.**
   - Historical evidence: top-level, APXH, and example `configure.ac` defined `AC_ARG_ENABLE([werror])` but tested `enable_relax` rather than `enable_werror`.
   - Current status: top-level, APXH, and example configure inputs now test `enable_werror`; the generated `configure` scripts were regenerated and verified so default configure keeps `-Werror` while `--disable-werror` omits it from generated build flags.
   - Follow-up trigger: if new configure inputs are added, keep the `AC_ARG_ENABLE([werror])` variable and generated scripts in sync.
7. **README boot-support claims are reconciled with configure behavior.**
   - Historical evidence: README said APXH supported EFI on i386, amd64, and riscv64 even though the APXH configure selection is `multiboot` only for i386, `multiboot efi` for amd64, and `sbi efi` for riscv64.
   - Current status: README now describes configured APXH boot paths by architecture: i386 -> `multiboot`, amd64 -> `multiboot efi`, and riscv64 -> `sbi efi`. It also states that the verified task path is i386/multiboot and that non-i386 build/runtime flows plus the RISC-V EFI platform contract remain unverified.
   - Follow-up trigger: reopen only if configure behavior changes or target-toolchain/runtime verification proves a different support claim. Full amd64/riscv64 verification remains tracked in the architecture-specific follow-ups.

## P1: correctness and runtime capability gaps

1. **Audit HAL PTE naming/ownership before changing the API.**
   - Evidence: task-log comment `2026-05-29T19:19:31Z` records an untracked base-checkout `/home/glguida/the_nux/TODO` note about `ROOTPTE`/`ROOTPTEP` and `LEAFPTE`/`LEAFPTEP`; planner comment `2026-05-29T19:34:18Z` required documenting it. This is historical task-log/base-checkout evidence, not a tracked-source mandate.
   - Current behavior: tracked `include/nux/hal.h` exposes leaf page-table pointer/entry APIs (`hal_l1p_t`, `hal_l1e_t`, `hal_kmap_getl1p()`, `hal_umap_getl1p()`, and `hal_l1e_*()`), and `libnux/kmap.c`/`libnux/umap.c` use those leaf entries for kernel and user data mappings. x86 and RISC-V walkers keep root/intermediate table details inside the HAL.
   - Audit status: `docs/nux-pte-entry-contracts.md` treats the old root/leaf wording as uncertain historical thinking. The current leaf-only contract is simple and adequate for current generic NUX callers; Murgia/MH is a downstream pressure test, not the only purpose of the API.
   - Follow-up trigger: no PTE code slice is authorized by the TODO alone. Reopen only for a demonstrated generic NUX problem, such as safe UMAP root-table ownership/share/detach semantics, partial page-table trimming that cannot be expressed with the current API, or a proven simplification that avoids parallel-name churn. Any proposal must keep amd64 especially important and riscv64 first-class with i386.
2. **Audit entry-hook return semantics before changing the ABI.**
   - Evidence: task-log comment `2026-05-29T19:19:31Z` records the base-checkout TODO note: “all entry function should not return a frame. Frame in input should be modified with return data.” Planner comment `2026-05-29T19:34:18Z` tied this to the required docs fix. This is historical input, not an implementation mandate.
   - Current behavior: `include/nux/nux.h` declares `entry_sysc()`, `entry_pf()`, `entry_ex()`, `entry_alarm()`, `entry_ipi()`, and `entry_irq()` as returning `uctxt_t *`; `libnux/entry.c` assigns those return values and converts them with `uctxt_frame()`; `libnux/uctxt.c` handles `UCTXT_IDLE`, `UCTXT_INVALID`, frame-pointer conversion, and register mutation helpers.
   - Audit status: `docs/nux-pte-entry-contracts.md` records the current return-based hook contract as authoritative and adequate for current NUX examples. It already supports mutating return registers, resuming a different saved context, or idling the CPU.
   - Follow-up trigger: no mutate-input-frame/action ABI slice is authorized by the TODO alone. Reopen only if a source-backed generic NUX problem shows that an action/result model would make entry handling smaller, safer, or easier to verify while preserving syscall arity, `UCTXT_SETA2`, user-access page-fault recovery, EOI ordering, idle wake behavior, and i386/amd64/riscv64 coverage.
3. **`uctxt_seta2()` setter bug is fixed.**
   - Historical evidence: `libnux/uctxt.c` called `hal_frame_seta1(f, a2)` inside `uctxt_seta2()`.
   - Current status: `uctxt_seta2()` now calls `hal_frame_seta2()`. The example kernel/user smoke includes syscall `7`, where the kernel uses `uctxt_seta2()` to write a known magic value into the resumed user frame's third argument register and userspace verifies it after the syscall returns.
   - Follow-up trigger: keep the `UCTXT_SETA2` serial markers in the i386 QEMU smoke output and in `tools/qemu-smoke-i386.sh` unless the regression is replaced by a stronger checked-in test.
4. **User-address range validation is fixed.**
   - Historical evidence: `libnux/uaddr.c` had unused malformed macros and `uaddr_validrange()` validated `a + size` rather than the last accessed byte; overflow/zero-length behavior was undocumented.
   - Current status: the stale macros are removed. `uaddr_validrange(a, size)` treats user ranges as half-open intervals `[a, a + size)` contained in `[hal_virtmem_userbase(), hal_virtmem_userbase() + hal_virtmem_usersize())`; for non-empty ranges it checks the last accessed byte without overflowing, so a range ending exactly at the user-region end is valid and a one-past-end non-empty range is invalid. Empty ranges are accepted for no-op user copies at any address from the user base through one-past-user-end inclusive. `uaddr_valid(a)` remains the single-address `[base, end)` check.
   - Verification: the example kernel i386 smoke now asserts first-byte validity, full `[base, end)` validity, one-past-end non-empty rejection, oversized/overflow rejection, and zero-length no-op acceptance, then prints `UADDR_VALIDRANGE test passed.`.
   - Follow-up trigger: keep the `UADDR_VALIDRANGE` marker covered by `tools/qemu-smoke-i386.sh` unless the regression is replaced by a stronger checked-in test.
5. **KVA allocator metadata removal is fixed.**
   - Historical evidence: `libnux/kva.c` `vmap_insert()` allocates each `struct vme` metadata node with `kmem_alloc(0, sizeof(struct vme))`, but `vmap_remove()` removed the node from the red-black tree and then called `kmem_alloc(0, sizeof(struct vme))` again instead of releasing the removed node.
   - Current status: `vmap_remove()` now frees the removed metadata with `kmem_free(0, (vaddr_t) vme, sizeof(struct vme))` after unlinking it and decrementing `vmap_size`, preserving the existing KVA lock/tree/zone flow. The example i386 smoke repeatedly allocates and frees KVA ranges, checks that the high KMEM brk is unchanged across the balanced KVA churn, and prints `KVA_ALLOC_FREE test passed.`.
   - Follow-up trigger: keep the `KVA_ALLOC_FREE` marker covered by `tools/qemu-smoke-i386.sh` unless the regression is replaced by a stronger checked-in test.
6. **Complete or document RISC-V SMP support.**
   - Evidence: `libhal_riscv/riscv.c` has TODOs in `hal_pcpu_init()` and `hal_pcpu_startaddr()` and returns `PADDR_INVALID` for secondary start.
   - Next slice: decide whether SBI HSM or another start mechanism should be used.
7. **Complete or document RISC-V external IRQ/PLIC support.**
   - Evidence: `libplt_sbi/sbi.c` TODOs cover IRQ type, enable/disable/max, EOI, platform CPU enter/start, and external interrupt dispatch.
   - Next slice: wire PLIC contexts discovered from DTB to `plt_interrupt()` and IRQ APIs.
8. **Define the RISC-V EFI platform contract.**
   - Evidence: APXH EFI has RISC-V entry code but returns `PLT_ACPI`; the configured RISC-V platform library requires `PLT_DTB`.
   - Next slice: decide whether RISC-V EFI should use ACPI, DTB handoff, or be disabled until supported.
9. **Add x86 SMEP/user-access hardening.**
   - Evidence: `libhal_x86/x86.c` has TODOs in `hal_useraccess_start()` and `hal_useraccess_end()`.
   - Next slice: implement CR4.SMEP/SMAP-aware behavior or explicitly document unsupported CPU hardening.
10. **Improve framebuffer correctness.**
    - Evidence: `libnux/framebuffer.c` has XXX comments for RGB masks, bounds checking, and rewrite need.
    - Next slice: honor framebuffer masks and clamp writes; add a QEMU visual/serial smoke check.
11. **Clarify i386 TLS support.**
    - Evidence: `libhal_x86/i386/sys_entry.c` states `hal_frame_settls()` is ignored because i386 TLS needs LDT support.
    - Next slice: document as unsupported or add LDT/TLS support.
12. **ACPI/x86 hardware expansion.**
    - Evidence: `libplt_acpi/acpi.c` ignores LSAPIC, x2APIC, IOSAPIC, and LX2APICNMI entries.
    - Next slice: prioritize x2APIC if modern hardware support is a near-term goal.
13. **Track the Murgia modern-hardware boundary without a NUX ACPI fact substrate.**
    - Evidence: task-log comment `2026-05-29T21:51:15Z` records the Murgia/MH design constraint that IOMMU support should stay transparent beneath the existing `hwdev`/`sys_export`/`dexport` device/export semantics. The corrected source-backed inventory in `docs/murgia-substrate-roadmap.md` records current NUX capabilities, gaps, and the approved APXH typed-platform-pointer/HAL boundary.
    - Current behavior: tracked x86 platform code scans ACPI RSDP/RSDT/XSDT internally for MADT and HPET, then initializes LAPIC, IOAPIC, and HPET support. Public NUX primitives already include PFN/KVA/KMAP/UMAP and `kva_physmap()` for CPU-side physical/MMIO mappings. A source/doc search found no current PCI bus enumeration, ACPI MCFG/PCIe ECAM discovery, MSI/MSI-X support, Intel DMAR or AMD IVRS parsing, IOMMU abstraction, DMA-remapping map/unmap API, AHCI/storage driver, filesystem, or real-disk-image QEMU harness.
    - Design constraint: APXH passes `struct apxh_pltdesc` (`type`, `pltptr`) to the selected HAL/PLT, and that code may consume the needed ACPI or DTB boot data internally. NUX must not export raw ACPI tables, ACPI table inventories, MCFG/ECAM records, DMAR/IVRS records, or public platform facts for Murgia. Murgia/kernel/userspace owns ACPI parsing, PCIe/MCFG interpretation, IOMMU/device policy, AHCI, and filesystem decisions above that boundary.
    - Priority order:
      1. Preserve and document the typed platform descriptor boundary; test only that selected platform libraries receive and use their expected descriptor internally.
      2. Keep x86 APIC/IOAPIC/HPET/x2APIC and RISC-V PLIC/timer work inside the platform/HAL abstraction instead of publishing hardware-description inventories.
      3. Route Murgia PCIe/MCFG/IOMMU/AHCI/filesystem policy to Murgia/kernel/userspace; do not make it depend on NUX ACPI export.
      4. Specify any future generic NUX memory, interrupt, or DMA helper as a separate reviewed API with concrete acceptance criteria and no raw ACPI table export.
      5. Preserve i386 QEMU harness coverage; add amd64/riscv64 and later disk-image harnesses only as bounded tasks after their boot paths and ownership boundaries are verified.
    - Next slice: no ACPI/platform-facts implementation slice is authorized. If future work is needed, start with an explicitly scoped internal HAL/platform test or a Murgia-owned device-policy task.

## P2: usability, tests, and polish

1. **Preserve and analyze `PORTING_0_EM` as a binary artifact, not documentation.**
   - Evidence: task-log comment `2026-05-29T19:19:31Z` identifies `/home/glguida/the_nux/PORTING_0_EM` as a base-checkout binary/ELF artifact to preserve. A local magic-byte check in this fix job read `7f454c46` (`ELF`) and size `303904` bytes; `file(1)` was unavailable in the container.
   - Next slice: if analysis is authorized, inspect it with appropriate binary tools (`readelf`, `objdump`, or equivalent) and record metadata separately. Do not edit it, delete it, or treat it as Markdown/source documentation.
2. **Checked-in automated i386 smoke harness is implemented.**
   - Evidence: `tools/qemu-smoke-i386.sh` runs from a source checkout/worktree, uses an out-of-tree build directory (defaulting under `/tmp`, overridable with `BUILD` or `NUX_BUILD`), prepends `TOOLBIN` when provided, runs `configure ARCH=i386`, `make`, and bounded `make qemu`, captures QEMU serial output, and verifies the reviewed APXH/NUX/userspace/regression markers before accepting timeout rc 124.
   - Follow-up trigger: add CI wiring, GDB/debug variants, or analogous amd64/riscv64 smoke harnesses only after those architectures have reviewed toolchain/QEMU paths.
3. **Add GDB/debugging helpers.**
   - Evidence: QEMU debug target exists but no GDB scripts were found.
   - Next slice: add docs or scripts for loading symbols and connecting to `:1234`.
4. **Document syscall ABI stability.**
   - Evidence: `libnux_user` wraps syscalls, but syscall numbers are example-local (`4096` putchar, `4097` exit in `example/kern/main.c`/`example/user/main.c`).
   - Next slice: either publish a minimal NUX syscall convention or explicitly state that kernels own their syscall ABI.
5. **Keep Murgia requirements traceable and triaged.**
   - Evidence: `docs/murgia-integration.md` records task-log-backed `MURGIA-MH-001`, the `2026-05-29T19:53:34Z` Murgia handoff rows for the `entry_sysc` arity contract, x86 PFN-0/MMIO-region behavior, and `UIOMAP`/`IOUNMAP` errno semantics, plus `MURGIA-IOMMU-006` from the `2026-05-29T21:51:15Z` IOMMU transparency constraint and the corrected `MURGIA-SUBSTRATE-007` boundary note from `docs/murgia-substrate-roadmap.md`.
   - Next slice: for confirmed NUX contracts, add compile/build checks or source comments when useful; for Murgia-side dependency candidates, wait for concrete NUX acceptance criteria before changing APIs.
6. **Clean README and install docs.**
   - Evidence: README has minor typos and a malformed closing fence in the build snippet; `install.sh` contains only a TODO comment.
   - Next slice: fix README after build commands are verified and either implement or remove/document `install.sh`.

## Source TODO/FIXME scan summary

A tracked-file grep excluding `contrib` found TODO/XXX items in:

- `install.sh`
- `apxh/efi/efi-main.c`
- `libhal_riscv/riscv.c`
- `libhal_x86/i386/i386.c`
- `libhal_x86/x86.c`
- `libnux/framebuffer.c`
- `libnux/kmem.c`
- `libnux/kva.c`
- `libplt_acpi/lapic.c`
- `libplt_sbi/sbi.c`

Generated `configure` files and imported `libec` headers also contain generic FIXME/XXX/panic/assert strings; those are lower signal than the project-specific items above.
