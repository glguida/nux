#!/bin/sh
# Run the reviewed riscv64 SBI/DTB QEMU serial-marker smoke test from an
# out-of-tree build. This intentionally builds the verified SBI runtime
# subset; the full top-level make now uses the same APXH SBI selection for
# riscv64, while RISC-V APXH EFI remains unverified and unselected by default.
#
# Environment:
#   BUILD/NUX_BUILD  Optional out-of-tree build directory. Defaults under /tmp.
#   TIMEOUT          QEMU timeout seconds, default 30.
#   JOBS             make parallelism, default nproc or 1.
#   QEMU_LOG         QEMU serial/output log path, default $BUILD/qemu-riscv64-smoke.log.
#   NUX_SRCDIR       Source checkout path, default parent of this script's tools/ dir.
#   NUX_SMOKE_REUSE_BUILD=1 allows use of a non-empty BUILD directory.
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

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
srcdir=${NUX_SRCDIR:-$(CDPATH= cd -- "$script_dir/.." && pwd -P)}
srcdir=$(CDPATH= cd -- "$srcdir" && pwd -P) || die "cannot resolve source directory"
[ -x "$srcdir/configure" ] || die "configure not found/executable in source directory: $srcdir"

require_cmd make
require_cmd timeout
require_cmd grep
require_cmd awk
require_cmd qemu-system-riscv64
require_cmd riscv64-unknown-elf-gcc
require_cmd riscv64-unknown-elf-ld
require_cmd riscv64-unknown-elf-ar
require_cmd riscv64-unknown-elf-objcopy

if [ -n "${BUILD:-}" ]; then
    build_input=$BUILD
elif [ -n "${NUX_BUILD:-}" ]; then
    build_input=$NUX_BUILD
else
    build_input=${TMPDIR:-/tmp}/the-nux-riscv64-smoke-$$
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
        echo "$prog: warning: source checkout has uninitialized submodules; run git submodule update --init --recursive in this worktree if configure/build fails" >&2
    fi
fi

if [ -z "${JOBS:-}" ]; then
    if command -v nproc >/dev/null 2>&1; then
        JOBS=$(nproc)
    else
        JOBS=1
    fi
fi
TIMEOUT=${TIMEOUT:-30}
PAGEFAULT_IDLE_MIN=${PAGEFAULT_IDLE_MIN:-2}
configure_log=${CONFIGURE_LOG:-$builddir/configure-riscv64.log}
make_log=${MAKE_LOG:-$builddir/make-riscv64-sbi.log}
qemu_log=${QEMU_LOG:-${LOG:-$builddir/qemu-riscv64-smoke.log}}

note "source: $srcdir"
note "build: $builddir"
note "configure log: $configure_log"
note "SBI/DTB subset make log: $make_log"
note "qemu log: $qemu_log"

note "running configure ARCH=riscv64"
if ! (cd "$builddir" && "$srcdir/configure" ARCH=riscv64 >"$configure_log" 2>&1); then
    tail -n 80 "$configure_log" >&2 || true
    die "configure failed; see $configure_log"
fi

: >"$make_log"
run_make_step() {
    subdir=$1
    shift
    note "running make -j$JOBS -C $subdir $*"
    {
        echo
        echo "### make -j$JOBS -C $subdir $*"
    } >>"$make_log"
    if ! (cd "$builddir" && make -j"$JOBS" -C "$subdir" "$@" >>"$make_log" 2>&1); then
        tail -n 160 "$make_log" >&2 || true
        die "make -C $subdir $* failed; see $make_log"
    fi
}

run_make_step libfdt all
run_make_step apxh/sbi all
run_make_step libhal_riscv all
run_make_step libplt_sbi all
run_make_step libnux all
run_make_step libnux_user all
run_make_step tools all
run_make_step example example_qemu

[ -d "$builddir/example" ] || die "build did not create example directory: $builddir/example"
[ -f "$builddir/example/example_qemu" ] || die "build did not create example_qemu: $builddir/example/example_qemu"

note "running timeout --foreground ${TIMEOUT}s make qemu"
set +e
(cd "$builddir/example" && timeout --foreground "${TIMEOUT}s" make qemu >"$qemu_log" 2>&1)
qemu_rc=$?
set -e

if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
    tail -n 180 "$qemu_log" >&2 || true
    die "make qemu failed with rc $qemu_rc; see $qemu_log"
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
OpenSBI v1.6
APXH started.
NUX library (nux)
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
UADDR_VALIDRANGE test passed.
KVA_ALLOC_FREE test passed.
User exited with error code: 42
MARKERS

if grep -Fq -- 'Unexpected Kernel Page Fault' "$qemu_log"; then
    echo "$prog: unexpected kernel page fault marker found" >&2
    missing=1
fi

pagefault_idle_count=$(awk '/pnux_entry_pagefault/ { v=$NF; gsub(/\r/, "", v); if (v == "0") count++ } END { print count + 0 }' "$qemu_log")
if [ "$pagefault_idle_count" -ge "$PAGEFAULT_IDLE_MIN" ]; then
    note "idle pagefault counter evidence: $pagefault_idle_count zero-valued pnux_entry_pagefault lines"
else
    echo "$prog: missing repeated idle pagefault counter evidence: found $pagefault_idle_count, need at least $PAGEFAULT_IDLE_MIN zero-valued pnux_entry_pagefault lines" >&2
    missing=1
fi

if [ "$missing" -ne 0 ]; then
    tail -n 240 "$qemu_log" >&2 || true
    die "QEMU smoke markers missing; see $qemu_log"
fi

if [ "$qemu_rc" -eq 124 ]; then
    note "PASS: QEMU timed out after ${TIMEOUT}s only after all required markers appeared"
else
    note "PASS: QEMU exited with rc $qemu_rc after all required markers appeared"
fi

note "logs left in: $builddir"
