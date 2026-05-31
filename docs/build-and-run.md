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

For amd64, use the README `gcc_toolchain_build` install bin directory on
`PATH` to exercise the real default freestanding target-toolchain path. In this
task environment, the reviewed cache contains both `amd64-unknown-elf-*` and
`i686-unknown-elf-*` tools built from `gcc_toolchain_build` commit
`eecef0929616a96517a83dab988a8429ad8c62d8`:

```sh
AMD64_TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-amd64-default-toolchain-policy/toolchains/gcc_toolchain_build/install/bin
PATH="$AMD64_TOOLBIN:$PATH" ARCH=amd64 ./tools/build-preflight.sh

# Manual default-prefix smoke flow from a checkout/worktree:
src=$PWD
BUILD=/tmp/the-nux-amd64-default-build
mkdir -p "$BUILD"
(cd "$BUILD" && PATH="$AMD64_TOOLBIN:$PATH" "$src/configure" ARCH=amd64)
(cd "$BUILD" && PATH="$AMD64_TOOLBIN:$PATH" make -j1)
(cd "$BUILD/example" && PATH="$AMD64_TOOLBIN:$PATH" timeout --foreground 30s make qemu)

# Checked-in amd64 harness, forced to the default freestanding prefixes:
TOOLBIN="$AMD64_TOOLBIN" TOOLCHAIN=amd64-unknown-elf TOOLCHAIN32=i686-unknown-elf \
  ./tools/qemu-smoke-amd64.sh
```

The local host-prefix smoke path remains a reviewed override: use a `TOOLBIN`
that provides `i686-unknown-elf-gcc`, plus `TOOLCHAIN=x86_64-linux-gnu` and
`TOOLCHAIN32=i686-unknown-elf`. That path passed preflight, configure, `make`,
and bounded QEMU in the reviewed environment after the freestanding PIE fixes:
commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e` adds compile-side
`-fno-pie`, complementing commit `51fc152c5d4cd92be9ee0ec9f7410e245bb16dd0`'s
link-side `-no-pie`. The checked-in `tools/qemu-smoke-amd64.sh` defaults to the
out-of-tree configure/build/QEMU flow for that override. It requires host
`x86_64-linux-gnu-{gcc,ld,ar,objcopy}`, the 32-bit APXH compiler from
`TOOLBIN`, `make`, and `qemu-system-x86_64` to be available in the runner. It
counts timeout rc 124 as success only after APXH/NUX boot output, `IPI!`,
userspace hello, `SYSC0` through `SYSC6`, `UCTXT_SETA2` kernel/user markers,
`User exited with error code: 42`, no unexpected kernel page fault, and repeated
zero-valued `pnux_entry_pagefault` idle counter lines appear:

```sh
TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-docs-capabilities/toolchains/i686-unknown-elf/bin

TOOLBIN="$TOOLBIN" ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf \
  ./tools/build-preflight.sh

TOOLBIN="$TOOLBIN" ./tools/qemu-smoke-amd64.sh

