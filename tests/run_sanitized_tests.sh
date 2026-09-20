#!/usr/bin/env bash
#
# run_sanitized_tests.sh — run all tests under ASan/UBSan.
#
# Usage:
#   ./tests/run_sanitized_tests.sh ./build-asan
#
# The build directory must have been configured with
# -Dsanitize=address,undefined.

set -euo pipefail

BUILD_DIR="${1:-build-asan}"

if [ ! -d "$BUILD_DIR" ]; then
    echo "error: build directory '$BUILD_DIR' not found" >&2
    echo "       run: meson setup $BUILD_DIR -Dsanitize=address,undefined" >&2
    exit 1
fi

# ASan options: fail hard, show full traces
export ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:print_stacktrace=1:halt_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:abort_on_error=1"
export LSAN_OPTIONS="suppressions=$(pwd)/tests/lsan.supp"

echo "==> Running all tests under ASan/UBSan..."
meson test -C "$BUILD_DIR" --print-errorlogs --timeout-multiplier 3

echo
echo "==> Sanitized test run complete."
