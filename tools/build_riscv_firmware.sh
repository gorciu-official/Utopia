#!/usr/bin/env -S bash --posix

# if anyone is wondering, yes, this script is 
# written by AI, sorry

set -euo pipefail

REPO="osdev0/edk2-ovmf-nightly"
OUT_DIR="target/third-party.riscv64.edk2"
RAW_DIR="$OUT_DIR/raw"

mkdir -p "$RAW_DIR"

echo "Fetching latest EDK2 OVMF release..."

API="https://api.github.com/repos/$REPO/releases/latest"

URL="$(
    curl -fsSL "$API" |
        grep '"browser_download_url"' |
        grep 'edk2-ovmf.tar.xz' |
        sed -E 's/.*"browser_download_url": "([^"]+)".*/\1/'
)"

if [ -z "$URL" ]; then
    echo "Could not find edk2-ovmf.tar.xz"
    exit 1
fi

echo "Downloading:"
echo "  $URL"

curl -fL "$URL" -o "$RAW_DIR/edk2-ovmf.tar.xz"

echo "Extracting..."

tar -xJf \
    "$RAW_DIR/edk2-ovmf.tar.xz" \
    -C "$RAW_DIR"

echo "Searching for RISC-V firmware..."

CODE="$(find "$RAW_DIR" -type f -name 'ovmf-code-riscv64.fd' | head -n1)"
VARS="$(find "$RAW_DIR" -type f -name 'ovmf-vars-riscv64.fd' | head -n1)"

if [ -z "$CODE" ] || [ -z "$VARS" ]; then
    echo "Could not find RISC-V firmware."
    echo
    echo "Available .fd files:"
    find "$RAW_DIR" -type f -name '*.fd'
    exit 1
fi

cp "$CODE" "$OUT_DIR/RISCV_VIRT_CODE.fd"
cp "$VARS" "$OUT_DIR/RISCV_VIRT_VARS.fd"

echo
echo "RISC-V UEFI firmware installed:"
ls -lh \
    "$OUT_DIR/RISCV_VIRT_CODE.fd" \
    "$OUT_DIR/RISCV_VIRT_VARS.fd"
