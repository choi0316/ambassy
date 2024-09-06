#/bin/bash
set -e
pushd arm-tf/
export CROSS_COMPILE=aarch64-linux-gnu-
make PLAT=zynqmp RESET_TO_BL31=1 ERROR_DEPRECATED=1 SPD=opteed bl31 -j$(nproc)
popd
cp arm-tf/build/zynqmp/release/bl31/bl31.elf boot


