#!/bin/sh
# Build and run the amd64 APXH EFI example under QEMU/OVMF.
#
# This harness is intentionally strict: it does not treat a missing firmware
# package as a skip, and it accepts QEMU timeout only after the normal amd64
# APXH/NUX/userspace serial markers have appeared.
#
# Environment:
#   TOOLBIN           Optional path prepended to PATH (for amd64-unknown-elf).
#   TOOLCHAIN         64-bit target tool prefix, default amd64-unknown-elf.
#   TOOLCHAIN32       32-bit APXH multiboot compiler prefix passed to configure,
#                     default i686-unknown-elf.
#   OVMF_CODE         x86_64 OVMF/edk2 firmware image. Auto-detected when installed.
#   OVMF_VARS         Optional OVMF variable-store template. Auto-detected when installed.
#   BUILD/NUX_BUILD   Optional out-of-tree build directory. Defaults under /tmp.
#   TIMEOUT           QEMU timeout seconds, default 30.
#   QEMU_LOG          QEMU serial/output log path, default $BUILD/qemu-amd64-efi-smoke.log.
#   NUX_SRCDIR        Source checkout path, default parent of this script's tools/ dir.
#   NUX_SMOKE_REUSE_BUILD=1 allows use of a non-empty BUILD directory.
#   NUX_EFI_ALLOW_DIRTY_GNUEFI=1 intentionally tests tracked local changes in
#                     contrib/gnu-efi; by default this smoke aborts on them.
#   PAGEFAULT_IDLE_MIN minimum zero-valued pnux_entry_pagefault counter lines, default 2.

set -eu

prog=${0##*/}

die() {
    echo "$prog: error: $*" >&2
    exit 1
}

note() {
    echo "$prog: $*"
}

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found on PATH: $1"
}

first_existing() {
    for path do
        if [ -r "$path" ]; then
            printf '%s\n' "$path"
            return 0
        fi
    done
    return 1
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
srcdir=${NUX_SRCDIR:-$(CDPATH= cd -- "$script_dir/.." && pwd -P)}
srcdir=$(CDPATH= cd -- "$srcdir" && pwd -P) || die "cannot resolve source directory"
[ -x "$srcdir/configure" ] || die "configure not found/executable in source directory: $srcdir"

if [ -n "${TOOLBIN:-}" ]; then
    [ -d "$TOOLBIN" ] || die "TOOLBIN does not name a directory: $TOOLBIN"
    PATH="$TOOLBIN:$PATH"
    export PATH
fi

TOOLCHAIN=${TOOLCHAIN:-amd64-unknown-elf}
TOOLCHAIN32=${TOOLCHAIN32:-i686-unknown-elf}
TIMEOUT=${TIMEOUT:-30}
PAGEFAULT_IDLE_MIN=${PAGEFAULT_IDLE_MIN:-2}

require_cmd make
require_cmd timeout
require_cmd grep
require_cmd awk
require_cmd qemu-system-x86_64
require_cmd "$TOOLCHAIN-gcc"
require_cmd "$TOOLCHAIN-ld"
require_cmd "$TOOLCHAIN-ar"
require_cmd "$TOOLCHAIN-objcopy"

ovmf_code=${OVMF_CODE:-}
if [ -z "$ovmf_code" ]; then
    ovmf_code=$(first_existing \
        /usr/share/OVMF/OVMF.fd \
        /usr/share/qemu/OVMF.fd \
        /usr/share/edk2/ovmf/OVMF.fd \
        /usr/share/edk2/x64/OVMF.fd \
        /usr/share/OVMF/OVMF_CODE_4M.fd \
        /usr/share/OVMF/OVMF_CODE.fd \
        /usr/share/edk2/ovmf/OVMF_CODE.fd \
        /usr/share/edk2/x64/OVMF_CODE.4m.fd \
        /usr/share/edk2/x64/OVMF_CODE.fd 2>/dev/null || true)
fi
[ -n "$ovmf_code" ] || die "OVMF/edk2 x86_64 firmware not found; install/provide an OVMF package or set OVMF_CODE=/path/to/OVMF_CODE.fd"
[ -r "$ovmf_code" ] || die "OVMF_CODE is not readable: $ovmf_code"

ovmf_vars=${OVMF_VARS:-}
if [ -z "$ovmf_vars" ]; then
    ovmf_vars=$(first_existing \
        /usr/share/OVMF/OVMF_VARS_4M.fd \
        /usr/share/OVMF/OVMF_VARS.fd \
        /usr/share/edk2/ovmf/OVMF_VARS.fd \
        /usr/share/edk2/x64/OVMF_VARS.4m.fd \
        /usr/share/edk2/x64/OVMF_VARS.fd 2>/dev/null || true)
fi
if [ -n "$ovmf_vars" ] && [ ! -r "$ovmf_vars" ]; then
    die "OVMF_VARS is not readable: $ovmf_vars"
fi

if [ -n "${BUILD:-}" ]; then
    build_input=$BUILD
elif [ -n "${NUX_BUILD:-}" ]; then
    build_input=$NUX_BUILD
else
    build_input=${TMPDIR:-/tmp}/the-nux-amd64-efi-smoke-$$
fi

build_parent=$(dirname -- "$build_input")
build_base=$(basename -- "$build_input")
mkdir -p "$build_parent"
build_parent=$(CDPATH= cd -- "$build_parent" && pwd -P) || die "cannot resolve build parent: $build_parent"
builddir=$build_parent/$build_base

case "$builddir/" in
    "$srcdir/"*) die "BUILD must be outside the source checkout; got $builddir" ;;
