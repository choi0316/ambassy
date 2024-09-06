#!/bin/bash
# Should run in the container
set -e
echo "1. Build Linux (Image)"
./build-linux.sh 1>/dev/null
echo "2. Build TF (bl31.elf)"
./build-tf.sh 1>/dev/null
echo "3. Build u-boot (u-boot.elf)"
./build-u-boot.sh 1> /dev/null
echo "4. Build optee os (tee.elf)"
./build-optee.sh 1> /dev/null
echo "5. pmufw and fsbl (fsbl.elf, pmufw.elf)"
./build-fw.sh 1> /dev/null
echo "6. Generate boot.bif"
source /home/android/Xilinx/SDK/2018.2/settings64.sh
cd boot
bootgen -arch zynqmp -image ../boot.bif -o BOOT.BIN -w
cd ..
cp ../embassy/project/zcu102_base_trd.runs/impl_1/zcu102_base_trd_wrapper.bit boot
rm -f boot/:gen
echo "7. Done!!!"
