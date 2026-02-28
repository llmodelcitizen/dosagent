#!/bin/bash
# Build DOSAGENT.EXE — DOS Remote Agent for FreeDOS/86Box

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WATCOM=${WATCOM:-$SCRIPT_DIR/../scripts/watcom}
export WATCOM
export PATH="$WATCOM/binl64:$WATCOM/binl:$PATH"
export INCLUDE="$WATCOM/h"
export EDPATH="$WATCOM/eddat"
export WIPFC="$WATCOM/wipfc"

PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# Auto-install Watt-32 TCP/IP library (32-bit flat model) if not present
if [ ! -f "$SCRIPT_DIR/../scripts/lib/watt32/lib/wattcpwf.lib" ]; then
    echo "=== Watt-32 (32-bit) not found, installing... ==="
    bash "$SCRIPT_DIR/../scripts/install_tcplib.sh" --32bit
fi

echo "=== Building DOSAGENT.EXE (DOS 32-bit, flat model, DOS/32A) ==="
echo "WATCOM=$WATCOM"

# Clean previous build
wmake -f agent/Makefile clean

# Build
mkdir -p agent/bin
wmake -f agent/Makefile

if [ -f agent/bin/dosagent.exe ]; then
    echo "=== Build successful: agent/bin/dosagent.exe ==="
    ls -la agent/bin/dosagent.exe
else
    echo "=== Build FAILED ==="
    exit 1
fi
