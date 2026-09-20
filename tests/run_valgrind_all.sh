#!/usr/bin/env bash
#
# run_valgrind_all.sh — build and run every test under Valgrind.
#
# Usage:
#   ./tests/run_valgrind_all.sh ./build
#
# Requires the build directory to have been configured with
# -Dvalgrind=true -Dbuild_tests=true.

set -euo pipefail

BUILD_DIR="${1:-build}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "error: build directory '$BUILD_DIR' not found" >&2
    echo "       run: meson setup $BUILD_DIR -Dvalgrind=true -Dbuild_tests=true" >&2
    exit 1
fi

echo "==> Running all Valgrind test suites..."
meson test -C "$BUILD_DIR" --suite valgrind --print-errorlogs --timeout-multiplier 10

echo
echo "==> Valgrind run complete."
