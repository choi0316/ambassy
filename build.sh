
        export CROSS_COMPILE_TA=aarch64-linux-gnu-
        export TA_DEV_KIT_DIR=/home/vagrant/workspace/pros/optee/optee_os/out/arm/export-ta_arm64
        mkdir -p ${TA_DEV_KIT_DIR}
        cp -r /home/vagrant/workspace/pros/optee/optee_os/out/arm-plat-zynqmp/export-ta_arm64/* ${TA_DEV_KIT_DIR}
        export OPTEE_CLIENT_EXPORT=/home/vagrant/workspace/pros/optee/optee_client/out/export/
        export COMPILE_NS_USER=64
        pushd $TOP_DIR/optee/optee_test
        make -j$PARALLELISM CROSS_COMPILE_HOST=aarch64-linux-gnu-

        export OPTEE_CLIENT_EXPORT=/home/vagrant/workspace/pros/optee/optee_client/libs/arm64-v8a
        export OPTEE_CLIENT_INCLUDE_EXPORT=/home/vagrant/workspace/pros/optee/optee_client/public
        ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_PLATFORM=android-21 APP_ABI=arm64-v8a
        popd
