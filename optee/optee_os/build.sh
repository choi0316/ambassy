export CROSS_COMPILE64=aarch64-linux-gnu-
export CROSS_COMPILE32=arm-linux-gnueabihf-
export PLATFORM=zynqmp-zcu102
export CFG_TEE_CORE_LOG_LEVEL=3
export CFG_TEE_TA_LOG_LEVEL=3
export CFG_ARM64_core=y
rm -rf out
make -j$PARALLELISM WARNS=0
## temp patch: to be fixed by proper memory mapping of TEE
