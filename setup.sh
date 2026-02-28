#!/bin/bash
# Full setup: install toolchain, build TCP library, compile agent
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# --- Uninstall ---
if [ "$1" = "--uninstall" ]; then
    echo "=== Uninstalling dosagent ==="
    bash "$SCRIPT_DIR/scripts/install_client.sh" --uninstall
    bash "$SCRIPT_DIR/scripts/install_tcplib.sh" --uninstall
    bash "$SCRIPT_DIR/scripts/install_compiler.sh" --uninstall
    echo "=== Uninstall complete ==="
    exit 0
fi

echo "=== Step 1/4: Installing OpenWatcom compiler ==="
bash "$SCRIPT_DIR/scripts/install_compiler.sh"

echo "=== Step 2/4: Building Watt-32 TCP library ==="
bash "$SCRIPT_DIR/scripts/install_tcplib.sh" --32bit

echo "=== Step 3/4: Compiling DOSAGENT ==="
bash "$SCRIPT_DIR/agent/build.sh"

echo "=== Step 4/4: Installing 'da' command ==="
bash "$SCRIPT_DIR/scripts/install_client.sh"
