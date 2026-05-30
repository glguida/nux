# Build and run

This page records the build flow supported by the tracked files and what was actually verified during the documentation pass.

## Prerequisites

The build is not self-contained. The tracked files show these requirements:

- A host C compiler and normal build tools. Top-level `configure` runs `AC_PROG_CC` before checking target tools (`configure.ac`).
- Cross binutils/GCC prefixes for the selected target:
  - `i686-unknown-elf-*` for `ARCH=i386`.
  - `amd64-unknown-elf-*` for `ARCH=amd64`.
  - `riscv64-unknown-elf-*` for `ARCH=riscv64`.
  - `ARCH=amd64` also needs a 32-bit toolchain for multiboot APXH; override with `TOOLCHAIN32` (`apxh/configure.ac`).
- Initialized submodules (`.gitmodules`):
  - `contrib/dtc` for `libfdt` (`libplt_sbi`, APXH SBI, and top-level `libfdt`).
  - `contrib/gnu-efi` for APXH EFI (`apxh/efi/Makefile.in`).
  - `contrib/binutils` for BFD/libiberty/zlib used by `tools/libbfd/Makefile.in` and `tools/objappend`.
- QEMU if using the example `qemu` targets:
  - `qemu-system-i386` for `ARCH=i386`.
  - `qemu-system-x86_64` for `ARCH=amd64`.
  - `qemu-system-riscv64 -M virt` for `ARCH=riscv64`.

The README points to `gcc_toolchain_build` as the intended way to create the cross toolchains.

## Regenerating configure scripts

Only needed after editing `configure.ac` or M4 macros:

```sh
./bootstrap.sh
```

`bootstrap.sh` runs `aclocal && autoconf` at the top level and in `apxh/` and `example/`.

## Normal build flow

From a fresh checkout:

```sh
git submodule update --init --recursive
mkdir build
cd build
../configure ARCH=i386
make -j"$(nproc)"
```

Override toolchain prefixes if they differ from the defaults:

```sh
../configure ARCH=i386 TOOLCHAIN=i686-elf
../configure ARCH=amd64 TOOLCHAIN=amd64-elf TOOLCHAIN32=i686-elf
../configure ARCH=riscv64 TOOLCHAIN=riscv64-elf
```

Useful configure options from `./configure --help`:

- `--disable-opt`: use `-O0 -g` rather than optimized flags.
- `--disable-werror`: omit `-Werror` from generated build flags. Default configure keeps `-Werror` enabled.
- `--disable-debug`: compile without debug code/messages.
- `--enable-plt-verbose`: enable extra platform logging.

## Running the example under QEMU

After a successful build:

```sh
cd build/example
make qemu
```

`example/Makefile.in` builds `example_qemu` by copying the APXH binary, packing `kern/example` and `user/exuser` with `tools/ar50/ar50`, and appending the archive with `tools/objappend/objappend`.

The generated QEMU commands are:

- i386: `qemu-system-i386 -kernel example_qemu -serial mon:stdio -nographic`
- amd64: `qemu-system-x86_64 -kernel example_qemu -serial mon:stdio -nographic`
- riscv64: `qemu-system-riscv64 -M virt -kernel example_qemu -serial mon:stdio -nographic`

Debug targets add `-S -s`:

```sh
cd build/example
make qemu_dbg
```

Attach GDB to QEMU's default stub on TCP port 1234.

## Architecture notes

### i386

- Top-level `configure.ac` selects `libhal_x86` + `libplt_acpi`.
- `apxh/configure.ac` selects the `multiboot` APXH subdir for `i386`.
- `libhal_x86/i386/exe.ld` defines a 32-bit kernel layout at `0xc0100000`, PAE paging, a 3 GiB user UMAP range, 512 MiB KMEM, 256 MiB KVA, and 16 MiB framebuffer mapping.

### amd64

- Top-level `configure.ac` selects `libhal_x86` + `libplt_acpi`.
- APXH `configure` selects `multiboot efi` for `amd64`. In this container the default `amd64-unknown-elf-*` tools are still absent; using the installed `x86_64-linux-gnu-*` prefix plus the reviewed i686 `TOOLCHAIN32` lets configure complete, but the full build has not passed yet.
- `libhal_x86/amd64/exe.ld` uses the high-half base `0xffff800000000000`, 512 GiB physmap, 512 GiB KVA, 512 GiB KMEM, 256 MiB PFN cache, and 64 MiB framebuffer mapping.

