#!/bin/bash
set -ex

SETTING=/opt/Xilinx/Vivado/2018.2/settings64.sh

if [[ -f $SETTING ]]; then
  source $SETTING
else
  source /opt/Xilinx/SDK/2018.2/settings64.sh
fi

#wget https://dl.google.com/android/repository/android-ndk-r16b-linux-x86_64.zip
unzip android-ndk-r16b-linux-x86_64.zip
#rm android-ndk-r16b-linux-x86_64.zip
export PATH=$PWD/android-ndk-r16b:$PATH

pushd optee/optee_os
./build.sh
popd
cp optee/optee_os/out/arm-plat-zynqmp/core/tee.elf boot

pushd optee/optee_client
./build.sh
popd

./build-linux.sh
./build-ta.sh
