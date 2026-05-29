# Architecture

NUX is a small kernel prototyping framework. A kernel links against `libnux`, implements `main()` plus event hooks declared in `include/nux/nux.h`, and is booted by APXH with optional userspace payload support.

## Build-time composition

The top-level configure script selects the architecture and then chooses one HAL and one platform library:

| `ARCH` | HAL | Platform library | Evidence |
| --- | --- | --- | --- |
| `i386` | `libhal_x86` | `libplt_acpi` | `configure.ac` maps `i386` to `hal_x86 plt_acpi`. |
| `amd64` | `libhal_x86` | `libplt_acpi` | `configure.ac`; `libhal_x86/Makefile.in` selects `amd64/Makefile.inc`. |
| `riscv64` | `libhal_riscv` | `libplt_sbi` | `configure.ac`; `libhal_riscv/Makefile.in`; `libplt_sbi/Makefile.in`. |

The top-level `Makefile.in` builds `libfdt`, `apxh`, the selected HAL/PLT, `libnux`, `libnux_user`, `tools`, and `example`. `AC_CONFIG_SUBDIRS([apxh example])` in `configure.ac` causes APXH and the example tree to be configured as subprojects.

## Boot and runtime flow

1. **APXH starts first.** APXH is an ELF loader with machine-dependent entry code for multiboot, EFI, and SBI/DTB boot paths (`apxh/multiboot/*`, `apxh/efi/*`, `apxh/sbi/*`).
2. **APXH loads payloads.** Common APXH code loads kernel and optional user ELF payloads, interprets APXH-specific program-header types, builds page tables, and writes boot data (`apxh/src/main.c`, `apxh/src/elf.c`, `apxh/src/project.h`).
3. **APXH passes boot contracts.** Boot data uses `struct apxh_bootinfo`, `struct apxh_region`, `struct apxh_stree`, and `struct apxh_pltdesc` from `include/nux/apxh.h`. Linker scripts request those areas with APXH program-header IDs such as `PHT_APXH_INFO`, `PHT_APXH_STREE`, `PHT_APXH_FRAMEBUF`, and `PHT_APXH_REGIONS` (`libhal_x86/*/exe.ld`, `libhal_riscv/exe.ld`).
4. **HAL takes over.** The selected HAL reads APXH boot information, validates the S-tree allocator bitmap, initializes console/framebuffer state, exposes memory ranges, and sets architecture-specific entry/paging/CPU functions (`libhal_x86/x86.c`, `libhal_riscv/riscv.c`).
5. **`libnux` constructor initializes the runtime.** `_nux_sysinit()` initializes PFN allocation, KMEM, KVA, PFN cache, platform devices, CPU state, secondary CPUs, and then marks NUX running (`libnux/init.c`).
6. **Kernel code runs.** The user kernel's `main()` runs after HAL/PLT initialization. Secondary CPUs run `main_ap()`. Events call `entry_sysc`, `entry_pf`, `entry_ex`, `entry_alarm`, `entry_ipi`, and `entry_irq` through `libnux/entry.c`.

## Component boundaries

### APXH boot support

APXH owns early boot payload loading, early memory map normalization, initial page-table construction, and the APXH boot-info contract. Common logic lives in `apxh/src/main.c`, `apxh/src/elf.c`, `apxh/src/payload.c`, `apxh/src/pae.c`, and `apxh/src/rv64-sv.c`. Machine-dependent backends provide firmware/boot-protocol details:

- `apxh/multiboot/*`: x86 multiboot memory map, framebuffer, RSDP, and x86 transition code.
- `apxh/efi/*`: EFI application, GOP framebuffer, EFI memory map, RSDP, and EFI boot-services exit.
- `apxh/sbi/*`: RISC-V SBI/OpenSBI path and DTB memory-region discovery.

### HAL (`libhal_x86`, `libhal_riscv`)

