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
- riscv64: `SBI` and `EFI`

The full default-toolchain runtime-smoke path in the current task environment
is i386/multiboot. The reviewed amd64 override path also passes a bounded
example QEMU smoke when the stable external i386 `TOOLBIN` is prepended and
`ARCH=amd64 TOOLCHAIN=x86_64-linux-gnu TOOLCHAIN32=i686-unknown-elf` is used;
QEMU times out only after the expected success/idle markers because the demo
does not shut the emulator down. The earlier amd64 failure caused by host
GCC default-PIE code generation in freestanding fixed-address objects is fixed
by commit `8e1a5365dbdb2277fe9a2853f272765cbc6dd98e`; default `ARCH=amd64`
still needs default `amd64-unknown-elf-*` tools and a default-PATH
`i686-unknown-elf-gcc`. The riscv64 target-toolchain, QEMU, and EFI
platform-contract verification remain open; see
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

For `ARCH=riscv64`, APXH configure currently selects both `sbi` and `efi`.
The SBI/DTB path is the coherent configured runtime path. RISC-V EFI source
exists and uses `gnu-efi`, but its platform-descriptor contract and full
build/runtime flow are unverified. If a non-`gcc_toolchain_build` toolchain
fails while building EFI, treat that as an open portability/configure-policy
issue rather than hand-editing generated build files; see
[docs/hardware-support.md](docs/hardware-support.md) and
[docs/backlog.md](docs/backlog.md).

