ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk APP_PLATFORM=android-21 APP_ABI=arm64-v8a 
EXPORT_DIR=`pwd`/out make CROSS_COMPILE=aarch64-linux-gnu-
