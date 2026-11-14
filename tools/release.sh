#!/usr/bin/env bash

set -Eeuo pipefail

echo "This is the current version of the kernel defined in header files. Please make sure it matches the release you are trying to make."

echo

grep -ir "#define UTOPIA_VERSION" include/constants.h | head -n 3 
grep -ir "#define UTOPIA_SUBVERSION" include/constants.h | head -n 1
grep -ir "#define UTOPIA_BETA_STATE" include/constants.h | head -n 1

echo

echo "Please also review versions listed in: docs/security.md"

echo 

readonly OUTPUT_DIR="target/kernel.release-binaries"

compile() {
    local arch="$1"
    local bootloader="$2"
    local build_dir="target/kernel.${arch}.${bootloader}"
    local make_args="BOOTLOADER=${bootloader}" "ARCH=${arch}"

    echo
    echo "==> Building ${arch} with ${bootloader}"

    make $make_args clean

    make $make_args

    echo "==> Packaging build artifacts"

    mkdir -p "$OUTPUT_DIR"

    cp "${build_dir}/Utopia.bin" \
        "${OUTPUT_DIR}/utopia-${arch}-${bootloader}.bin"

    cp "${build_dir}/Utopia.iso" \
        "${OUTPUT_DIR}/utopia-demo-${arch}-${bootloader}.iso"

    echo "==> ${arch}/${bootloader} build completed successfully"
}

make defconfig

compile x86_64 limine
compile x86_64 grub
compile riscv64 limine

echo
echo "==> All builds completed successfully"
echo "==> Release binaries:"
ls -lah "$OUTPUT_DIR"
