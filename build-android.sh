#!/bin/bash
set -e
cd android
wget https://www.xilinx.com/publications/products/tools/mali-400-userspace.tar
mkdir -p tmp_mali && \
  tar -xf mali-400-userspace.tar -C ./tmp_mali && \
  mkdir -p vendor/xilinx/zynqmp/proprietary && \
  cp -r tmp_mali/mali/Android/android-6.0.1/MALI-userspace/r6p2-01rel0/* vendor/xilinx/zynqmp/proprietary/ && \
  rm -rf tmp_mali/vmlinuz.old
tree vendor/xilinx/zynqmp/proprietary/
sleep 1
cp ../HOST_x86_common.mk build/core/clang/HOST_x86_common.mk 
source build/envsetup.sh
lunch zcu102-eng
make -j$(nproc)

echo "========================"
echo "Done building Android!!!"
echo "========================"
