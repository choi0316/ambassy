#!/bin/bash
set -e

pushd linux
export CROSS_COMPILE=aarch64-linux-gnu- 
make ARCH=arm64 xilinx_zynqmp_android_defconfig
make ARCH=arm64 -j$(nproc)
popd
cp linux/arch/arm64/boot/Image boot

