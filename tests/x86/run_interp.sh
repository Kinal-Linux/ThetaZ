#!/bin/bash
# Interpreter opcode test harness (32-bit guest).
#
# Compiles the interp_ops*.c fixtures with 32-bit MinGW (-msse2 so GCC emits
# the SSE2 encodings the interpreter must handle, incl. F3 0F 7E MOVQ loads),
# runs them through ThetaZ and asserts "ALL PASS" + exit 0.
#
# Usage: tests/x86/run_interp.sh [path-to-ThetaZ-binary]

set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${1:-$ROOT/build/ThetaZ}"
CC="i686-w64-mingw32-gcc"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0
FAIL=0

check() {
    local name="$1"
    if eval "$2"; then
        PASS=$((PASS + 1))
        echo "PASS: $name"
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $name"
    fi
}

for t in interp_ops1 interp_ops2 interp_ops3; do
    "$CC" -msse2 "$ROOT/tests/x86/$t.c" -o "$WORK/$t.exe" -lkernel32 2>/dev/null \
        && OUT=$("$BIN" "$WORK/$t.exe" 2>/dev/null) \
        && check "$t (all-checks)" 'echo "$OUT" | grep -q "ALL PASS"' \
        || check "$t (all-checks)" 'false'
done

echo "interp: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
