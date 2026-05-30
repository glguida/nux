# Build and run

This page records the build flow supported by the tracked files and what was actually verified during the documentation pass.

## Prerequisites

The build is not self-contained. The tracked files show these requirements:

- A host C compiler with C library development headers, GNU Make, and helper
  tools used by bundled host utilities. Top-level `configure` runs
  `AC_PROG_CC` before checking target tools (`configure.ac`); the current
  `tools/libbfd` build also needs `file(1)` and `makeinfo`/Texinfo while
  building the checked-in Binutils BFD submodule.
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

## Preflight without building

Use `tools/build-preflight.sh` from a checkout/worktree to repeat the
architecture readiness checks without running configure, make, submodule update,
or QEMU. The helper follows the same default target prefixes as `configure.ac`
and `apxh/configure.ac`, the same QEMU binary mapping as `example/Makefile.in`,
and reports checked-in submodule initialization status from git. It exits 0 only
when the selected target tools, runtime-smoke QEMU binary, and submodules are all
ready; a nonzero result is useful blocker evidence for missing tools, missing
QEMU, or uninitialized submodules.

Repeat the reviewed i386 prerequisite check with the stable task-workspace
`TOOLBIN`, then keep using the checked-in QEMU smoke harness for the actual
runtime smoke:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ARCH=i386 ./tools/build-preflight.sh

TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin \
  ./tools/qemu-smoke-i386.sh
```

For amd64, the default check still records missing default
`amd64-unknown-elf-*` tools and default-PATH 32-bit target tools. The supported
local smoke path is the reviewed override: use the stable external i386
`TOOLBIN`, `TOOLCHAIN=x86_64-linux-gnu`, and
`TOOLCHAIN32=i686-unknown-elf`. That path passed preflight, configure, `make`,
and bounded QEMU in the reviewed environment after the freestanding PIE fixes:
commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e` adds compile-side
`-fno-pie`, complementing commit `51fc152c5d4cd92be9ee0ec9f7410e245bb16dd0`'s
link-side `-no-pie`. The checked-in `tools/qemu-smoke-amd64.sh` repeats the
out-of-tree configure/build/QEMU flow for that override. It requires host
`x86_64-linux-gnu-{gcc,ld,ar,objcopy}`, the 32-bit APXH compiler from
`TOOLBIN`, `make`, and `qemu-system-x86_64` to be available in the runner. It
counts timeout rc 124 as success only after APXH/NUX boot output, `IPI!`,
userspace hello, `SYSC0` through `SYSC6`, `UCTXT_SETA2` kernel/user markers,
`User exited with error code: 42`, no unexpected kernel page fault, and repeated
zero-valued `pnux_entry_pagefault` idle counter lines appear:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin

ARCH=amd64 ./tools/build-preflight.sh

TOOLBIN="$TOOLBIN" ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf \
  ./tools/build-preflight.sh

TOOLBIN="$TOOLBIN" ./tools/qemu-smoke-amd64.sh

# Manual equivalent of the harness:
PATH="$TOOLBIN:$PATH" ./configure ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf
PATH="$TOOLBIN:$PATH" make
(cd example && PATH="$TOOLBIN:$PATH" timeout --foreground 30s make qemu)
```

For riscv64, the current container has a verified Debian package path for the
standard target tools and QEMU: `binutils-riscv64-unknown-elf` 2.44-3+7+b1,
`gcc-riscv64-unknown-elf` 14.2.0+19, and `qemu-system-misc`
1:10.0.8+ds-0+deb13u1+b2, with required dependencies `opensbi` 1.6-1,
`qemu-system-riscv`, and `qemu-system-s390x`. These packages are recorded
container evidence, not the only supported source of a RISC-V target toolchain.
With those tools and initialized submodules, preflight passes:

```sh
ARCH=riscv64 ./tools/build-preflight.sh
```

The checked-in riscv64 smoke harness repeats the verified SBI/DTB runtime path
from a fresh out-of-tree build. It runs `configure ARCH=riscv64`, builds only
`libfdt`, `apxh/sbi`, `libhal_riscv`, `libplt_sbi`, `libnux`, `libnux_user`,
`tools`, and `example example_qemu`, then launches the generated example through
`make qemu`:

```sh
BUILD=/tmp/the-nux-riscv64-smoke
export BUILD
./tools/qemu-smoke-riscv64.sh

