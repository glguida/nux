#!/bin/sh
# Run the reviewed i386 QEMU serial-marker smoke test from an out-of-tree build.
#
# Environment:
#   TOOLBIN           Optional path to i686-unknown-elf toolchain bin directory.
#   BUILD/NUX_BUILD  Optional out-of-tree build directory. Defaults under /tmp.
#   TIMEOUT           QEMU timeout seconds, default 20.
#   JOBS              make parallelism, default nproc or 1.
#   QEMU_LOG          QEMU serial/output log path, default $BUILD/qemu-i386-smoke.log.
#   NUX_SRCDIR        Source checkout path, default parent of this script's tools/ dir.
#   NUX_SMOKE_REUSE_BUILD=1 allows use of a non-empty BUILD directory.

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

if [ -n "${TOOLBIN:-}" ]; then
    [ -d "$TOOLBIN" ] || die "TOOLBIN does not name a directory: $TOOLBIN"
    PATH="$TOOLBIN:$PATH"
    export PATH
fi

require_cmd make
require_cmd timeout
require_cmd grep
require_cmd qemu-system-i386
require_cmd i686-unknown-elf-gcc
require_cmd i686-unknown-elf-ld
require_cmd i686-unknown-elf-ar
require_cmd i686-unknown-elf-objcopy

if [ -n "${BUILD:-}" ]; then
    build_input=$BUILD
elif [ -n "${NUX_BUILD:-}" ]; then
    build_input=$NUX_BUILD
else
    build_input=${TMPDIR:-/tmp}/the-nux-i386-smoke-$$
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
TIMEOUT=${TIMEOUT:-20}
configure_log=${CONFIGURE_LOG:-$builddir/configure-i386.log}
make_log=${MAKE_LOG:-$builddir/make-i386.log}
qemu_log=${QEMU_LOG:-${LOG:-$builddir/qemu-i386-smoke.log}}

note "source: $srcdir"
note "build: $builddir"
if [ -n "${TOOLBIN:-}" ]; then
    note "toolbin: $TOOLBIN"
else
    note "toolbin: not set; using i686-unknown-elf tools already on PATH"
fi
note "configure log: $configure_log"
note "make log: $make_log"
note "qemu log: $qemu_log"

note "running configure ARCH=i386"
if ! (cd "$builddir" && "$srcdir/configure" ARCH=i386 >"$configure_log" 2>&1); then
    tail -n 80 "$configure_log" >&2 || true
    die "configure failed; see $configure_log"
fi

note "running make -j$JOBS"
if ! (cd "$builddir" && make -j"$JOBS" >"$make_log" 2>&1); then
    tail -n 120 "$make_log" >&2 || true
    die "make failed; see $make_log"
fi

[ -d "$builddir/example" ] || die "build did not create example directory: $builddir/example"

note "running timeout --foreground ${TIMEOUT}s make qemu"
set +e
(cd "$builddir/example" && timeout --foreground "${TIMEOUT}s" make qemu >"$qemu_log" 2>&1)
qemu_rc=$?
set -e

if [ "$qemu_rc" -ne 0 ] && [ "$qemu_rc" -ne 124 ]; then
    tail -n 160 "$qemu_log" >&2 || true
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
APXH started.
NUX library (nux)
Hello from userspace, NUX!
SYSC0 test passed.
SYSC6 test passed.
UCTXT_SETA2 test passed.
UCTXT_SETA2 user test passed.
UADDR_MEMSET test passed.
UADDR_MEMSET user test passed.
UADDR_VALIDRANGE test passed.
KVA_ALLOC_FREE test passed.
KMAP_UPDATE test passed.
User exited with error code: 42
MARKERS

if [ "$missing" -ne 0 ]; then
    tail -n 200 "$qemu_log" >&2 || true
    die "QEMU smoke markers missing; see $qemu_log"
fi

if [ "$qemu_rc" -eq 124 ]; then
    note "PASS: QEMU timed out after ${TIMEOUT}s only after all required markers appeared"
else
    note "PASS: QEMU exited with rc $qemu_rc after all required markers appeared"
fi

note "logs left in: $builddir"
