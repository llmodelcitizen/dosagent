#!/bin/bash
# Install OpenWatcom v2 portable to /opt/watcom
set -e

WATCOM_DIR="/opt/watcom"
ZIP_URL="http://openwatcom.org/ftp/source/ow_portable_v2_stable.zip"

# --- Uninstall ---
if [ "$1" = "--uninstall" ]; then
    if [ ! -d "$WATCOM_DIR" ]; then
        echo "Nothing to uninstall: $WATCOM_DIR does not exist"
        exit 0
    fi

    # Sanity check: make sure this looks like an OpenWatcom install,
    # not something the user repurposed the directory for
    if [ ! -f "$WATCOM_DIR/binl/wcl" ] && [ ! -f "$WATCOM_DIR/readme.w32" ]; then
        echo "ERROR: $WATCOM_DIR exists but does not look like an OpenWatcom install" >&2
        echo "Refusing to remove. Inspect and remove manually if needed." >&2
        exit 1
    fi

    echo "Removing OpenWatcom at $WATCOM_DIR ..."
    sudo rm -rf "$WATCOM_DIR"
    echo "OpenWatcom uninstalled."
    exit 0
fi

# --- Install ---

# Already installed?
if [ -x "$WATCOM_DIR/binl/wcl" ]; then
    echo "OpenWatcom already installed at $WATCOM_DIR"
    "$WATCOM_DIR/binl/wcl" 2>&1 | head -1
    exit 0
fi

echo "Installing OpenWatcom v2 to $WATCOM_DIR ..."

TMP_ZIP=$(mktemp /tmp/ow_portable_XXXXXX.zip)
trap 'rm -f "$TMP_ZIP"' EXIT

curl -L -o "$TMP_ZIP" "$ZIP_URL"

sudo mkdir -p "$WATCOM_DIR"
sudo unzip -q -o "$TMP_ZIP" -d "$WATCOM_DIR"
sudo chown -R "$(id -u):$(id -g)" "$WATCOM_DIR"

# Verify
if [ -x "$WATCOM_DIR/binl/wcl" ]; then
    echo "Installed successfully:"
    "$WATCOM_DIR/binl/wcl" 2>&1 | head -1
else
    echo "ERROR: wcl not found after extraction" >&2
    exit 1
fi
