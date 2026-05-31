# Porting guide

This is a source-oriented checklist for adding or extending NUX support. It is based on the current `i386`, `amd64`, and `riscv64` paths.

## Decide what is being ported

NUX separates three concerns that are easy to mix up:

1. **Architecture/HAL support**: CPU instructions, page tables, interrupt frames, entry/return, TLB operations, and virtual-memory layout (`include/nux/hal.h`, `libhal_x86/*`, `libhal_riscv/*`).
2. **Boot/APXH support**: firmware or boot-protocol entry, payload discovery, memory map collection, page-table creation, and `apxh_bootinfo` production (`apxh/*`, `include/nux/apxh.h`).
3. **Platform support**: hardware discovery and runtime device model for CPUs, interrupts, IPIs/NMIs, timers, and IRQ EOI (`include/nux/plt.h`, `libplt_acpi/*`, `libplt_sbi/*`). APXH passes a typed platform descriptor (`struct apxh_pltdesc` with `type` and `pltptr`) to this layer; the descriptor is consumed internally and must not become a raw ACPI/DTB export API.

A new board may only need a platform extension; a new CPU ISA needs all three plus userspace syscall wrappers.

## Add or extend an architecture

Update build selection first:

- Add the new `ARCH` case to top-level `configure.ac` with `tool_prefix`, `hal`, and `plt`.
- Add generated-file outputs to `AC_MK_CONFIG_FILES` and `AC_CONFIG_FILES` if the new HAL/PLT has different files.
- Add `Makefile.in`, `hal.mk.in`, source files, headers under a new HAL directory or under an existing one.
- Update `libnux_user/Makefile.in` and add `libnux_user/<arch>/arch_syscalls.h` if userspace should run.
- Add or update `example/user/<arch>/crt0.S` for a userspace entry stack/global-pointer setup.

Implement the HAL contract from `include/nux/hal.h`:

- CPU I/O/trap/cycles/idle/halt/TLB operations.
- User-access enable/disable for safe kernel copies from user memory.
- Physical and virtual memory descriptions from APXH bootinfo.
- Current leaf PTE APIs: `hal_kmap_getl1p`, `hal_umap_getl1p`, `hal_l1e_*`, and UMAP load/init/free/next. The generic PTE audit treats historical `ROOTPTE`/`LEAFPTE` wording as uncertain, not an implementation mandate; implement new ports against the tracked `hal_l1p_t`/`hal_l1e_t` contract unless a future reviewed generic NUX problem changes that API (see `docs/nux-pte-entry-contracts.md`).
- PCPU init/add/enter/startaddr, per-CPU data, and secondary CPU entry.
- `struct hal_frame` plus all frame getters/setters and printing.
- Trap/syscall/page-fault/IRQ dispatch into `hal_entry_*`. Current NUX entry hooks return a `uctxt_t *`; the generic entry-hook audit treats the mutate-input-frame idea as conditional future design space, not a tracked API change (see `docs/nux-pte-entry-contracts.md`).
- Panic output.

Evidence examples:

- x86 interface implementation: `libhal_x86/x86.c`, `libhal_x86/pmap.c`, `libhal_x86/i386/sys_entry.c`, `libhal_x86/amd64/frame.c`.
- RISC-V interface implementation: `libhal_riscv/riscv.c`, `libhal_riscv/pmap.c`, `libhal_riscv/sv48.c`, `libhal_riscv/entry.S`.

## Add or extend a boot path

APXH must eventually provide the common APXH contract consumed by HAL:

- `struct apxh_bootinfo` with magic, max PFNs, region count, optional user entry, framebuffer, platform descriptor, and TLS info (`include/nux/apxh.h`). The embedded `struct apxh_pltdesc` carries only a typed pointer such as `PLT_ACPI`/RSDP or `PLT_DTB`/DTB for the selected HAL/PLT to consume internally.
- A memory-region list (`PHT_APXH_REGIONS`).
- An S-tree allocation bitmap (`PHT_APXH_STREE`).
- Optional framebuffer (`PHT_APXH_FRAMEBUF`).
- Initial mappings requested by linker-script program headers (`libhal_x86/*/exe.ld`, `libhal_riscv/exe.ld`).

