#!/bin/bash
set -e

pushd optee/optee_os
rm -rf out
export ARCH=arm
export PLATFORM=zynqmp-zcu102
export AARCH64_CROSS_COMPILE=aarch64-linux-gnu-
export AARCH32_CROSS_COMPILE=arm-linux-gnueabihf-
make CFG_TEE_CORE_LOG_LEVEL=4 CFG_TEE_TA_LOG_LEVEL=4 CFG_ARM64_core=y WARNS=0 -j$(nproc) 
popd
cp optee/optee_os/out/arm-plat-zynqmp/core/tee.elf boot

