# Userspace model

NUX supports an optional boot-time userspace payload and a small syscall wrapper library. There is no full process manager in the current source; kernels decide how to map, schedule, and handle user contexts.

## Boot-time user payload

APXH can load a kernel payload and an optional user payload:

- Multiboot and SBI paths expose payload slots through `payload_get()` (`apxh/multiboot/mb.c`, `apxh/sbi/md.c`, `apxh/src/payload.c`).
- EFI loads `kernel.elf` and optionally `user.elf` (`apxh/efi/efi-main.c`).
- Common APXH code loads the user ELF as user mappings and stores its entry address in `apxh_bootinfo.uentry` (`apxh/src/main.c`, `apxh/src/elf.c`, `include/nux/apxh.h`).

At runtime, `uctxt_bootstrap()` checks `hal_virtmem_userentry()` and initializes a `uctxt_t` if a boot-time user process exists (`libnux/uctxt.c`). The example kernel uses this path in `example/kern/main.c`.

## User context

`uctxt_t` is an alias for the HAL interrupt frame (`include/nux/types.h`). `libnux/uctxt.c` provides architecture-neutral helpers for:

- initializing instruction, stack, and global pointers,
- getting/setting instruction and stack pointers,
- setting syscall return and argument registers,
- setting TLS,
- printing a frame.

The HAL supplies the actual frame layout and register setters/getters (`include/nux/hal.h`, `libhal_x86/i386/sys_entry.c`, `libhal_x86/amd64/frame.c`, `libhal_riscv/frame.c`).

## Event hooks seen by a kernel

A NUX kernel implements the hooks declared in `include/nux/nux.h`:

- `entry_sysc(uctxt_t *, unsigned long a1, ..., unsigned long a7)` for syscalls. The tracked `include/nux/nux.h` contract has seven syscall words after the `uctxt_t *`; `libnux/entry.c` passes all seven to the kernel hook.
- `entry_pf(uctxt_t *, va, hal_pfinfo_t)` for page faults.
- `entry_ex(uctxt_t *, ex)` for generic exceptions.
- `entry_alarm(uctxt_t *)` for platform timer alarms.
- `entry_ipi(uctxt_t *)` for inter-processor interrupts.
- `entry_irq(uctxt_t *, irq, level)` for platform IRQs.

`libnux/entry.c` validates whether the interrupted frame is user, idle, or invalid, panics on unexpected kernel faults, calls the kernel hook, and converts the returned `uctxt_t` back to a HAL frame or idle state.

This is the current return-based contract: `include/nux/nux.h` declares every `entry_*` hook as returning `uctxt_t *`, `libnux/entry.c` stores that return value, and `libnux/uctxt.c` converts it with `uctxt_frame()`. The generic NUX contract audit treats historical mutate-input-frame wording as conditional future design space, not an implementation mandate. Kernels and examples must treat the tracked headers as authoritative unless a future reviewed generic NUX problem changes the API (see `docs/nux-pte-entry-contracts.md`).

## Syscall ABI wrappers

`libnux_user` exposes `syscall0` through `syscall6` (`libnux_user/nux/syscalls.h`, `libnux_user/syscalls.c`). Architecture-specific assembly is in:

- `libnux_user/i386/arch_syscalls.h`: software interrupt `int $0x21`; syscall number in `eax`; arguments in `edi`, `esi`, `ecx`, `edx`, `ebx`, `ebp`.
- `libnux_user/amd64/arch_syscalls.h`: `syscall`; syscall number in `rax`; arguments in `rdi`, `rsi`, `rdx`, `rbx`, `r8`, `r9`.
- `libnux_user/riscv64/arch_syscalls.h`: `ecall`; syscall number/return in `a0`; arguments in `a1` through `a6`.

The HAL entry side maps those registers back into `hal_entry_syscall()` (`libhal_x86/i386/sys_entry.c`, `libhal_x86/amd64/frame.c`, `libhal_riscv/riscv.c`). `libnux/entry.c` then calls the kernel's `entry_sysc()`.

The example user program defines `putchar` as syscall `4096` and `exit` as syscall `4097`, then tests syscall arities 0 through 6 (`example/user/main.c`). In the example kernel, `a1` is the syscall number and `a2` through `a7` are the six possible user arguments. These numbers are example policy, not a documented stable global ABI.

## User entry and linking

`example/user/Makefile.in` builds `exuser` from `main.c` plus an architecture-specific `crt0.S`, links `libnux_user`, and links `libec`. The crt0 files set up a small static stack and call `___start`:

- `example/user/i386/crt0.S`
- `example/user/amd64/crt0.S`
- `example/user/riscv64/crt0.S`

`example/Makefile.in` packs `kern/example` and `user/exuser` into the QEMU image through `tools/ar50` and `tools/objappend`.

## User memory and page faults

- `umap_bootstrap()` captures APXH-created user mappings, and `cpu_umap_enter()` loads a UMAP into the CPU (`libnux/umap.c`, `libnux/cpu.c`).
- `uaddr_valid()` checks one address inside the HAL user range; `uaddr_validrange()` checks half-open user-copy byte ranges, accepts non-empty ranges through a last byte inside the user interval, rejects overflow/oversized ranges, and treats zero-length no-ops as valid from userbase through one-past-user-end (`libnux/uaddr.c`).
- `uaddr_copyfrom`, `uaddr_copyto`, and `uaddr_memset` call CPU user-access helpers that validate those ranges before recovering from page faults via `setjmp`/`longjmp` and an optional callback (`include/nux/nux.h`, `libnux/cpu.c`).

## Current userspace gaps

- There is no central syscall-number registry beyond the example kernel/user pair; Murgia currently depends on the NUX `entry_sysc` handler arity remaining explicit.
- There is no scheduler/process abstraction in the current public API; event hooks return the next user context or `UCTXT_IDLE`.
- The entry-hook contract is return-based today; `docs/nux-pte-entry-contracts.md` audits a possible mutate-input-frame/action model but recommends no ABI change unless a source-backed generic NUX problem justifies it.
- i386 TLS is explicitly ignored in `hal_frame_settls()` (`libhal_x86/i386/sys_entry.c`).
- The example initializes only a boot-time user context; broader lifecycle rules for multiple user address spaces are left to kernels using NUX.
- `uctxt_seta2()` now delegates to `hal_frame_seta2()` in `libnux/uctxt.c`; the example kernel/user smoke checks the setter by writing and reading back a known third-argument-register value across a syscall return.
