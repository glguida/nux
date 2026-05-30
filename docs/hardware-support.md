# Hardware support

This matrix distinguishes configured support, source-level support, and gaps found in tracked files. For the Murgia modern-hardware/AHCI/filesystem boundary and gap inventory, see [`murgia-substrate-roadmap.md`](murgia-substrate-roadmap.md).

## Architecture and boot matrix

| Architecture | Configure support | HAL | Platform library | APXH paths in source/config | Status notes |
| --- | --- | --- | --- | --- | --- |
| `i386` | Yes (`configure.ac`) | `libhal_x86` i386 | `libplt_acpi` | `apxh/multiboot`; EFI source has i386 settings in `apxh/efi/Makefile.in` but `apxh/configure.ac` selects only `multiboot` for i386 | Multiboot + ACPI is the configured path. README now reflects this configure behavior; do not claim i386 EFI support until configure policy and runtime verification change. |
| `amd64` | Yes (`configure.ac`) | `libhal_x86` amd64 | `libplt_acpi` | `apxh/configure` selects `multiboot efi` | Source has amd64 HAL, multiboot, EFI, ACPI, LAPIC/IOAPIC/HPET support. Default-prefix multiboot now passes when the README-built `gcc_toolchain_build` cache provides `amd64-unknown-elf-*` and default-PATH `i686-unknown-elf-gcc`; the standardized local smoke path also remains verified with the explicit `x86_64-linux-gnu` prefix override and i686 `TOOLCHAIN32`. `tools/qemu-smoke-amd64.sh` captures the override path and can be forced to the default prefixes. Bounded QEMU reaches APXH/NUX/tests, IPI, userspace, syscall, `UCTXT_SETA2`, exit, and idle counter markers; timeout rc 124 is expected after success because the demo keeps idling. EFI-specific amd64 runtime coverage remains separate from the multiboot smoke. |
| `riscv64` | Yes (`configure.ac`) | `libhal_riscv` | `libplt_sbi` | `apxh/configure` selects `sbi`; EFI RISC-V code exists in `apxh/efi/apxhefi/efi_md.c` but is not selected by default | SBI/DTB is the coherent configured/default path. The reviewed container has `riscv64-unknown-elf-*` tools and `qemu-system-riscv64` for this path. RISC-V EFI platform-descriptor compatibility remains unverified, and Debian `riscv64-unknown-elf-ld` reports `-shared not supported` when linking the EFI `apxh.so`. |

## x86 / ACPI support

Current implemented areas:

- CPU instructions, serial/VGA/framebuffer console, APXH bootinfo, S-tree validation, memory-region pinning, panic output, and stack traces: `libhal_x86/x86.c`.
- i386 PAE paging and a 3 GiB user UMAP: `libhal_x86/i386/pae32.c`, `libhal_x86/include/nux/hal_config_i386.h`.
- amd64 4-level paging and 42-bit user UMAP by default: `libhal_x86/amd64/pae64.c`, `libhal_x86/include/nux/hal_config_amd64.h`.
- i386 and amd64 secondary CPU bootstrap using LAPIC INIT/SIPI through `libplt_acpi/lapic.c` and HAL trampoline code in `libhal_x86/i386/i386.c`, `libhal_x86/amd64/amd64.c`.
- Internal ACPI table loading for MADT/HPET setup, LAPIC, IOAPIC, GSI routing, and HPET timer: `libplt_acpi/acpi.c`, `lapic.c`, `ioapic.c`, `hpet.c`. This consumes the `PLT_ACPI` platform descriptor inside `libplt_acpi`; it is not a raw ACPI table export.

Known gaps:

- x2APIC, LSAPIC, IOSAPIC entries are explicitly ignored by the ACPI scanner (`libplt_acpi/acpi.c`).
- No tracked ACPI MCFG/PCIe discovery, PCI bus enumeration, MSI/MSI-X, Intel DMAR or AMD IVRS parsing, IOMMU abstraction, DMA-remapping API, AHCI/storage driver, filesystem, or real-disk-image QEMU harness was found in the current tracked source/doc search. Murgia/kernel/userspace owns that ACPI/device policy above the existing NUX typed-platform-pointer boundary; do not treat it as a future NUX ACPI export or platform-fact inventory.
- x86 `hal_useraccess_start/end` have TODO placeholders for SMEP handling (`libhal_x86/x86.c`).
- i386 TLS setup is explicitly ignored in `hal_frame_settls` (`libhal_x86/i386/sys_entry.c`).
- EFI support still needs full architecture-specific runtime verification. i386 EFI source/settings are present but not selected by APXH configure; amd64 multiboot preflight/configure/build and bounded QEMU now pass both with the README-built default `amd64-unknown-elf`/`i686-unknown-elf` cache and with the reviewed `x86_64-linux-gnu`/`i686-unknown-elf` override after the libec PIE fixes, but amd64 EFI-specific runtime verification remains open. riscv64 SBI/DTB target-toolchain and QEMU coverage are available, but RISC-V EFI remains unselected by default and needs a separate platform-contract/toolchain decision. APXH subdir selection is no longer blocked by the malformed generated configure case.

## RISC-V / SBI support

