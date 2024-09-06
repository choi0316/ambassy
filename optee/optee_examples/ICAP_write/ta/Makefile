CFG_TEE_TA_LOG_LEVEL ?= 4
CPPFLAGS += -DCFG_TEE_TA_LOG_LEVEL=$(CFG_TEE_TA_LOG_LEVEL)
CPPFLAAGS += -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/arch/arm/include -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/arch/arm/plat-zynqmp -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/include
#LDFLAAGS += -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/arch/arm/include -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/arch/arm/plat-zynqmp -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/include

#-I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/arch/arm/plat-zynqmp -I/home/dihwang/pros/PrOS_android6/optee/optee_os/core/include
# The UUID for the Trusted Application
#BINARY=8aaaf200-2450-11e4-abe2-0002a5d5c51b
BINARY=c2b4d887-52d7-4e60-9d33-561840411458

-include $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk

ifeq ($(wildcard $(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk), )
clean:
	@echo 'Note: $$(TA_DEV_KIT_DIR)/mk/ta_dev_kit.mk not found, cannot clean TA'
	@echo 'Note: TA_DEV_KIT_DIR=$(TA_DEV_KIT_DIR)'
endif
