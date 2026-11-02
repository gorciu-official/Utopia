#!/usr/bin/env bash

set -Eeuo pipefail

echo "This is the version. Please review it."

echo

grep -ir "#define UTOPIA_VERSION" include/constants.h | head -n 3 
grep -ir "#define UTOPIA_SUBVERSION" include/constants.h | head -n 1
grep -ir "#define UTOPIA_BETA_STATE" include/constants.h | head -n 1

echo

readonly OUTPUT_DIR="target/kernel.release-binaries"

read -r -p "Cross compile prefix for x86_64: " x86_64_cross_compile
read -r -p "Cross compile prefix for RISC-V 64: " riscv64_cross_compile

compile() {
    local arch="$1"
    local cross_compile="$2"
    local bootloader="$3"
    local build_dir="target/kernel.release.${arch}.${bootloader}"
    local make_args="BOOTLOADER=${bootloader}" "ARCH=${arch}" "CROSS_COMPILE=${cross_compile}"

    echo
    echo "==> Building ${arch} with ${bootloader}"
    echo "    Cross compile prefix: ${cross_compile:-<native>}"

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

compile x86_64 "$x86_64_cross_compile" limine
compile x86_64 "$x86_64_cross_compile" grub
compile riscv64 "$riscv64_cross_compile" limine

echo
echo "==> All builds completed successfully"
echo "==> Release binaries:"
ls -lah "$OUTPUT_DIR"