Files to update:

- `apxh/configure.ac`: select boot subdirs for the target `ARCH`.
- `apxh/Makefile.in` and boot-subdir `Makefile.in`.
- Common loader code only if a new APXH program-header behavior is needed (`apxh/src/main.c`, `apxh/src/elf.c`, `apxh/src/project.h`).
- Machine-dependent boot code, following `apxh/multiboot`, `apxh/efi`, or `apxh/sbi`.

Current APXH selection contract: the tracked `apxh/configure` selects `multiboot` for `i386`, `multiboot efi` for `amd64`, and `sbi` for `riscv64`. The amd64 multiboot path has reviewed bounded QEMU coverage both with the README-built default `amd64-unknown-elf`/`i686-unknown-elf` cache on `PATH` and with the explicit `TOOLCHAIN=x86_64-linux-gnu` plus `TOOLCHAIN32=i686-unknown-elf` override; `tools/qemu-smoke-amd64.sh` captures the override path and can be forced to the default prefixes. The amd64 EFI path also has reviewed default-prefix OVMF runtime-smoke coverage from a clean worktree through the normal APXH/NUX, IPI, userspace, syscall, exit, and idle markers. The riscv64 SBI/DTB path has reviewed target-toolchain/QEMU coverage through `tools/qemu-smoke-riscv64.sh`; RISC-V EFI remains unselected by default until its platform contract and linker/toolchain path are decided; see `docs/backlog.md`.

## Add or extend a platform library

Implement `include/nux/plt.h`:

- `plt_init()` must validate the descriptor from `hal_pltinfo()` and discover hardware.
- `plt_pcpu_iterate`, `plt_pcpu_enter`, `plt_pcpu_id`, and `plt_pcpu_start` drive CPU enumeration and startup.
- `plt_pcpu_ipi`/`plt_pcpu_nmi` and broadcast variants back `libnux` CPU operations.
- IRQ functions must describe, enable, disable, and EOI platform IRQs.
- Timer functions provide a counter, period in femtoseconds, alarm programming, and clear alarm.
- `plt_interrupt()` maps HAL vectors/causes back to `hal_entry_timer`, `hal_entry_irq`, or `hal_entry_ipi`.

Evidence examples:

- ACPI platform: `libplt_acpi/plt.c`, `acpi.c`, `lapic.c`, `ioapic.c`, `hpet.c`.
- SBI/DTB platform: `libplt_sbi/sbi.c`.

## Add a device or hardware feature

Prefer platform-specific additions behind `include/nux/plt.h` or a new public header if kernels need to use it. For example, timer support is abstracted by `plt_tmr_*` and exported as `timer_*` from `include/nux/nux.h`/`libnux/time.c`. IRQ routing belongs in the PLT layer, not in `libnux`.

For future IOMMU or PCIe work, keep the Murgia/MH transparency constraint in mind: Murgia should keep the same user-facing `hwdev`/`sys_export`/`dexport` device/export API on IOMMU-present and no-IOMMU systems, and Murgia/kernel/userspace owns ACPI parsing, PCIe/MCFG interpretation, IOMMU/device policy, AHCI, and filesystem policy above the NUX platform-pointer boundary. NUX platform additions may consume the typed APXH descriptor internally, but they must not export raw ACPI tables, table inventories, MCFG/ECAM records, or a public ACPI fact substrate.

## Update examples and documentation

For each new port or hardware path:

- Update `README.md`, `docs/hardware-support.md`, and this file.
- Add a minimal QEMU or smoke path if possible (`example/Makefile.in`).
- Add a backlog entry for unverified hardware requirements and missing tests.
- Record whether the path was built, run under QEMU, or only source-inspected.
