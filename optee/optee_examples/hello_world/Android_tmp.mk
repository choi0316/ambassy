###################### optee-hello-world ######################
LOCAL_PATH := $(call my-dir)

#jwseo
OPTEE_CLIENT_EXPORT = /home/jwseo/workspace/PrOS_ver6/optee/optee_client/out/export

include $(CLEAR_VARS)
LOCAL_CFLAGS += -DANDROID_BUILD
LOCAL_CFLAGS += -Wall

LOCAL_SRC_FILES += host/main.c

LOCAL_C_INCLUDES := /home/jwseo/workspace/PrOS_ver6/optee/optee_examples/hello_world/ta/include \
		/home/jwseo/workspace/PrOS_ver6/optee/optee_client/out/export/include \

#jwseo
LOCAL_LDLIBS=-L$(OPTEE_CLIENT_EXPORT) -lteec
#LOCAL_SHARED_LIBRARIES := libteec
LOCAL_MODULE := optee_example_hello_world
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(LOCAL_PATH)/ta/Android.mk