The HAL abstracts CPU instructions, interrupt frames, page-table leaf entries, TLB operations, virtual-memory areas, physical-memory descriptions, and CPU bring-up. The interface is declared in `include/nux/hal.h`.

- x86 common code is in `libhal_x86/x86.c` and `libhal_x86/pmap.c`; `libhal_x86/i386/*` and `libhal_x86/amd64/*` implement architecture-specific frames, entry stubs, paging, and secondary CPU bootstrap.
- RISC-V code is in `libhal_riscv/riscv.c`, `libhal_riscv/pmap.c`, `libhal_riscv/sv48.c`, and `libhal_riscv/entry.S`.

### Platform libraries (`libplt_acpi`, `libplt_sbi`)

The PLT layer abstracts discovered hardware: CPUs, IRQs, IPIs/NMIs, timers, and platform interrupt dispatch (`include/nux/plt.h`).

- `libplt_acpi` expects APXH to provide a `PLT_ACPI` descriptor. It scans ACPI tables, MADT, LAPIC, IOAPIC, and HPET (`libplt_acpi/plt.c`, `acpi.c`, `lapic.c`, `ioapic.c`, `hpet.c`).
- `libplt_sbi` expects `PLT_DTB`. It parses `/cpus`, `timebase-frequency`, and PLIC information from a DTB, implements SBI timer calls, and uses software interrupts plus `libnux` NMI emulation (`libplt_sbi/sbi.c`, `libnux/nmiemul.c`). Several SBI/PLIC paths are still TODO.

### `libnux`

`libnux` is the runtime library linked into kernels. It contains:

- initialization and status flags (`libnux/init.c`, `libnux/internal.h`),
- PFN allocation and PFN cache (`libnux/pfnalloc.c`, `libnux/pfncache.c`),
- KVA/KMAP/KMEM/UMAP and user access (`libnux/kva.c`, `kmap.c`, `kmem.c`, `umap.c`, `uaddr.c`),
- CPU operations, cross-CPU TLB flushes, IPIs/NMIs (`libnux/cpu.c`, `ktlbgen.c`, `nmiemul.c`),
- event dispatch (`libnux/entry.c`),
- framebuffer, symbols, time, and performance support (`libnux/framebuffer.c`, `symbol.c`, `time.c`, `include/nux/nuxperf.h`).

### `libec`

`libec` is the embedded C library. Its `README` describes it as a small, self-contained, non-standard C library based mostly on NetBSD code. It provides C runtime support, string/memory functions, printf, atomics/assembly helpers, setjmp, and build fragments used by APXH, kernels, and userspace.

### `libnux_user`

`libnux_user` is a small userspace syscall wrapper library. It exposes `syscall0` through `syscall6` in `libnux_user/nux/syscalls.h`, uses architecture-specific inline assembly in `libnux_user/{i386,amd64,riscv64}/arch_syscalls.h`, and is linked by `example/user/Makefile.in`.

### Examples and tools

- `example/kern/main.c` is a NUX kernel example that boots an optional userspace payload, handles syscall IDs used by the example user program, prints performance counters, and demonstrates timer/IPI/page-fault hooks.
- `example/user/main.c` calls the syscall wrappers and defines minimal `putchar`, `puts`, and `exit` on top of kernel-provided syscall IDs.
- `tools/ar50` creates the small archive format used for payloads.
- `tools/objappend` appends payload data as loadable ELF sections.
- `tools/mksyms/mksyms.sh` generates kernel symbol tables from `nm` output; `mk/nuxexe.mk` links the final symbolized kernel.

## Current architectural gaps

- RISC-V platform support has explicit TODOs for secondary CPU start, platform CPU enter, external IRQs, IRQ enable/disable/type/max, and EOI (`libhal_riscv/riscv.c`, `libplt_sbi/sbi.c`).
- x86 user-access hardening has TODO placeholders for SMEP in `libhal_x86/x86.c`.
- The README's boot-support claims and APXH configure logic are not fully aligned; see [hardware support](hardware-support.md) and [backlog](backlog.md).