# Manual equivalent of the override harness:
PATH="$TOOLBIN:$PATH" ./configure ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf
PATH="$TOOLBIN:$PATH" make
(cd example && PATH="$TOOLBIN:$PATH" timeout --foreground 30s make qemu)
```

### amd64 default toolchain policy

The default `ARCH=amd64` path is intentionally a freestanding target-toolchain
path, not a host-distro alias. A clean default runner must provide
`amd64-unknown-elf-{gcc,ld,ar,objcopy}` on `PATH`, and amd64's multiboot APXH
build must also resolve `i686-unknown-elf-gcc` (or an explicitly selected
`TOOLCHAIN32` prefix). The reviewed local smoke path with
`TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` is useful evidence for
this container, but it must stay an explicit override because the host Linux
prefix has different target defaults and previously needed reviewed
freestanding `-no-pie`/`-fno-pie` fixes before the runtime smoke passed.

Package/tool discovery found no Debian-packaged or preinstalled real amd64
freestanding candidate under the default names (`amd64-unknown-elf`,
`amd64-elf`, `x86_64-unknown-elf`, or `x86_64-elf`). Debian package metadata in
this environment exposes the host `x86_64-linux-gnu` tools and packaged
`riscv64-unknown-elf` tools, but not an amd64/x86_64 unknown-elf GCC/binutils
pair. The README `gcc_toolchain_build` flow is therefore the durable default
provisioning path for amd64 in this environment: build or reuse a true
freestanding `amd64-unknown-elf` GCC/binutils pair, include the required
`i686-unknown-elf` APXH compiler in the same runner/toolchain cache, prepend the
cache's `install/bin` to `PATH`, and require
`ARCH=amd64 ./tools/build-preflight.sh` to pass before default amd64
configure/build/smoke jobs run.

The current task cache is an external artifact, not repository content:

```sh
AMD64_TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-amd64-default-toolchain-policy/toolchains/gcc_toolchain_build/install/bin
```

It was copied from a local README-built `gcc_toolchain_build` checkout at commit
`eecef0929616a96517a83dab988a8429ad8c62d8` and provides GCC 14.2.0 plus
Binutils 2.43.1 for both `amd64-unknown-elf` and `i686-unknown-elf`. With that
bin directory on `PATH`, default `ARCH=amd64` preflight, configure, `make -j1`,
and bounded QEMU multiboot smoke all pass in this container.

Do not commit compiler artifacts, generated build trees, or wrapper aliases into
this repository. A wrapper/prefix is acceptable only as an operator/CI mechanism
around a true freestanding target toolchain (or around the explicit override
smoke job); it should not make default `ARCH=amd64` silently resolve to the
host Linux prefix. If the toolchain cache is absent, corrupted, or removed, the
exact default-path blocker reverts to "missing `amd64-unknown-elf-*` plus
default-PATH `i686-unknown-elf-gcc`" and the next action is to rebuild or
restore the README `gcc_toolchain_build` artifact outside the repository.

### amd64 EFI APXH build and runtime smoke

`ARCH=amd64` APXH configure selects both `multiboot` and `efi`, but the
standard example `make qemu` target is still the multiboot path. The EFI loader
is built under `apxh/efi` through `contrib/gnu-efi`: `apxh/efi/Makefile.in`
maps amd64 to gnu-efi `ARCH=x86_64`, objcopies the linked `apxh.so` as an
`efi-app-x86_64`, and produces `apxh.efi`. At runtime `apxh.efi` loads
`kernel.elf` and optional `user.elf` from the EFI volume before entering the
normal APXH/NUX path.

The amd64 EFI build path is verified in this task environment with the same
README-built default cache used for amd64 multiboot:

```sh
AMD64_TOOLBIN=/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-amd64-default-toolchain-policy/toolchains/gcc_toolchain_build/install/bin
src=$PWD
build=/tmp/the-nux-amd64-efi-build
mkdir -p "$build"
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" "$src/configure" ARCH=amd64)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C apxh/efi all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C libhal_x86 all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C libplt_acpi all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C libnux all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C libnux_user all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C example/kern all)
(cd "$build" && PATH="$AMD64_TOOLBIN:$PATH" make -j1 -C example/user all)
```

Expected artifacts are `apxh/efi/apxh.efi`, `example/kern/example`, and
`example/user/exuser`. In the reviewed container, `file(1)` reports
`apxh.efi` as a `PE32+ executable for EFI (application), x86-64`, while the
kernel and user payloads are static x86-64 ELF executables. The APXH EFI
makefile builds gnu-efi objects under the build tree
`apxh/efi/gnu-efi/<ARCH>` and links against those build-local archives; NUX EFI
builds must not reuse or require source-tree `contrib/gnu-efi/<ARCH>` generated
artifacts.

EFI runtime smoke also needs an x86_64 OVMF/edk2 firmware image for QEMU. In
this reviewed container, `/usr/bin/qemu-system-x86_64` reports `QEMU emulator
version 10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`, and the bounded apt gate
installed only `ovmf=2025.02-8+deb13u1` with no recommends:

```sh
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ovmf=2025.02-8+deb13u1
```

The simulation for that exact package showed one new package, 0 upgrades, and
0 removals; `dpkg-query` now reports `ovmf 2025.02-8+deb13u1 install ok
installed` with `Installed-Size: 16443`. Readable firmware includes
`/usr/share/qemu/OVMF.fd`, `/usr/share/OVMF/OVMF_CODE_4M.fd`, and
`/usr/share/OVMF/OVMF_VARS_4M.fd`. Local FAT-image helpers
(`mformat`/`mcopy`/`mkfs.vfat`) remain separate host conveniences because the
checked-in EFI harness uses QEMU's `fat:rw:` directory backend.

Use the conservative harness to reproduce the current OVMF result, or name
non-standard firmware explicitly on another runner:

```sh
TOOLBIN="$AMD64_TOOLBIN" ./tools/qemu-smoke-amd64-efi.sh