### riscv64

- Top-level `configure.ac` selects `libhal_riscv` + `libplt_sbi`.
- APXH `configure` selects `sbi efi` for `riscv64`; full riscv64 configure/build/QEMU verification still needs real RISC-V target tools and `qemu-system-riscv64` availability.
- `libhal_riscv/exe.ld` uses the same high-half base, 512 GiB physmap/KVA/KMEM, 256 MiB PFN cache, and 32 MiB framebuffer mapping.
- Treat RISC-V EFI as unverified: APXH EFI records `PLT_ACPI`, while `libplt_sbi` requires `PLT_DTB` (`apxh/efi/apxhefi/efi_md.c`, `libplt_sbi/sbi.c`).

## Verification history and current container status

Commands from the initial documentation pass in the task worktree:

```sh
./configure --help >/tmp/the-nux-configure-help.txt
mkdir -p build-docs-check
cd build-docs-check
../configure ARCH=i386 > /tmp/the-nux-build-configure-i386.txt 2>&1
```

Initial results:

- `./configure --help` succeeded and produced 78 lines of help in `/tmp/the-nux-configure-help.txt`.
- `../configure ARCH=i386` failed before target-tool checks because the container PATH did not contain a host C compiler:

```text
checking for gcc... no
checking for cc... no
checking for cl.exe... no
configure: error: no acceptable C compiler found in $PATH
```

That host-compiler result is history, not the current first blocker in this container.

A later reviewed host-tools readiness slice on task `the-nux-docs-capabilities` installed the normal host baseline with apt: `build-essential`, `autoconf`, `automake`, and `file`, plus dependencies including GCC/G++, Make, and binutils. The reviewer independently verified the tools and versions, and `./configure --help` still passed with 78 lines.

After those host tools were present, fresh out-of-tree `ARCH=i386` configure probes from `/tmp` passed the host C compiler checks and failed at the target toolchain check:

```text
checking for gcc... gcc
checking whether the C compiler works... yes
checking whether we are using the GNU C compiler... yes
checking for i686-unknown-elf-gcc... no
configure: error: i686-unknown-elf-gcc not found
```

A subsequent reviewed target-toolchain slice built the README-recommended `gcc_toolchain_build` i386 tools under `/tmp/the-nux-i386-target-toolchain-gcc_toolchain_build/install/bin` at source commit `eecef0929616a96517a83dab988a8429ad8c62d8`. The `/tmp` install remains useful provenance, but it is not the documented current path.

The current reviewed i386 toolchain path for this container is the stable task-workspace install copied from that reviewed `/tmp` build:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin
```

With `PATH="$TOOLBIN:$PATH"`, `i686-unknown-elf-gcc` 14.2.0 and Binutils 2.43.1 `ld`/`ar`/`objcopy` resolve from that stable `TOOLBIN`. Do not commit the toolchain into this repository; it is an external task-workspace artifact.

The i386 QEMU runtime blocker is also resolved in this container. The reviewed package install requested `qemu-system-x86` with `--no-install-recommends`, which provides both `/usr/bin/qemu-system-i386` and `/usr/bin/qemu-system-x86_64`; both report QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`.

