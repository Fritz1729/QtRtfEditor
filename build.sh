#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${2:-build}"

echo "=== QtRtfEditor build ($BUILD_TYPE) ==="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cd "$BUILD_DIR"
ctest --output-on-failure -C "$BUILD_TYPE"
echo "=== Done ==="
