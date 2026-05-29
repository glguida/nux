# Hardware support

This matrix distinguishes configured support, source-level support, and gaps found in tracked files.

## Architecture and boot matrix

| Architecture | Configure support | HAL | Platform library | APXH paths in source/config | Status notes |
| --- | --- | --- | --- | --- | --- |
| `i386` | Yes (`configure.ac`) | `libhal_x86` i386 | `libplt_acpi` | `apxh/multiboot`; EFI source has i386 settings in `apxh/efi/Makefile.in` but `apxh/configure.ac` selects only `multiboot` for i386 | Multiboot + ACPI is the configured path. README's EFI-on-i386 claim needs reconciliation with configure. |
| `amd64` | Yes (`configure.ac`) | `libhal_x86` amd64 | `libplt_acpi` | `apxh/configure` selects `multiboot efi` | Source has amd64 HAL, multiboot, EFI, ACPI, LAPIC/IOAPIC/HPET support. APXH subdir selection is script-verified; full amd64 build/runtime verification still needs an amd64 target-toolchain path. |
| `riscv64` | Yes (`configure.ac`) | `libhal_riscv` | `libplt_sbi` | `apxh/configure` selects `sbi efi`; EFI RISC-V code exists in `apxh/efi/apxhefi/efi_md.c` | SBI/DTB is the coherent configured path. APXH subdir selection is script-verified; RISC-V EFI platform-descriptor compatibility and full build/runtime remain unverified. |

## x86 / ACPI support

Current implemented areas:

- CPU instructions, serial/VGA/framebuffer console, APXH bootinfo, S-tree validation, memory-region pinning, panic output, and stack traces: `libhal_x86/x86.c`.
- i386 PAE paging and a 3 GiB user UMAP: `libhal_x86/i386/pae32.c`, `libhal_x86/include/nux/hal_config_i386.h`.
- amd64 4-level paging and 42-bit user UMAP by default: `libhal_x86/amd64/pae64.c`, `libhal_x86/include/nux/hal_config_amd64.h`.
- i386 and amd64 secondary CPU bootstrap using LAPIC INIT/SIPI through `libplt_acpi/lapic.c` and HAL trampoline code in `libhal_x86/i386/i386.c`, `libhal_x86/amd64/amd64.c`.
- ACPI table loading, MADT scan, LAPIC, IOAPIC, GSI routing, and HPET timer: `libplt_acpi/acpi.c`, `lapic.c`, `ioapic.c`, `hpet.c`.

Known gaps:

- x2APIC, LSAPIC, IOSAPIC entries are explicitly ignored by the ACPI scanner (`libplt_acpi/acpi.c`).
- x86 `hal_useraccess_start/end` have TODO placeholders for SMEP handling (`libhal_x86/x86.c`).
- i386 TLS setup is explicitly ignored in `hal_frame_settls` (`libhal_x86/i386/sys_entry.c`).
- EFI support still needs full architecture-specific configure/build/runtime verification, especially i386 EFI and non-i386 target-toolchain paths; APXH subdir selection is no longer blocked by the malformed generated configure case.

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
| `i386` | `/usr/bin/qemu-system-i386` from apt package `qemu-system-x86`, QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)` | Verified. Fresh `ARCH=i386` configure/build/QEMU smoke passes with `TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin` prepended to `PATH`; `timeout --foreground 20s make qemu` returns rc 124 only after success markers appear. |
| `amd64` | `/usr/bin/qemu-system-x86_64` is present from the same package | Not verified in this task. Configure/build/QEMU smoke still need an amd64 target-toolchain path; APXH subdir selection itself has been script-verified. |
| `riscv64` | Source target uses `qemu-system-riscv64 -M virt` | Not verified in this task. RISC-V target tools, QEMU availability, SBI/DTB path, and platform gaps remain open. |

The i386 serial-smoke markers observed in the verified run were `APXH started.`, `NUX library (nux)`, userspace hello output, `SYSC0`/`SYSC6` pass messages, the `UCTXT_SETA2` kernel/user regression markers, and `User exited with error code: 42`. Treat an rc 124 timeout as a pass only when those markers appear before the timeout; otherwise investigate it as a failed or inconclusive runtime smoke.