esac

if [ -d "$builddir" ] && [ "$(find "$builddir" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]; then
    if [ "${NUX_SMOKE_REUSE_BUILD:-0}" != "1" ]; then
        die "BUILD exists and is not empty: $builddir (choose a fresh directory or set NUX_SMOKE_REUSE_BUILD=1)"
    fi
fi
mkdir -p "$builddir"

if git -C "$srcdir" submodule status --recursive >/dev/null 2>&1; then
    if git -C "$srcdir" submodule status --recursive | grep -q '^-'; then
        echo "$prog: warning: source checkout has uninitialized submodules; run git submodule update --init --recursive in this worktree before EFI smoke" >&2
    fi
fi

gnuefi_src=$srcdir/contrib/gnu-efi
if [ -d "$gnuefi_src" ] && git -C "$gnuefi_src" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    gnuefi_dirty=0
    git -C "$gnuefi_src" diff --quiet --no-ext-diff -- || gnuefi_dirty=1
    git -C "$gnuefi_src" diff --cached --quiet --no-ext-diff -- || gnuefi_dirty=1
    if [ "$gnuefi_dirty" -ne 0 ]; then
        if [ "${NUX_EFI_ALLOW_DIRTY_GNUEFI:-0}" != "1" ]; then
            git -C "$gnuefi_src" status --short --untracked-files=no >&2 || true
            die "contrib/gnu-efi has tracked local modifications; EFI smoke would compile those sources. Use a clean/disposable worktree or set NUX_EFI_ALLOW_DIRTY_GNUEFI=1 to test them intentionally"
        fi
        echo "$prog: warning: continuing with tracked local contrib/gnu-efi modifications because NUX_EFI_ALLOW_DIRTY_GNUEFI=1" >&2
    fi
fi

configure_log=${CONFIGURE_LOG:-$builddir/configure-amd64-efi.log}
make_log=${MAKE_LOG:-$builddir/make-amd64-efi.log}
qemu_log=${QEMU_LOG:-${LOG:-$builddir/qemu-amd64-efi-smoke.log}}
esp=$builddir/esp

note "source: $srcdir"
note "build: $builddir"
if [ -n "${TOOLBIN:-}" ]; then
    note "toolbin: $TOOLBIN"
else
    note "toolbin: not set; using $TOOLCHAIN tools already on PATH"
fi
note "toolchain: $TOOLCHAIN"
note "toolchain32 passed to configure: $TOOLCHAIN32"
note "OVMF_CODE: $ovmf_code"
if [ -n "$ovmf_vars" ]; then
    note "OVMF_VARS template: $ovmf_vars"
else
    note "OVMF_VARS template: not found; using -bios with OVMF_CODE"
fi
note "configure log: $configure_log"
note "make log: $make_log"
note "qemu log: $qemu_log"

note "running configure ARCH=amd64 TOOLCHAIN=$TOOLCHAIN TOOLCHAIN32=$TOOLCHAIN32"
if ! (cd "$builddir" && "$srcdir/configure" ARCH=amd64 TOOLCHAIN="$TOOLCHAIN" TOOLCHAIN32="$TOOLCHAIN32" >"$configure_log" 2>&1); then
    tail -n 80 "$configure_log" >&2 || true
    die "configure failed; see $configure_log"
fi

note "building APXH EFI loader and example payloads"
if ! (
    cd "$builddir"
    {
        echo "# amd64 EFI smoke build"
        date -u +%Y-%m-%dT%H:%M:%SZ
        echo "## make -j1 -C apxh/efi all"
        make -j1 -C apxh/efi all
        for dir in libhal_x86 libplt_acpi libnux libnux_user example/kern example/user; do
            echo
            echo "## make -j1 -C $dir all"
            make -j1 -C "$dir" all
        done
    } >"$make_log" 2>&1
); then
    tail -n 140 "$make_log" >&2 || true
    die "EFI build failed; see $make_log"
fi

apxh_efi=$builddir/apxh/efi/apxh.efi
kernel_elf=$builddir/example/kern/example
user_elf=$builddir/example/user/exuser
[ -f "$apxh_efi" ] || die "APXH EFI artifact missing: $apxh_efi"
[ -f "$kernel_elf" ] || die "kernel payload missing: $kernel_elf"
[ -f "$user_elf" ] || die "user payload missing: $user_elf"

mkdir -p "$esp/EFI/BOOT"
cp "$apxh_efi" "$esp/EFI/BOOT/BOOTX64.EFI"
cp "$kernel_elf" "$esp/kernel.elf"
cp "$user_elf" "$esp/user.elf"

note "staged EFI System Partition directory: $esp"

set +e
if [ -n "$ovmf_vars" ]; then
    vars_copy=$builddir/OVMF_VARS.fd
    cp "$ovmf_vars" "$vars_copy"
    (cd "$builddir" && timeout --foreground "${TIMEOUT}s" qemu-system-x86_64 \
        -drive if=pflash,format=raw,readonly=on,file="$ovmf_code" \
        -drive if=pflash,format=raw,file="$vars_copy" \
        -hda "fat:rw:$esp" \
        -serial mon:stdio -nographic -no-reboot >"$qemu_log" 2>&1)
    qemu_rc=$?
else
    (cd "$builddir" && timeout --foreground "${TIMEOUT}s" qemu-system-x86_64 \
        -bios "$ovmf_code" \
        -hda "fat:rw:$esp" \
        -serial mon:stdio -nographic -no-reboot >"$qemu_log" 2>&1)
    qemu_rc=$?
fi
set -e

if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
    tail -n 160 "$qemu_log" >&2 || true
    die "QEMU EFI smoke failed with rc $qemu_rc; see $qemu_log"
fi

missing=0
while IFS= read -r marker; do
    [ -n "$marker" ] || continue
    if grep -Fq -- "$marker" "$qemu_log"; then
        note "marker found: $marker"
    else
        echo "$prog: missing marker: $marker" >&2
        missing=1
    fi
done <<'MARKERS'
APXH started.
NUX library (nux)
IPI!
Hello from userspace, NUX!
SYSC0 test passed.
SYSC1 test passed.
SYSC2 test passed.
SYSC3 test passed.
SYSC4 test passed.
SYSC5 test passed.
SYSC6 test passed.
UCTXT_SETA2 test passed.
UCTXT_SETA2 user test passed.
UADDR_MEMSET test passed.
UADDR_MEMSET user test passed.
User exited with error code: 42
MARKERS

if grep -Fq -- 'Unexpected Kernel Page Fault' "$qemu_log"; then
    echo "$prog: unexpected kernel page fault marker found" >&2
    missing=1
fi

pagefault_idle_count=$(awk '/pnux_entry_pagefault/ && $NF == "0" { count++ } END { print count + 0 }' "$qemu_log")
if [ "$pagefault_idle_count" -ge "$PAGEFAULT_IDLE_MIN" ]; then
    note "idle pagefault counter evidence: $pagefault_idle_count zero-valued pnux_entry_pagefault lines"
else
    echo "$prog: missing repeated idle pagefault counter evidence: found $pagefault_idle_count, need at least $PAGEFAULT_IDLE_MIN zero-valued pnux_entry_pagefault lines" >&2
    missing=1
fi

if [ "$missing" -ne 0 ]; then
    tail -n 220 "$qemu_log" >&2 || true
    die "QEMU EFI smoke markers missing; see $qemu_log"
fi

if [ "$qemu_rc" -eq 124 ]; then
    note "PASS: QEMU EFI timed out after ${TIMEOUT}s only after all required markers appeared"
else
    note "PASS: QEMU EFI exited with rc $qemu_rc after all required markers appeared"
fi

note "logs left in: $builddir"
