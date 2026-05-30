#!/bin/sh
# Report NUX build/runtime prerequisites for one architecture without
# configuring, building, modifying submodules, or launching QEMU.

set -eu

prog=${0##*/}
status=0
submodule_tmp=

usage() {
    cat <<'EOF'
Usage: tools/build-preflight.sh [--arch ARCH] [--srcdir DIR]

Check the toolchain, QEMU runtime-smoke binary, and checked-in submodule
readiness for a selected NUX architecture without running configure, make, or
QEMU and without modifying submodules.

Environment:
  ARCH        Architecture to check: i386, amd64, or riscv64.
  TOOLCHAIN   Override the target tool prefix checked by configure.
  TOOLCHAIN32 Override the amd64 APXH multiboot 32-bit compiler prefix.
  TOOLBIN     Optional directory prepended to PATH before tool checks.
  NUX_SRCDIR  Source checkout path; default is the parent of this script's tools/.

Default prefixes match configure.ac and apxh/configure.ac:
  ARCH=i386    -> i686-unknown-elf
  ARCH=amd64   -> amd64-unknown-elf, plus TOOLCHAIN32=i686-unknown-elf
  ARCH=riscv64 -> riscv64-unknown-elf

QEMU binaries match example/Makefile.in:
  i386 -> qemu-system-i386
  amd64 -> qemu-system-x86_64
  riscv64 -> qemu-system-riscv64

Exit status:
  0 all checked prerequisites are available
  1 one or more tools, QEMU binaries, or submodules are missing/not ready
  2 invalid usage or unsupported source/architecture
EOF
}

cleanup() {
    if [ -n "$submodule_tmp" ]; then
        rm -f "$submodule_tmp"
    fi
}
trap cleanup EXIT HUP INT TERM

note() {
    echo "$prog: $*"
}

die() {
    echo "$prog: error: $*" >&2
    exit 2
}

missing() {
    echo "$prog: missing: $*" >&2
    status=1
}

check_cmd() {
    cmd=$1
    label=$2
    path=$(command -v "$cmd" 2>/dev/null || true)
    if [ -z "$path" ]; then
        missing "$label: $cmd not found on PATH"
        return
    fi

    version=$({ "$path" --version 2>/dev/null || true; } | sed -n '1p')
    if [ -n "$version" ]; then
        note "ok: $label: $cmd -> $path ($version)"
    else
        note "ok: $label: $cmd -> $path"
    fi
}

check_submodules() {
    if [ ! -f "$srcdir/.gitmodules" ]; then
        note "submodules: no .gitmodules file"
        return
    fi

    if ! command -v git >/dev/null 2>&1; then
        missing "submodules: git not found on PATH, cannot read .gitmodules status"
        return
    fi

    submodule_tmp=${TMPDIR:-/tmp}/nux-build-preflight-submodules.$$
    if ! git -C "$srcdir" submodule status --recursive >"$submodule_tmp" 2>/dev/null; then
        missing "submodules: unable to read git submodule status in $srcdir"
        return
    fi

    if [ ! -s "$submodule_tmp" ]; then
        note "submodules: no submodules reported by git"
        return
    fi

    note "submodules:"
    while IFS= read -r line; do
        [ -n "$line" ] || continue
        state=$(printf '%s' "$line" | cut -c 1)
        rest=${line#?}
        set -- $rest
        commit=${1:-unknown}
        path=${2:-unknown}
        case "$state" in
            ' ')
                note "ok: submodule $path at $commit"
                ;;
            '-')
                missing "submodule $path is uninitialized (expected $commit)"
                ;;
            '+')
                missing "submodule $path is checked out at a commit different from $commit"
                ;;
            'U')
                missing "submodule $path has merge conflicts"
                ;;
            *)
                missing "submodule status is not ready: $line"
                ;;
        esac
    done <"$submodule_tmp"
}

arch=${ARCH:-}
srcdir=${NUX_SRCDIR:-}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -a|--arch)
            [ "$#" -ge 2 ] || die "--arch requires a value"
            arch=$2
            shift 2
            ;;
        --arch=*)
            arch=${1#--arch=}
            shift
            ;;
        --srcdir)
            [ "$#" -ge 2 ] || die "--srcdir requires a value"
            srcdir=$2
            shift 2
            ;;
        --srcdir=*)
            srcdir=${1#--srcdir=}
            shift
            ;;
        *)
            die "unexpected argument: $1"
            ;;
    esac
