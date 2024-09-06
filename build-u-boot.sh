#!/bin/bash
set -e

pushd u-boot/
export CROSS_COMPILE=aarch64-linux-gnu-
make xilinx_zynqmp_zcu102_rev1_0_defconfig
make -j$(nproc)
popd
cp u-boot/u-boot.elf boot
