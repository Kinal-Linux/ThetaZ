#!/bin/bash
# Build ThetaZ and run its test suite.
# Usage: ./build.sh [Debug|Release]

set -e

BUILD_TYPE="${1:-Debug}"
ROOT="$(cd "$(dirname "$0")" && pwd)"

cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$ROOT/build" -j"$(nproc)"

echo
echo "Running test suite..."
"$ROOT/tests/run_tests.sh" "$ROOT/build/ThetaZ"
