#!/bin/sh
set -eu

if command -v apt >/dev/null 2>&1; then
    sudo apt install build-essential pkg-config libgtk-4-dev pciutils
else
    echo "This helper currently supports Debian/Ubuntu/Linux Mint via apt."
    exit 1
fi
