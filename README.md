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

See the [example kernel](https://github.com/glguida/nux/blob/main/example/kern/main.c) and
[exmaple userspace](https://github.com/glguida/nux/blob/main/example/user/main.c).

NUX also provides _libnux_, a runtime kernel support library to handle platform and memory,
and _libec_ a basic embedded C library based on the NetBSD libc.

On the userspace side, NUX provides `libnux_user`, that defines the syscall interface of the kernel,
and _libec_, the same embedded C library used by the kernel side.

NUX kernels are booted by APXH (uppercase for αρχη, or _beginning_ in ancient greek).
APXH currently supports:
- `EFI` on i386, amd64 and riscv64
- `multiboot` on i386 and amd64
- `SBI` (riscv64).

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

Building NUX is as simple as using `configure` and `make`.

```
git clone https://github.com/glguida/nux
cd nux
git submodule update --init --recursive
mkdir build
cd build
../configure ARCH=i386
make -j
````

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

NUX will attempt to build APXH with EFI support on riscv64. This is done using `gnu-efi`.
If you are _not_ using the toolchain built with `gcc_toolchain_build`, this will fail.

If you still intend to use another toolchain, then you have to edit apxh/Makefile.in,
removing 'efi' from the list of `SUBDIRS`.

