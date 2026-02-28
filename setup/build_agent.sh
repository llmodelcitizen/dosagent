#!/bin/bash
# Build DOSAGENT.EXE — DOS Remote Agent for FreeDOS/86Box
# Requires OpenWatcom v2 installed at /opt/watcom

set -e

WATCOM=${WATCOM:-/opt/watcom}
export WATCOM
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
export EDPATH="$WATCOM/eddat"
export WIPFC="$WATCOM/wipfc"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Auto-install Watt-32 TCP/IP library (32-bit flat model) if not present
if [ ! -f "$PROJECT_DIR/lib/watt32/lib/wattcpwf.lib" ]; then
    echo "=== Watt-32 (32-bit) not found, installing... ==="
    bash "$SCRIPT_DIR/install_tcplib.sh" --32bit
fi

echo "=== Building DOSAGENT.EXE (DOS 32-bit, flat model, DOS/32A) ==="
echo "WATCOM=$WATCOM"

# Clean previous build
wmake -f setup/Makefile clean

# Build
wmake -f setup/Makefile

if [ -f dosagent.exe ]; then
    echo "=== Build successful: dosagent.exe ==="
    ls -la dosagent.exe
else
    echo "=== Build FAILED ==="
    exit 1
fi