# Manual equivalent of the harness:
src=$PWD
mkdir -p "$BUILD"
(cd "$BUILD" && "$src/configure" ARCH=riscv64)
(cd "$BUILD" && make -C libfdt all)
(cd "$BUILD" && make -C apxh/sbi all)
(cd "$BUILD" && make -C libhal_riscv all)
(cd "$BUILD" && make -C libplt_sbi all)
(cd "$BUILD" && make -C libnux all)
(cd "$BUILD" && make -C libnux_user all)
(cd "$BUILD" && make -C tools all)
(cd "$BUILD" && make -C example example_qemu)
(cd "$BUILD/example" && timeout --foreground 30s make qemu)
```

The riscv64 QEMU smoke treats timeout rc 124 as success only after the serial
log has already reached OpenSBI/APXH/NUX/userspace markers, `SYSC0` through
`SYSC6`, `UCTXT_SETA2`, `UADDR_VALIDRANGE`, `KVA_ALLOC_FREE`,
`User exited with error code: 42`, and repeated zero-valued
`pnux_entry_pagefault` idle counters. The full default top-level `make` is still
not this verified path: APXH selects `sbi efi` for riscv64, and Debian
`riscv64-unknown-elf-ld` currently fails the RISC-V EFI link with
`riscv64-unknown-elf-ld: -shared not supported` while linking `apxh.so`.

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
- APXH `configure` selects `multiboot efi` for `amd64`. The default `amd64-unknown-elf-*` tools are still absent in the reviewed local paths; the standardized local smoke path uses the host-prefixed `x86_64-linux-gnu-*` override plus the reviewed i686 `TOOLCHAIN32`. `tools/qemu-smoke-amd64.sh` captures that out-of-tree flow after the freestanding libec `-no-pie`/`-fno-pie` fixes. The bounded run reaches APXH/NUX, IPI, userspace, syscall, `UCTXT_SETA2`, exit, and idle counter markers before the expected timeout.
- `libhal_x86/amd64/exe.ld` uses the high-half base `0xffff800000000000`, 512 GiB physmap, 512 GiB KVA, 512 GiB KMEM, 256 MiB PFN cache, and 64 MiB framebuffer mapping.

### riscv64

- Top-level `configure.ac` selects `libhal_riscv` + `libplt_sbi`.
- APXH `configure` selects `sbi efi` for `riscv64`. The verified runtime path is SBI/DTB: build `libfdt`, `apxh/sbi`, `libhal_riscv`, `libplt_sbi`, `libnux`, `libnux_user`, `tools`, and `example example_qemu`, then run `qemu-system-riscv64 -M virt` through the generated `example` `make qemu` target.
- Full default top-level `make` is still limited by the RISC-V APXH EFI target: Debian `riscv64-unknown-elf-ld` reports `-shared not supported` while linking `apxh.so`. Do not treat RISC-V EFI or the full APXH `sbi efi` build as verified by the SBI smoke.
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

Current integrated amd64/riscv64 follow-through evidence from tasks
`the-nux-amd64-build-smoke-followthrough`,
`the-nux-amd64-runtime-page-fault-followup`, and
`the-nux-riscv64-smoke-tooling-followthrough` is:

- `./configure --help` advertises `ARCH=i386`, `ARCH=amd64`, and `ARCH=riscv64`.
- QEMU: `/usr/bin/qemu-system-x86_64` and `/usr/bin/qemu-system-riscv64` are present and report QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`.
- Default amd64 preflight/configure remains blocked by missing default `amd64-unknown-elf-*` tools and `i686-unknown-elf-gcc` on the default `PATH`.
- Worktree-local submodule initialization succeeded for the follow-through tasks; the current amd64 override path and riscv64 default-tool path are not blocked by submodule checkout or workspace capacity.
- The installed host-prefixed `x86_64-linux-gnu-{gcc,ld,ar,objcopy}` tools plus `TOOLCHAIN32=i686-unknown-elf` from the stable external i386 `TOOLBIN` are sufficient for `ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` preflight, configure, and `make` to pass after commit `51fc152c5d4cd92be9ee0ec9f7410e245bb16dd0` added `-no-pie` to freestanding libec links and commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e` added compile-side `-fno-pie`.
- The baseline amd64 QEMU failure was traced to host GCC default-PIE code generation in freestanding fixed-address objects. With the override path after commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e`, the bounded amd64 QEMU smoke reaches APXH/NUX boot output, `IPI!`, `Hello from userspace, NUX!`, `SYSC0` through `SYSC6`, `UCTXT_SETA2 test passed.`, `UCTXT_SETA2 user test passed.`, `User exited with error code: 42`, and repeated zero-valued `pnux_entry_pagefault` idle counter lines; timeout rc 124 is expected only after those success/idle markers.
- i386 smoke remains passing with the stable external i386 `TOOLBIN`; the follow-up verification recorded `tools/qemu-smoke-i386.sh` rc 0.
- riscv64 default target tools are present from the bounded Debian package path: `binutils-riscv64-unknown-elf` 2.44-3+7+b1, `gcc-riscv64-unknown-elf` 14.2.0+19, and `qemu-system-misc` 1:10.0.8+ds-0+deb13u1+b2, with required dependencies `opensbi` 1.6-1, `qemu-system-riscv`, and `qemu-system-s390x`. With initialized submodules, `ARCH=riscv64 ./tools/build-preflight.sh` passes.
- The verified riscv64 runtime path is the explicit SBI/DTB subset build used by `tools/qemu-smoke-riscv64.sh`; bounded QEMU reaches OpenSBI/APXH/NUX/userspace/syscall/UCTXT/UADDR/KVA markers and repeated zero-valued `pnux_entry_pagefault` idle counters before the expected timeout.
- Default full riscv64 top-level `make` is still not verified because APXH selects both `sbi` and `efi`; Debian `riscv64-unknown-elf-ld` fails the RISC-V APXH EFI link with `riscv64-unknown-elf-ld: -shared not supported` while linking `apxh.so`.

Authoritative follow-through logs are under `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-impl/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-review/workspace/review-logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-commit/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-impl/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-review/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-commit/workspace/logs`, and `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-riscv64-smoke-tooling-followthrough-impl/workspace/logs`.

The architecture/toolchain follow-through standardized the reviewed `x86_64-linux-gnu`/`i686-unknown-elf` override as the local amd64 smoke path and added `tools/qemu-smoke-amd64.sh`. A task-local real `amd64-unknown-elf` build is not currently a low-risk substitute: fresh discovery found no `amd64-unknown-elf-*`, `amd64-elf-*`, `x86_64-unknown-elf-*`, or `x86_64-elf-*` candidates in PATH, task workspaces, or common bin roots; the `gcc_toolchain_build` source tree was not present locally; the stable i386 toolchain alone occupies about 2.0G; and the fresh implementer runner lacked `make`/host GCC/QEMU with `/home` already 98% used. The riscv64 path is runnable in the current container through the checked-in SBI/DTB smoke harness; remaining open items are future deliberate default amd64 target-toolchain provision if desired, CI policy for checked-in smoke harnesses, and a separate policy/toolchain decision for RISC-V APXH EFI if full default riscv64 `make` should become a verified path.
