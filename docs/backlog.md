# Backlog and capability roadmap

This backlog is source-inspected only unless a verification result is explicitly mentioned.

## P0: build and documentation blockers

1. **Provide baseline build tools in the development container.**
   - Evidence: `../configure ARCH=i386` failed in `build-docs-check` with `no acceptable C compiler found in $PATH`.
   - Next slice: install/provide a host C compiler, then rerun configure and capture the next target-tool or submodule error.
2. **Initialize and verify required submodules.**
   - Evidence: `.gitmodules` lists `contrib/gnu-efi`, `contrib/binutils`, and `contrib/dtc`; worktree `git submodule status` showed them uninitialized with leading `-`.
   - Next slice: initialize submodules in a clean worktree and rerun configure/build.
3. **Fix APXH configure architecture selection.**
   - Evidence: `apxh/configure.ac` is missing a separator between the `amd64` and `riscv64` `AS_CASE` branches; generated `apxh/configure` shows an obviously malformed amd64 branch and has an error message that omits `riscv64`.
   - Next slice: fix `apxh/configure.ac`, run `./bootstrap.sh`, and verify `ARCH=i386`, `ARCH=amd64`, and `ARCH=riscv64` APXH subdir selection.
4. **Fix `--disable-werror` handling in configure inputs.**
   - Evidence: top-level, APXH, and example `configure.ac` define `AC_ARG_ENABLE([werror])` but test `enable_relax` rather than `enable_werror`.
   - Next slice: correct the variable, regenerate configure scripts, and test `--disable-werror`.
5. **Reconcile README boot-support claims with configure behavior.**
   - Evidence: README says APXH supports EFI on i386, amd64, and riscv64. `apxh/configure.ac` currently selects only `multiboot` for i386, intended `multiboot efi` for amd64, and `sbi efi` for riscv64.
   - Next slice: decide whether the README, configure logic, or both should change.

## P1: correctness and runtime capability gaps

1. **Fix `uctxt_seta2()`.**
   - Evidence: `libnux/uctxt.c` calls `hal_frame_seta1(f, a2)` inside `uctxt_seta2()`.
   - Next slice: change to `hal_frame_seta2()`, add a small kernel/user check using `uctxt_seta2`, and verify on one architecture.
2. **Review user-address range validation.**
   - Evidence: `libnux/uaddr.c` has unused malformed macros and `uaddr_validrange()` validates `a + size` rather than the last byte; overflow/zero-length behavior is undocumented.
   - Next slice: define desired boundary semantics and add tests or assertions.
3. **Review KVA allocator removal.**
   - Evidence: `libnux/kva.c` `vmap_remove()` calls `kmem_alloc(0, sizeof(struct vme))`, which looks like a typo for freeing the metadata node.
   - Next slice: inspect allocator invariants, fix if confirmed, and add a simple KVA allocate/free stress test.
4. **Complete or document RISC-V SMP support.**
   - Evidence: `libhal_riscv/riscv.c` has TODOs in `hal_pcpu_init()` and `hal_pcpu_startaddr()` and returns `PADDR_INVALID` for secondary start.
   - Next slice: decide whether SBI HSM or another start mechanism should be used.
5. **Complete or document RISC-V external IRQ/PLIC support.**
   - Evidence: `libplt_sbi/sbi.c` TODOs cover IRQ type, enable/disable/max, EOI, platform CPU enter/start, and external interrupt dispatch.
   - Next slice: wire PLIC contexts discovered from DTB to `plt_interrupt()` and IRQ APIs.
6. **Define the RISC-V EFI platform contract.**
   - Evidence: APXH EFI has RISC-V entry code but returns `PLT_ACPI`; the configured RISC-V platform library requires `PLT_DTB`.
   - Next slice: decide whether RISC-V EFI should use ACPI, DTB handoff, or be disabled until supported.
7. **Add x86 SMEP/user-access hardening.**
   - Evidence: `libhal_x86/x86.c` has TODOs in `hal_useraccess_start()` and `hal_useraccess_end()`.
   - Next slice: implement CR4.SMEP/SMAP-aware behavior or explicitly document unsupported CPU hardening.
8. **Improve framebuffer correctness.**
   - Evidence: `libnux/framebuffer.c` has XXX comments for RGB masks, bounds checking, and rewrite need.
   - Next slice: honor framebuffer masks and clamp writes; add a QEMU visual/serial smoke check.
9. **Clarify i386 TLS support.**
   - Evidence: `libhal_x86/i386/sys_entry.c` states `hal_frame_settls()` is ignored because i386 TLS needs LDT support.
   - Next slice: document as unsupported or add LDT/TLS support.
10. **ACPI/x86 hardware expansion.**
    - Evidence: `libplt_acpi/acpi.c` ignores LSAPIC, x2APIC, IOSAPIC, and LX2APICNMI entries.
    - Next slice: prioritize x2APIC if modern hardware support is a near-term goal.

## P2: usability, tests, and polish

1. **Create an automated smoke target.**
   - Evidence: only manual `make qemu`/`make qemu_dbg` targets were found.
   - Next slice: add a timeout-based QEMU smoke script that checks for expected serial output.
2. **Add GDB/debugging helpers.**
   - Evidence: QEMU debug target exists but no GDB scripts were found.
   - Next slice: add docs or scripts for loading symbols and connecting to `:1234`.
3. **Document syscall ABI stability.**
   - Evidence: `libnux_user` wraps syscalls, but syscall numbers are example-local (`4096` putchar, `4097` exit in `example/kern/main.c`/`example/user/main.c`).
   - Next slice: either publish a minimal NUX syscall convention or explicitly state that kernels own their syscall ABI.
4. **Expand Murgia requirements as they arrive.**
   - Evidence: no concrete Murgia requirements were present in this task.
   - Next slice: add trace rows to `docs/murgia-integration.md` when Murgia sends requirements.
5. **Clean README and install docs.**
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
