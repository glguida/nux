# Debugging and observability

## QEMU debug target

The example tree provides two run targets in `example/Makefile.in`:

```sh
cd build/example
make qemu
make qemu_dbg
```

`qemu_dbg` appends `-S -s`, so QEMU starts paused and exposes a GDB stub on port 1234. The exact QEMU binary depends on `ARCH`:

- `qemu-system-i386` for i386.
- `qemu-system-x86_64` for amd64.
- `qemu-system-riscv64 -M virt` for riscv64.

No repository GDB script was found during this pass. A typical manual flow after `make qemu_dbg` is to start the matching target GDB, connect to `:1234`, load symbols for the built kernel, set breakpoints, then continue.

## Timeout-based i386 smoke workflow

The current container has a reviewed i386 smoke workflow. Use the stable task-workspace target toolchain path rather than the older `/tmp` toolchain install:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin
BUILD=/tmp/the-nux-i386-qemu-stable-path-build-i386
rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"
PATH="$TOOLBIN:$PATH" /home/glguida/the_nux/configure ARCH=i386
PATH="$TOOLBIN:$PATH" make -j"$(nproc)"
cd example
PATH="$TOOLBIN:$PATH" timeout --foreground 20s make qemu
```

A timeout rc 124 is expected for this smoke because the guest idles after userspace exits. Count it as a pass only if the captured serial output includes the reviewed markers: `APXH started.`, `NUX library (nux)`, `Hello from userspace, NUX!`, `SYSC0 test passed.`, `SYSC6 test passed.`, `UCTXT_SETA2 test passed.`, `UCTXT_SETA2 user test passed.`, `UADDR_VALIDRANGE test passed.`, and `User exited with error code: 42`.

## Logging paths

Kernel code uses `printf`, `info`, `warn`, `error`, `fatal`, and `debug` macros from `include/nux/nux.h`. `putchar` is routed to `hal_putchar` by `libnux/ec.c`.

- x86 `hal_putchar` writes to framebuffer when available, otherwise VGA text, and always to serial port `0x3f8` (`libhal_x86/x86.c`, `serial.c`, `vga_text.c`). The QEMU target uses `-serial mon:stdio -nographic`, so serial output should appear on the terminal.
- RISC-V `hal_putchar` uses an SBI ecall (`libhal_riscv/riscv.c`).
- APXH prints boot diagnostics while loading payloads and constructing mappings (`apxh/src/main.c`, `apxh/src/elf.c`, `apxh/multiboot/mb.c`, `apxh/sbi/md.c`, `apxh/efi/efi-main.c`).

`DEBUG` is controlled by configure's `--disable-debug` path (`configure.ac`, `include/nux/nux.h`). Platform verbosity is controlled by `--enable-plt-verbose`, which substitutes `PLT_VERBOSE` from `configure.ac`.

## Trap and break instructions

- `hal_debug()` is `int3` on x86 and `ebreak` on RISC-V (`libhal_x86/include/nux/hal_config.h`, `libhal_riscv/include/nux/hal_config.h`).
- `hal_cpu_trap()` is `ud2` on x86 and `ebreak` on RISC-V (`libhal_x86/x86.c`, `libhal_riscv/riscv.c`).
- `abort()` loops on `hal_debug()` (`libnux/ec.c`).

## Panic and crash output

`nux_panic()` marks the system panicking, sends NMIs to stop other CPUs, and calls `hal_panic()` (`libnux/ec.c`).

- x86 `hal_panic()` resets framebuffer state, prints CPU ID and error, prints the HAL frame, prints a stack trace using generated symbols, walks the page table for CR2, and halts (`libhal_x86/x86.c`).
- RISC-V `hal_panic()` resets framebuffer state, prints CPU ID/error/frame, and halts; it currently does not include the x86 stack trace/page-table walk detail (`libhal_riscv/riscv.c`).

## Symbol generation

`tools/mksyms/mksyms.sh` runs `nm -n` over the unsymbolized kernel and emits `__nux_syms.c`. `mk/nuxexe.mk` links `__nux_syms.o` into the final `$(NUX_KERNEL)`. `libhal_x86/x86.c` uses `nux_symresolve()` while printing panic stack traces.

## Performance counters and measures

`include/nux/nuxperf.h` provides counters in the `.perfctr` section and measures in the `.measure` section. `libnux/entry.c` increments counters for syscalls, page faults, exceptions, NMIs, timers, IRQs, and IPIs. The example kernel prints counters and syscall timing measurements from `entry_alarm()` (`example/kern/main.c`).

## Known debugging gaps

- No automated smoke-test harness or CI script was found.
- No checked-in GDB command files were found.
- RISC-V panic output is less detailed than x86 panic output.
- `libnux/framebuffer.c` contains explicit TODO/XXX comments about RGB masks and bounds checks, so framebuffer debugging output may be fragile.