done

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ -z "$srcdir" ]; then
    srcdir=$(CDPATH= cd -- "$script_dir/.." && pwd -P)
else
    srcdir=$(CDPATH= cd -- "$srcdir" && pwd -P) || die "cannot resolve source directory"
fi

[ -f "$srcdir/configure.ac" ] || die "configure.ac not found in source directory: $srcdir"
[ -f "$srcdir/example/Makefile.in" ] || die "example/Makefile.in not found in source directory: $srcdir"
[ -n "$arch" ] || die "select an architecture with ARCH=i386|amd64|riscv64 or --arch"

if [ -n "${TOOLBIN:-}" ]; then
    [ -d "$TOOLBIN" ] || die "TOOLBIN does not name a directory: $TOOLBIN"
    toolbin_abs=$(CDPATH= cd -- "$TOOLBIN" && pwd -P)
    PATH="$toolbin_abs:$PATH"
    export PATH
    note "TOOLBIN prepended to PATH: $toolbin_abs"
fi

case "$arch" in
    i386)
        tool_prefix=${TOOLCHAIN:-i686-unknown-elf}
        qemu_cmd=qemu-system-i386
        tool_prefix_note="default from configure.ac for ARCH=i386"
        ;;
    amd64)
        tool_prefix=${TOOLCHAIN:-amd64-unknown-elf}
        tool_prefix_32=${TOOLCHAIN32:-i686-unknown-elf}
        qemu_cmd=qemu-system-x86_64
        tool_prefix_note="default from configure.ac for ARCH=amd64"
        ;;
    riscv64)
        tool_prefix=${TOOLCHAIN:-riscv64-unknown-elf}
        qemu_cmd=qemu-system-riscv64
        tool_prefix_note="default from configure.ac for ARCH=riscv64"
        ;;
    *)
        die "unsupported ARCH=$arch (supported: i386, amd64, riscv64)"
        ;;
esac

if [ -n "${TOOLCHAIN:-}" ]; then
    tool_prefix_note="TOOLCHAIN override"
fi

note "source: $srcdir"
note "ARCH=$arch"
note "target tool prefix: $tool_prefix ($tool_prefix_note)"
if [ "$arch" = amd64 ]; then
    if [ -n "${TOOLCHAIN32:-}" ]; then
        tool_prefix_32_note="TOOLCHAIN32 override"
    else
        tool_prefix_32_note="default from apxh/configure.ac for amd64 multiboot"
    fi
    note "amd64 APXH multiboot 32-bit compiler prefix: $tool_prefix_32 ($tool_prefix_32_note)"
fi
note "QEMU runtime-smoke binary: $qemu_cmd (from example/Makefile.in)"

for suffix in gcc ld ar objcopy; do
    check_cmd "$tool_prefix-$suffix" "target $arch $suffix"
done

if [ "$arch" = amd64 ]; then
    check_cmd "$tool_prefix_32-gcc" "amd64 APXH multiboot 32-bit gcc"
fi

check_cmd "$qemu_cmd" "QEMU runtime smoke"
check_submodules

if [ "$status" -eq 0 ]; then
    note "PASS: prerequisites are available for ARCH=$arch"
else
    if [ "$arch" = amd64 ] && [ -z "${TOOLCHAIN:-}" ]; then
        note "amd64 default toolchain policy: default ARCH=amd64 requires real freestanding amd64-unknown-elf tools plus an i686-unknown-elf compiler for APXH multiboot"
        note "amd64 default toolchain policy: build/provide the README gcc_toolchain_build install/bin on PATH, or another true freestanding target-toolchain cache"
        note "amd64 default toolchain policy: do not satisfy the default by silently aliasing x86_64-linux-gnu; use TOOLCHAIN/TOOLCHAIN32 overrides only as an explicit reviewed local smoke path"
        note "amd64 default toolchain policy: see docs/build-and-run.md#amd64-default-toolchain-policy"
    fi
    note "FAIL: one or more prerequisites are missing or not ready for ARCH=$arch"
fi
exit "$status"
