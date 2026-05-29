# Hardware support

This matrix distinguishes configured support, source-level support, and gaps found in tracked files.

## Architecture and boot matrix

| Architecture | Configure support | HAL | Platform library | APXH paths in source/config | Status notes |
| --- | --- | --- | --- | --- | --- |
| `i386` | Yes (`configure.ac`) | `libhal_x86` i386 | `libplt_acpi` | `apxh/multiboot`; EFI source has i386 settings in `apxh/efi/Makefile.in` but `apxh/configure.ac` selects only `multiboot` for i386 | Multiboot + ACPI is the configured path. README's EFI-on-i386 claim needs reconciliation with configure. |
| `amd64` | Yes (`configure.ac`) | `libhal_x86` amd64 | `libplt_acpi` | Intended `multiboot efi` in `apxh/configure.ac`; generated `apxh/configure` currently appears malformed around the amd64/riscv64 case | Source has amd64 HAL, multiboot, EFI, ACPI, LAPIC/IOAPIC/HPET support. Build script needs repair/verification. |
| `riscv64` | Yes (`configure.ac`) | `libhal_riscv` | `libplt_sbi` | `apxh/sbi`; EFI RISC-V code exists in `apxh/efi/apxhefi/efi_md.c` | SBI/DTB is the coherent configured path. RISC-V EFI is source-present but platform-descriptor compatibility is unverified. |

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
- EFI support needs configure/build verification, especially for i386 and the malformed generated APXH configure case.

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

These targets were source-inspected during the initial docs pass. The old missing-host-compiler blocker has since been resolved in this container. The reviewed i386 target-toolchain slice now passes configure/build with `/tmp/the-nux-i386-target-toolchain-gcc_toolchain_build/install/bin` prepended to `PATH`; without that prefix, the target tools are still absent. The current runtime smoke blocker is QEMU: `qemu-system-i386` and `qemu-system-x86_64` are not installed.
