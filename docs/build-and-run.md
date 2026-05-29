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
- `--disable-werror`: intended to disable `-Werror` (see backlog: the configure inputs currently test `enable_relax`, so this needs verification/fixing).
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
- APXH is intended to include multiboot and EFI for `amd64`; however the generated `apxh/configure` currently contains a malformed `case` branch for `amd64`/`riscv64`, so this needs repair before treating the generated script as authoritative.
- `libhal_x86/amd64/exe.ld` uses the high-half base `0xffff800000000000`, 512 GiB physmap, 512 GiB KVA, 512 GiB KMEM, 256 MiB PFN cache, and 64 MiB framebuffer mapping.

### riscv64

- Top-level `configure.ac` selects `libhal_riscv` + `libplt_sbi`.
- APXH source supports SBI/DTB (`apxh/sbi/*`) and has EFI RISC-V code in `apxh/efi/apxhefi/efi_md.c`.
- `libhal_riscv/exe.ld` uses the same high-half base, 512 GiB physmap/KVA/KMEM, 256 MiB PFN cache, and 32 MiB framebuffer mapping.
- Treat RISC-V EFI as unverified: APXH EFI records `PLT_ACPI`, while `libplt_sbi` requires `PLT_DTB` (`apxh/efi/apxhefi/efi_md.c`, `libplt_sbi/sbi.c`).

## Verification from this documentation pass

Commands run in the task worktree:

```sh
./configure --help >/tmp/the-nux-configure-help.txt
mkdir -p build-docs-check
cd build-docs-check
../configure ARCH=i386 > /tmp/the-nux-build-configure-i386.txt 2>&1
```

Results:

- `./configure --help` succeeded and produced 78 lines of help in `/tmp/the-nux-configure-help.txt`.
- `../configure ARCH=i386` failed before target-tool checks because the container PATH did not contain a host C compiler:

```text
checking for gcc... no
checking for cc... no
checking for cl.exe... no
configure: error: no acceptable C compiler found in $PATH
```

Because configure failed, `make -j"$(nproc)"` was not actionable in this environment. The first actionable blocker is to install or provide a host compiler, then provide the target cross toolchains and initialized submodules.
