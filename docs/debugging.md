# Debugging and observability

## QEMU/GDB debug helper

The example tree still provides the raw run targets in `example/Makefile.in`:

```sh
cd build/example
make qemu
make qemu_dbg
```

`qemu_dbg` appends `-S -s`, so QEMU starts paused and exposes a GDB stub on port 1234. The checked-in `tools/qemu-debug.sh` helper prepares that flow from a source checkout/worktree without requiring manual symbol notes. It uses an out-of-tree `BUILD`/`NUX_BUILD`, refuses to build inside the source checkout, supports `ARCH=i386`, `ARCH=amd64` multiboot, and `ARCH=riscv64` SBI/DTB, writes a GDB command file in the build directory, and prints both the `make qemu_dbg` command and the matching GDB invocation.

Required external tools are the same build tools, initialized submodules, and target toolchains needed by the selected architecture's normal example build. QEMU is needed when running `make qemu_dbg` or `--run-qemu`. GDB is optional until the developer runs the printed attach command.

Examples:

```sh
# Prepare a RISC-V SBI/DTB debug build and GDB command file.
ARCH=riscv64 BUILD=/tmp/the-nux-riscv64-debug ./tools/qemu-debug.sh --prepare

# Prepare i386 with the reviewed task-local toolchain path.
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ARCH=i386 BUILD=/tmp/the-nux-i386-debug ./tools/qemu-debug.sh --prepare

# Prepare amd64 multiboot with the reviewed local smoke override.
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf \
  BUILD=/tmp/the-nux-amd64-debug ./tools/qemu-debug.sh --prepare
```

After `--prepare`, start QEMU in one terminal with the printed `(cd .../example && make qemu_dbg)` command, then run the printed GDB command in another terminal. The generated command file loads the built `example/kern/example` kernel ELF symbols, connects with `target remote :1234`, and includes commented optional `add-symbol-file` lines for APXH and the user payload. Use `GDB=/path/to/gdb` to choose a debugger; by default the helper only prints `gdb` for x86 and `gdb-multiarch` for RISC-V. No GDB package or debugger frontend is committed to or required by NUX itself, and `--prepare` verification does not require GDB to be installed.

For a bounded non-interactive QEMU diagnostic, use `--run-qemu`. The helper runs `make qemu_dbg` under `TIMEOUT`; because `-S -s` pauses before guest execution, timeout rc 124 is expected evidence that QEMU reached the debugger wait, not a smoke-marker pass.

The exact QEMU binary still depends on `ARCH`:

- `qemu-system-i386` for i386.
- `qemu-system-x86_64` for amd64.
- `qemu-system-riscv64 -M virt` for riscv64.

## Timeout-based smoke workflows

The current reviewed i386 smoke workflow has a checked-in harness for repeating it. From a source checkout/worktree, use the stable task-workspace target toolchain path rather than the older `/tmp` toolchain install:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ./tools/qemu-smoke-i386.sh
```

`tools/qemu-smoke-i386.sh` creates an out-of-tree build under `/tmp` by default; set `BUILD` or `NUX_BUILD` to choose another out-of-tree directory and `TIMEOUT` to override the default 20 second QEMU timeout. It records configure, make, and QEMU serial logs in the build directory. A timeout rc 124 is expected because the guest idles after userspace exits, but the harness counts it as a pass only if the captured serial output includes the reviewed markers: `APXH started.`, `NUX library (nux)`, `Hello from userspace, NUX!`, `SYSC0 test passed.`, `SYSC6 test passed.`, `UCTXT_SETA2 test passed.`, `UCTXT_SETA2 user test passed.`, `UADDR_MEMSET test passed.`, `UADDR_MEMSET user test passed.`, `UADDR_VALIDRANGE test passed.`, `UMAP_BOUNDS test passed.`, `KVA_ALLOC_FREE test passed.`, `KMAP_UPDATE test passed.`, and `User exited with error code: 42`.

The checked-in amd64 harness uses the standardized local override path. It defaults to `TOOLCHAIN=x86_64-linux-gnu` and `TOOLCHAIN32=i686-unknown-elf`, so the runner must provide host-prefixed x86_64 tools, `qemu-system-x86_64`, `make`, and the stable i386 `TOOLBIN`:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ./tools/qemu-smoke-amd64.sh
```

`tools/qemu-smoke-amd64.sh` uses the same out-of-tree build, log, timeout, `BUILD`/`NUX_BUILD`, `TIMEOUT`, `JOBS`, and `NUX_SMOKE_REUSE_BUILD` conventions as the i386 harness. It accepts timeout rc 124 only after the reviewed amd64 markers appear, including `IPI!`, userspace hello, `SYSC0` through `SYSC6`, `UCTXT_SETA2` and `UADDR_MEMSET` kernel/user markers, the `UMAP_BOUNDS` and `KMAP_UPDATE` markers, `User exited with error code: 42`, no unexpected kernel page fault, and repeated zero-valued `pnux_entry_pagefault` idle counter lines.

The checked-in riscv64 harness is runnable in this container with the reviewed Debian `riscv64-unknown-elf-*` tools, initialized submodules, and `qemu-system-riscv64`. It builds the verified SBI/DTB subset from an out-of-tree directory and accepts timeout rc 124 only after the OpenSBI/APXH/NUX/userspace/syscall/UCTXT/UADDR/UMAP/KVA/KMAP/exit/idle markers appear:

```sh
./tools/qemu-smoke-riscv64.sh
```

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

- The checked-in i386, amd64, amd64 EFI, and riscv64 smoke harnesses are not wired into CI.
- `tools/qemu-debug.sh` prepares command files for the existing QEMU GDB stub, but it is intentionally not a full debugger frontend and does not install or vendor GDB.
- RISC-V panic output is less detailed than x86 panic output.
- Framebuffer color packing and bounds checks have a serial `FRAMEBUFFER_MASK_BOUNDS` regression marker, but there is still no screenshot/visual comparison harness.
