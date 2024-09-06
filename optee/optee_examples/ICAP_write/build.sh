#!/usr/bin/env bash

# optee_test build

export CROSS_COMPILE_TA=aarch64-linux-gnu-
export TA_DEV_KIT_DIR=$PWD/../../optee_os/out/arm/export-ta_arm64
mkdir -p ${TA_DEV_KIT_DIR}
cp -r $PWD/../../optee_os/out/arm-plat-zynqmp/export-ta_arm64/* ${TA_DEV_KIT_DIR}
export OPTEE_CLIENT_EXPORT=$PWD/../../optee_client/out/export
export COMPILE_NS_USER=64
export OPTEE_CLIENT_EXPORT=$PWD/../../optee_client/libs/arm64-v8a
export OPTEE_CLIENT_INCLUDE_EXPORT=$PWD/../../optee_client/public

# hello world build
export BUILD_OPTEE_MK=$PWD/../../hello_world/android_ta.mk
export TEEC_EXPORT=$PWD/../../optee_client/out
export HOST_CROSS_COMPILE=aarch64-linux-gnu-
export TA_CROSS_COMPILE=aarch64-linux-gnu-
export TA_DEV_KIT_DIR=$PWD/../../optee_os/out/arm-plat-zynqmp/export-ta_arm64
echo "making.."
make -j$PARALLELISM CROSS_COMPILE_HOST=aarch64-linux-gnu-
echo "ndk-build.."
ndk-build NDK_PROJECT_PATH=$PWD APP_BUILD_SCRIPT=$PWD/Android.mk APP_PLATFORM=android-21 APP_ABI=arm64-v8a

echo "packaging hello world.."
#mkdir -p ${OUTDIR}/optee/ta/
#pushd $TOP_DIR/optee/hello_world
rm -rf output/ta/*
rm -rf output/ta/.ta.ld.d
rm -rf output/bin/*

mkdir -p $PWD/../../optee_examples/output/$TA_NAME/ta
mkdir -p $PWD/../../optee_examples/output/$TA_NAME/bin
find ./ -name "*.ta*" -exec cp {} $PWD/../../optee_examples/output/$TA_NAME/ta \;
cp $PWD/libs/arm64-v8a/tee_embassy $PWD/../../optee_examples/output/$TA_NAME/bin