Current implemented areas:

- SV48 page tables and 42-bit user UMAP default: `libhal_riscv/sv48.c`, `libhal_riscv/include/nux/hal_config.h`.
- SBI/OpenSBI APXH path parses DTB memory and reserved-memory nodes: `apxh/sbi/md.c`.
- Runtime platform parses DTB `/cpus`, `timebase-frequency`, and PLIC-compatible nodes: `libplt_sbi/sbi.c`.
- Timer uses `rdtime` plus SBI calls to set/clear alarms (`libplt_sbi/sbi.c`).
- RISC-V uses `HAL_NMIEMUL` and `libnux/nmiemul.c` because the HAL cannot use true NMIs as NUX expects (`libhal_riscv/include/nux/hal_config.h`, `libhal_riscv/internal.h`).

Known gaps:

- `hal_pcpu_init`, `hal_pcpu_startaddr`, and `hal_init_done` are TODO or no-op in `libhal_riscv/riscv.c`; secondary CPU bring-up currently returns `PADDR_INVALID`.
- `plt_pcpu_enter`, `plt_pcpu_iterate`, `plt_pcpu_start`, IRQ type/enable/disable/max, IRQ EOI, and external interrupts have TODO placeholders in `libplt_sbi/sbi.c`.
- PLIC contexts are printed but not fully wired to external interrupt dispatch (`libplt_sbi/sbi.c`).
- RISC-V EFI APXH records a `PLT_ACPI` descriptor, while `libplt_sbi` requires `PLT_DTB`; this path should be treated as unverified until the boot/platform contract is made explicit.

## Console and framebuffer

- x86 `hal_putchar` writes to framebuffer if initialized, otherwise VGA text, and always serial port `0x3f8` (`libhal_x86/x86.c`, `libhal_x86/serial.c`, `libhal_x86/vga_text.c`).
- RISC-V `hal_putchar` currently uses an SBI console ecall (`libhal_riscv/riscv.c`).
- APXH multiboot and EFI can provide framebuffer descriptions (`apxh/multiboot/mb.c`, `apxh/efi/efi-main.c`).
- `libnux/framebuffer.c` has explicit TODO/XXX comments about RGB masks and bounds checking; treat framebuffer drawing as basic console support, not a finished graphics API.

## QEMU support

The example makefile defines QEMU smoke targets for all configured architectures (`example/Makefile.in`):

- i386: `qemu-system-i386`
- amd64: `qemu-system-x86_64`
- riscv64: `qemu-system-riscv64 -M virt`

Current reviewed container status:

| Architecture | QEMU status in this container | Verification status |
| --- | --- | --- |
| `i386` | `/usr/bin/qemu-system-i386` from apt package `qemu-system-x86`, QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)` | Verified. Fresh `ARCH=i386` configure/build/QEMU smoke passes with `TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin` prepended to `PATH`; `tools/qemu-smoke-i386.sh` now repeats that out-of-tree build/run and treats `timeout --foreground 20s make qemu` rc 124 as success only after the required markers appear. |
| `amd64` | `/usr/bin/qemu-system-x86_64` is present from the same package in the reviewed QEMU environment, QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)` | Default-prefix and override multiboot runtime verified. With `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-amd64-default-toolchain-policy/toolchains/gcc_toolchain_build/install/bin` on `PATH`, default `ARCH=amd64` preflight, configure, `make`, and bounded `make qemu` pass using `amd64-unknown-elf-*` plus default-PATH `i686-unknown-elf-gcc`. `ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` remains the standardized local smoke override. Both paths require APXH/NUX boot output, `IPI!`, userspace hello, `SYSC0`-`SYSC6`, `UCTXT_SETA2` kernel/user markers, `User exited with error code: 42`, no unexpected kernel page fault, and repeated zero-valued `pnux_entry_pagefault` idle counter lines before accepting timeout rc 124. |
| `riscv64` | `/usr/bin/qemu-system-riscv64` is present, QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)` | Verified for the SBI/DTB path with `riscv64-unknown-elf-*` target tools and `tools/qemu-smoke-riscv64.sh`. Default APXH configure selects `sbi`; RISC-V EFI remains unverified and is not selected by default. |

The checked-in i386 smoke harness requires the serial markers `APXH started.`, `NUX library (nux)`, userspace hello output, `SYSC0`/`SYSC6` pass messages, the `UCTXT_SETA2` kernel/user regression markers, `UADDR_VALIDRANGE test passed.`, `KVA_ALLOC_FREE test passed.`, and `User exited with error code: 42`. The checked-in amd64 smoke harness defaults to the standardized `x86_64-linux-gnu`/`i686-unknown-elf` override, can be forced to `TOOLCHAIN=amd64-unknown-elf TOOLCHAIN32=i686-unknown-elf` when the README-built default cache is on `PATH`, and additionally requires `IPI!`, all `SYSC0` through `SYSC6` pass messages, no unexpected kernel page fault, and repeated zero-valued `pnux_entry_pagefault` idle counter lines. Treat an rc 124 timeout as a pass only when the architecture's required markers appear before the timeout; otherwise investigate it as a failed or inconclusive runtime smoke.
