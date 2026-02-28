#!/bin/bash
# Full setup: install toolchain, build TCP library, compile agent
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Step 1/3: Installing OpenWatcom compiler ==="
bash "$SCRIPT_DIR/scripts/install_compiler.sh"

echo "=== Step 2/3: Building Watt-32 TCP library ==="
bash "$SCRIPT_DIR/scripts/install_tcplib.sh" --32bit

echo "=== Step 3/3: Compiling DOSAGENT ==="
bash "$SCRIPT_DIR/agent/build.sh"
