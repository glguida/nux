# NUX
* _nux, nucis : .gen. plur. nucerum for nucum, f. etym. dub., a nut._

For a high-level introduction on NUX and its motivation, check [this article](https://nux.tlbflush.org/post/2024_12_24_notes_nux/).

## What it is
NUX is a framework to prototype kernels and related userspace programs that run on real, modern hardware.
Currently supported architectures are x86_64, riscv64 and i386.

A kernel, with NUX, is nothing more than a C file with a `main` function and other functions
that defines how the kernel behaves on certain events:

- `main_ap` called by a secondary processor when it is booted
- `entry_ipi` called when an inter-processor interrupt is received by the current CPU
- `entry_alarm` called when the platform timer expires
- `entry_irq` called when the platform issues an IRQ.
- `entry_sysc` to handle user space system calls.
- `entry_ex` to handle user space exceptions
- `entry_pf` to handle user space page faults

See the [example kernel](example/kern/main.c) and
[example userspace](example/user/main.c).

NUX also provides _libnux_, a runtime kernel support library to handle platform and memory,
and _libec_ a basic embedded C library based on the NetBSD libc.

On the userspace side, NUX provides `libnux_user`, that defines the syscall interface of the kernel,
and _libec_, the same embedded C library used by the kernel side.

NUX kernels are booted by APXH (uppercase for αρχη, or _beginning_ in ancient greek).
Configured APXH boot paths are architecture-specific:
- i386: `multiboot`
- amd64: `multiboot` and `EFI`
- riscv64: `SBI`

The full default-toolchain runtime-smoke paths in the current task environment
are i386/multiboot and amd64/multiboot when the README-built
`gcc_toolchain_build` install bin directory is prepended to `PATH`. For amd64,
that means real `amd64-unknown-elf-*` tools plus a default-PATH
`i686-unknown-elf-gcc`. The local amd64 host-prefix smoke path remains an
explicit reviewed override:
`ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf`. The
checked-in `tools/qemu-smoke-amd64.sh` harness repeats that out-of-tree override
flow when the runner provides the host-prefixed x86_64 tools and
`qemu-system-x86_64`; QEMU times out only after the expected success/idle
markers because the demo does not shut the emulator down. The earlier amd64
failure caused by host GCC default-PIE code generation in freestanding
fixed-address objects is fixed by commit
`8e1a5365dbdb2277fe9a2853f272765cbc6dd98e`. The riscv64 default
target-toolchain/QEMU path is the reviewed SBI/DTB path. The amd64 EFI APXH
loader builds with the same README-built default amd64 cache, and this
container now has `/usr/bin/qemu-system-x86_64` (QEMU `10.0.8 (Debian
1:10.0.8+ds-0+deb13u1+b2)`) plus `ovmf 2025.02-8+deb13u1` installed outside
the repository. With `/usr/share/qemu/OVMF.fd` or split
`/usr/share/OVMF/OVMF_CODE_4M.fd` plus `/usr/share/OVMF/OVMF_VARS_4M.fd`,
`tools/qemu-smoke-amd64-efi.sh` boots under OVMF and reaches the amd64
APXH/NUX, IPI, userspace, syscall, `UCTXT_SETA2`, `UADDR_MEMSET`, `KMAP_UPDATE`, exit, and idle markers before
the expected timeout. The EFI build uses build-local `gnu-efi` objects rather
than source-tree generated submodule artifacts, and the harness stops early on
tracked dirty `contrib/gnu-efi` sources unless explicitly overridden. RISC-V EFI
platform-contract verification remains open; see
[docs/hardware-support.md](docs/hardware-support.md).

## Documentation

The `docs/` tree contains a source-backed architecture, build/run, porting,
hardware-support, debugging, memory, userspace, Murgia-integration, and backlog
baseline. Start with [`docs/README.md`](docs/README.md).

## Building NUX

You need to have and embedded ELF target compiler. If you're building for riscv, be sure to read instructions
below.

_If you have already your own embedded ELF compiler (such as amd64-unknown-elf-gcc or amd64-elf-gcc), you
can skip the following_.

### 1. Building the toolchain

`gcc_toolchain_build` is a super simple Makefile to automate building GCC for embedded targets.

If you want to build at once all the compilers and tools required to build all platforms supported by nux,
do the following: _(it'll take quite a while)_

```
git clone https://github.com/glguida/gcc_toolchain_build
cd gcc_toolchain_build
make populate
make amd64-unknown-elf-gcc
make i686-unknown-elf-gcc
make riscv64-unknown-elf-gcc
export PATH=$PWD/install/bin
cd ..
```

### 2. Compile NUX

Building NUX is as simple as using `configure` and `make`. To check the
selected architecture's target tools, QEMU binary, and submodule readiness
without configuring, building, updating submodules, or launching QEMU, run
`tools/build-preflight.sh` first; see [docs/build-and-run.md](docs/build-and-run.md)
for i386, amd64, and riscv64 examples.

```
git clone https://github.com/glguida/nux
cd nux
git submodule update --init --recursive
mkdir build
cd build
../configure ARCH=i386
make -j
```

Now you can run the demo:

```
cd example
make qemu
```

**Note if you are using your own compiler:**

If you need to specify which compiler to use, pass the `TOOLCHAIN` and `TOOLCHAIN32` parameters to
`configure`.

E.g., suppose you have `x86_64-elf-gcc` and `i686-elf-gcc` (AMD64 requires both):

```
../nux/configure ARCH=amd64 TOOLCHAIN=amd64-elf TOOLCHAIN32=i686-elf
```

or

```
../nux/configure ARCH=i386 TOOLCHAIN=i686-elf
```

**Note for RISCV64:**

For `ARCH=riscv64`, APXH configure selects `sbi` by default. The SBI/DTB
path is the coherent configured runtime path and is the path used by the
default top-level `make`. RISC-V EFI source exists under `apxh/efi` and uses
`gnu-efi`, but it is not selected by default because its platform-descriptor
contract and full build/runtime flow are unverified. Debian
`riscv64-unknown-elf-ld` reports `-shared not supported` when manually building
that EFI target; treat EFI as a separate portability/configure-policy decision
rather than hand-editing generated build files. See
[docs/hardware-support.md](docs/hardware-support.md) and
[docs/backlog.md](docs/backlog.md).

