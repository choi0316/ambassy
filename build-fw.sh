#!/bin/bash

set -e
cd xsdk-workspace
cd fsbl_bsp
make all 1>/dev/null
cd ..
pushd fsbl/Debug
make all 1>/dev/null
popd
pushd pmufw_bsp
make all 1>/dev/null
popd
pushd pmufw/Debug
make all 1>/dev/null
popd
cd ..

cp xsdk-workspace/fsbl/Debug/fsbl.elf boot
cp xsdk-workspace/pmufw/Debug/pmufw.elf boot
