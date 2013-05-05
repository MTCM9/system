LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                                      \
                  BandwidthController.cpp              \
                  CommandListener.cpp                  \
                  DnsProxyListener.cpp                 \
                  NatController.cpp                    \
                  NetdCommand.cpp                      \
                  NetlinkHandler.cpp                   \
                  NetlinkManager.cpp                   \
                  PanController.cpp                    \
                  PppController.cpp                    \
                  ResolverController.cpp               \
                  SecondaryTableController.cpp         \
                  TetherController.cpp                 \
                  ThrottleController.cpp               \
                  oem_iptables_hook.cpp                \
                  logwrapper.c                         \
                  main.cpp                             \


LOCAL_MODULE:= netd

LOCAL_C_INCLUDES := $(KERNEL_HEADERS) \
                    $(LOCAL_PATH)/../bluetooth/bluedroid/include \
                    $(LOCAL_PATH)/../bluetooth/bluez-clean-headers \
                    external/openssl/include \
                    external/stlport/stlport \
                    bionic \
                    $(call include-path-for, libhardware_legacy)/hardware_legacy

LOCAL_CFLAGS :=

LOCAL_SHARED_LIBRARIES := libstlport libsysutils libcutils libnetutils \
                          libcrypto libhardware_legacy

ifdef BOARD_SOFTAP_DEVICE
    LOCAL_CFLAGS += -D__BYTE_ORDER_LITTLE_ENDIAN
    LOCAL_STATIC_LIBRARIES := libhostapdcli
    LOCAL_C_INCLUDES += $(WILINK_INCLUDES)
    LOCAL_SRC_FILES += SoftapControllerTI.cpp
else ifeq ($(WIFI_DRIVER_MODULE_NAME),ar6000)
    ifneq ($(WIFI_DRIVER_MODULE_PATH),rfkill)
        LOCAL_CFLAGS += -DWIFI_MODULE_PATH=\"$(WIFI_DRIVER_MODULE_PATH)\"
    endif
    LOCAL_C_INCLUDES += external/wpa_supplicant external/hostapd
    LOCAL_SRC_FILES += SoftapControllerATH.cpp
else ifeq ($(BOARD_WLAN_DEVICE),libra)
    LOCAL_C_INCLUDES += external/wpa_supplicant external/hostapd
    LOCAL_SRC_FILES += SoftapControllerQC.cpp
else ifeq ($(BOARD_SOFTAP_DEVICE_TI),true)
    LOCAL_SRC_FILES += SoftapControllerTI.cpp
    LOCAL_C_INCLUDES += external/libnl-headers
    LOCAL_STATIC_LIBRARIES += libnl_2
else
    LOCAL_SRC_FILES += SoftapController.cpp
endif

ifneq ($(BOARD_HOSTAPD_DRIVER),)
  LOCAL_CFLAGS += -DHAVE_HOSTAPD
  ifneq ($(BOARD_HOSTAPD_DRIVER_NAME),)
    LOCAL_CFLAGS += -DHOSTAPD_DRIVER_NAME=\"$(BOARD_HOSTAPD_DRIVER_NAME)\"
  endif
endif

ifeq ($(BOARD_WLAN_DEVICE),libra)
LOCAL_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES) libwpa_client
endif

ifneq ($(BOARD_HOSTAPD_NO_ENTROPY),)
  LOCAL_CFLAGS += -DHOSTAPD_NO_ENTROPY
endif

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES) libbluedroid
  LOCAL_CFLAGS := $(LOCAL_CFLAGS) -DHAVE_BLUETOOTH
endif

ifeq ($(WIFI_DRIVER_HAS_LGE_SOFTAP),true)
  LOCAL_CFLAGS += -DLGE_SOFTAP
endif

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:=          \
                  ndc.c \

LOCAL_MODULE:= ndc

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_CFLAGS := 

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)

