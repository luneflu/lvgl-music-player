#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

OS_NAME="$(uname -s)"
if [ "$OS_NAME" != "Darwin" ]; then
    echo "[WARN] Non-macOS environment detected ($OS_NAME). Continuing with CMake/Make build..."
fi

echo "=========================================="
echo "LVGL macOS / Unix Launcher"
echo "=========================================="

BUILD_DIR="$SCRIPT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring CMake..."
cmake ..

echo "Building project with Make..."
make -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

echo "Build Succeeded!"

BIN_PATH="$SCRIPT_DIR/bin/main"
if [ ! -f "$BIN_PATH" ]; then
    BIN_PATH="$BUILD_DIR/bin/main"
fi

if [ -f "$BIN_PATH" ]; then
    echo "Running $BIN_PATH..."
    "$BIN_PATH"
else
    echo "[ERROR] Binary not found at $BIN_PATH"
    exit 1
fi
