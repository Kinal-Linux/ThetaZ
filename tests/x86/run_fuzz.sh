#!/bin/bash
# Differential fuzz: native x86-64 golden vs ThetaZ interpreter.
set -e
cd "$(dirname "$0")/../.."

gcc -O0 -no-pie tests/x86/fuzz_ops.c -o /tmp/opencode/fuzz_native
/tmp/opencode/fuzz_native > /tmp/opencode/fuzz_golden.txt

i686-w64-mingw32-gcc -msse2 tests/x86/fuzz_ops.c -o /tmp/opencode/fuzz_ops.exe -lkernel32
timeout 60 ./build/ThetaZ /tmp/opencode/fuzz_ops.exe > /tmp/opencode/fuzz_guest.txt

if diff -q /tmp/opencode/fuzz_golden.txt /tmp/opencode/fuzz_guest.txt > /dev/null; then
    echo "FUZZ PASS ($(wc -l < /tmp/opencode/fuzz_golden.txt) probes)"
else
    echo "FUZZ MISMATCH"
    diff /tmp/opencode/fuzz_golden.txt /tmp/opencode/fuzz_guest.txt | head -40
fi