TOOLBIN="$AMD64_TOOLBIN" \
  OVMF_CODE=/path/to/OVMF_CODE.fd \
  OVMF_VARS=/path/to/OVMF_VARS.fd \
  ./tools/qemu-smoke-amd64-efi.sh
```

After initializing submodules in a clean dedicated worktree,
`PATH="$AMD64_TOOLBIN:$PATH" ARCH=amd64 ./tools/build-preflight.sh` passed, and
`TOOLBIN="$AMD64_TOOLBIN" ./tools/qemu-smoke-amd64-efi.sh` stages
`EFI/BOOT/BOOTX64.EFI`, `kernel.elf`, and `user.elf`, boots under OVMF, reaches
APXH/NUX, `IPI!`, userspace, `SYSC0` through `SYSC6`, the `UCTXT_SETA2`
kernel/user markers, `User exited with error code: 42`, and repeated
zero-valued `pnux_entry_pagefault` idle counters before the expected timeout.
The earlier `libnux/alloc.h:192` ACPI/KVA assertion on this path was fixed by
commit `d9ba5f5256cb76c4f6b9207aec775a65d22e0f1a`.

The EFI smoke harness is intentionally conservative about the `contrib/gnu-efi`
submodule: untracked source-tree generated artifacts are ignored by the
build-local gnu-efi object path above, but tracked local modifications in
`contrib/gnu-efi` make the harness stop before QEMU unless
`NUX_EFI_ALLOW_DIRTY_GNUEFI=1` is set. If this preflight fires, preserve the
checkout and inspect with `git -C contrib/gnu-efi status --short
--untracked-files=no` and `git -C contrib/gnu-efi diff`; run the smoke from a
clean/disposable worktree or make an explicit operator decision before
regenerating or cleaning local submodule artifacts.

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
`pnux_entry_pagefault` idle counters. The full default top-level `make` now
uses the same APXH `sbi` selection for riscv64. RISC-V EFI source remains
present but is intentionally not in the default APXH subdir list because its
platform contract is unresolved; manually building `apxh/efi` with Debian
`riscv64-unknown-elf-ld` still fails with `-shared not supported` while linking
`apxh.so`.

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
- APXH `configure` selects `multiboot efi` for `amd64`. With the README-built task cache on `PATH`, the default `amd64-unknown-elf-*` tools and default-PATH `i686-unknown-elf-gcc` pass preflight, configure, `make`, and bounded multiboot QEMU. The standardized local smoke path still uses the host-prefixed `x86_64-linux-gnu-*` override plus the reviewed i686 `TOOLCHAIN32`, and `tools/qemu-smoke-amd64.sh` captures that out-of-tree override flow after the freestanding libec `-no-pie`/`-fno-pie` fixes. Both bounded runs reach APXH/NUX, IPI, userspace, syscall, `UCTXT_SETA2`, exit, and idle counter markers before the expected timeout. EFI-specific amd64 runtime coverage remains separate from the multiboot smoke.
- `libhal_x86/amd64/exe.ld` uses the high-half base `0xffff800000000000`, 512 GiB physmap, 512 GiB KVA, 512 GiB KMEM, 256 MiB PFN cache, and 64 MiB framebuffer mapping.

### riscv64

- Top-level `configure.ac` selects `libhal_riscv` + `libplt_sbi`.
- APXH `configure` selects `sbi` for `riscv64`. The verified runtime path is SBI/DTB: build `libfdt`, `apxh/sbi`, `libhal_riscv`, `libplt_sbi`, `libnux`, `libnux_user`, `tools`, and `example example_qemu`, then run `qemu-system-riscv64 -M virt` through the generated `example` `make qemu` target. The default top-level `make` uses this APXH selection.
- RISC-V EFI source remains under `apxh/efi`, but it is not in the default riscv64 APXH subdir list. Debian `riscv64-unknown-elf-ld` reports `-shared not supported` while linking `apxh.so` if that EFI target is built manually. Do not treat RISC-V EFI as verified by the SBI smoke.
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
`the-nux-amd64-runtime-page-fault-followup`,
`the-nux-riscv64-smoke-tooling-followthrough`, and the amd64 EFI OVMF
follow-through is:

- `./configure --help` advertises `ARCH=i386`, `ARCH=amd64`, and `ARCH=riscv64`.
- QEMU: `/usr/bin/qemu-system-x86_64` and `/usr/bin/qemu-system-riscv64` are present and report QEMU `10.0.8 (Debian 1:10.0.8+ds-0+deb13u1+b2)`.
- Default amd64 preflight/configure/build/QEMU now passes when the README-built `gcc_toolchain_build` cache is prepended to `PATH`; the cache provides `amd64-unknown-elf-*` plus default-PATH `i686-unknown-elf-gcc`.
- Worktree-local submodule initialization succeeded for the follow-through tasks; the current amd64 override path and riscv64 default-tool path are not blocked by submodule checkout or workspace capacity.
- The installed host-prefixed `x86_64-linux-gnu-{gcc,ld,ar,objcopy}` tools plus `TOOLCHAIN32=i686-unknown-elf` from the stable external i386 `TOOLBIN` are sufficient for `ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` preflight, configure, and `make` to pass after commit `51fc152c5d4cd92be9ee0ec9f7410e245bb16dd0` added `-no-pie` to freestanding libec links and commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e` added compile-side `-fno-pie`.
- The baseline amd64 QEMU failure was traced to host GCC default-PIE code generation in freestanding fixed-address objects. With the override path after commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e`, the bounded amd64 QEMU smoke reaches APXH/NUX boot output, `IPI!`, `Hello from userspace, NUX!`, `SYSC0` through `SYSC6`, `UCTXT_SETA2 test passed.`, `UCTXT_SETA2 user test passed.`, `User exited with error code: 42`, and repeated zero-valued `pnux_entry_pagefault` idle counter lines; timeout rc 124 is expected only after those success/idle markers.
- i386 smoke remains passing with the stable external i386 `TOOLBIN`; the follow-up verification recorded `tools/qemu-smoke-i386.sh` rc 0.
- riscv64 default target tools are present from the bounded Debian package path: `binutils-riscv64-unknown-elf` 2.44-3+7+b1, `gcc-riscv64-unknown-elf` 14.2.0+19, and `qemu-system-misc` 1:10.0.8+ds-0+deb13u1+b2, with required dependencies `opensbi` 1.6-1, `qemu-system-riscv`, and `qemu-system-s390x`. With initialized submodules, `ARCH=riscv64 ./tools/build-preflight.sh` passes.
- The verified riscv64 runtime path is the explicit SBI/DTB subset build used by `tools/qemu-smoke-riscv64.sh`; bounded QEMU reaches OpenSBI/APXH/NUX/userspace/syscall/UCTXT/UADDR/KVA markers and repeated zero-valued `pnux_entry_pagefault` idle counters before the expected timeout.
- Default full riscv64 top-level `make` uses the APXH `sbi` path; RISC-V EFI remains unverified and unselected by default after reproducing the old default EFI subdir selection failure: Debian `riscv64-unknown-elf-ld: -shared not supported` while linking `apxh.so`.
- amd64 EFI now has local OVMF runtime coverage rather than a missing-firmware or ACPI/KVA blocker: `ovmf 2025.02-8+deb13u1` is installed outside the repository, the checked-in harness reaches APXH/NUX, IPI, userspace, syscall, `UCTXT_SETA2`, exit, and idle counter markers under OVMF from a clean worktree. The EFI build uses build-local gnu-efi objects and the harness fails early on tracked dirty `contrib/gnu-efi` sources unless explicitly overridden.

Authoritative follow-through logs are under `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-impl/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-review/workspace/review-logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-build-smoke-followthrough-commit/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-impl/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-review/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-runtime-page-fault-followup-commit/workspace/logs`, `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-riscv64-smoke-tooling-followthrough-impl/workspace/logs`, and `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/jobs/the-nux-amd64-default-toolchain-readme-build-impl/workspace/logs`.

The architecture/toolchain follow-through standardized the reviewed `x86_64-linux-gnu`/`i686-unknown-elf` override as the local amd64 smoke path and added `tools/qemu-smoke-amd64.sh`. The later README toolchain follow-through provisioned a real default-prefix cache from a local `gcc_toolchain_build` checkout at commit `eecef0929616a96517a83dab988a8429ad8c62d8`, copied its `install/` artifact into `/home/glguida/mysrc/system/state/the_nux-d552afcb8e35/tasks/the-nux-amd64-default-toolchain-policy/toolchains/gcc_toolchain_build`, and verified default `ARCH=amd64` preflight, configure, `make -j1`, direct bounded `make qemu`, and `tools/qemu-smoke-amd64.sh` forced to `TOOLCHAIN=amd64-unknown-elf TOOLCHAIN32=i686-unknown-elf`. The override smoke path remains verified separately. The riscv64 path is runnable in the current container through the checked-in SBI/DTB smoke harness; remaining open items are CI/toolchain-cache policy for checked-in smoke harnesses and a separate policy/toolchain decision before treating RISC-V APXH EFI as a verified path.