Checked-in i386 smoke harness:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ./tools/qemu-smoke-i386.sh
```

The harness runs from a source checkout/worktree, defaults to a fresh out-of-tree build directory under `/tmp`, accepts `BUILD` or `NUX_BUILD` to choose another out-of-tree build, writes configure/make/QEMU logs inside the build directory, and leaves source/submodule cleanup decisions to the operator. It may also be used without `TOOLBIN` if the `i686-unknown-elf-*` tools are already on `PATH`.

Manual equivalent reviewed i386 smoke flow:

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

In the reviewed run, `configure` and `make -j"$(nproc)"` passed. The bounded `make qemu` returned timeout rc 124 because the guest idled after success; treat that timeout as acceptable only when the serial log already contains the success markers:

- `APXH started.`
- `NUX library (nux)`
- `Hello,`
- `Hello from userspace, NUX!`
- `SYSC0 test passed.`
- `SYSC6 test passed.`
- `UCTXT_SETA2 test passed.`
- `UCTXT_SETA2 user test passed.`
- `UADDR_VALIDRANGE test passed.`
- `KVA_ALLOC_FREE test passed.`
- `User exited with error code: 42`

Representative logs for the original stable-path verification are `/tmp/the-nux-i386-qemu-doc-toolchain-configure-i386.txt`, `/tmp/the-nux-i386-qemu-doc-toolchain-make-i386.txt`, `/tmp/the-nux-i386-qemu-doc-toolchain-qemu-i386.txt`, and `/tmp/the-nux-i386-qemu-doc-toolchain-qemu-markers.txt`. The `uctxt_seta2()` fix repeated the flow from `/tmp/the-nux-uctxt-seta2-build-i386` after initializing submodules in its dedicated worktree; logs are `/tmp/the-nux-uctxt-seta2-configure-i386.txt`, `/tmp/the-nux-uctxt-seta2-make-i386.txt`, `/tmp/the-nux-uctxt-seta2-qemu-i386.txt`, and `/tmp/the-nux-uctxt-seta2-qemu-markers.txt`. The `uaddr_validrange()` fix repeated the same smoke path from `/tmp/the-nux-uaddr-validrange-build-i386`; logs are `/tmp/the-nux-uaddr-validrange-configure-i386.txt`, `/tmp/the-nux-uaddr-validrange-make-i386.txt`, `/tmp/the-nux-uaddr-validrange-qemu-i386.txt`, and `/tmp/the-nux-uaddr-validrange-qemu-markers.txt`. The KVA metadata-removal fix repeated it from `/tmp/the-nux-kva-vmap-free-build-i386`; logs are `/tmp/the-nux-kva-vmap-free-configure-i386.txt`, `/tmp/the-nux-kva-vmap-free-make-i386.txt`, `/tmp/the-nux-kva-vmap-free-qemu-i386.txt`, and `/tmp/the-nux-kva-vmap-free-qemu-markers.txt`.

A 2026-05-30 amd64/riscv64 capability probe in task `the-nux-amd64-riscv-smoke` found:

- `./configure --help` advertises `ARCH=i386`, `ARCH=amd64`, and `ARCH=riscv64`.
- QEMU: `/usr/bin/qemu-system-x86_64` is present and reports QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`; `qemu-system-riscv64` is not on `PATH`.
- Default amd64 configure is still blocked at the target-toolchain probe: `amd64-unknown-elf-gcc not found`.
- The installed host-prefixed `x86_64-linux-gnu-{gcc,ld,ar,objcopy}` tools plus `TOOLCHAIN32=i686-unknown-elf` from the stable i386 `TOOLBIN` are sufficient for `ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` configure to complete through the top-level, APXH, and example sub-configures.
- The follow-on amd64 build did not reach QEMU. It stopped while building submodule-dependent `tools/libbfd` and APXH EFI pieces because the dedicated worktree submodules were not fully checked out; attempting `git submodule update --init --recursive` in that worktree then failed with `No space left on device` while checking out `contrib/binutils`.
- Default riscv64 configure is blocked at `riscv64-unknown-elf-gcc not found`; no project-local `riscv64-unknown-elf-*`, `riscv64-elf-*`, or `riscv64-linux-gnu-*` toolchain and no `qemu-system-riscv64` were found.

The job evidence logs are under `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-riscv-smoke-capability-impl/workspace/evidence/`, including `configure-amd64-default.log`, `configure-amd64-x86_64-linux-gnu.log`, `make-amd64-x86_64-linux-gnu.log`, `configure-riscv64-default.log`, `tool-discovery.log`, `state-toolchain-discovery.log`, and `submodule-update.log`.

Remaining build/run gaps are not the old host compiler, i386 target-toolchain, i386 QEMU blocker, or i386 submodule initialization. The open items are a real amd64 target-toolchain or a reviewed decision to support the `x86_64-linux-gnu` prefix for amd64, enough free workspace/submodule checkout capacity to complete the amd64 build, `qemu-system-riscv64` plus a RISC-V target toolchain for riscv64, and the source fixes tracked in `docs/backlog.md`.
