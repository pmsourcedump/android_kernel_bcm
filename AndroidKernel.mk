#Android makefile to build kernel as a part of Android Build
PERL            = perl

KERNEL_OUT := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT)/.config
TARGET_PREBUILT_INT_KERNEL := $(KERNEL_OUT)/arch/arm/boot/zImage-dtb
KERNEL_HEADERS_INSTALL := $(KERNEL_OUT)/usr
KERNEL_MODULES_INSTALL := system
KERNEL_MODULES_OUT := $(TARGET_OUT)/lib/modules
KERNEL_IMG=$(KERNEL_OUT)/arch/arm/boot/Image
mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
KERNEL_PATH := kernel/$(notdir $(patsubst %/,%,$(dir $(mkfile_path))))
KERNEL_GCC_PATH := $(abspath prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin)
PATH := $(KERNEL_GCC_PATH):${PATH}

ifeq ($(TARGET_USES_UNCOMPRESSED_KERNEL),true)
  $(info Using uncompressed kernel)
  TARGET_PREBUILT_KERNEL := $(KERNEL_OUT)/piggy
else
  TARGET_PREBUILT_KERNEL := $(TARGET_PREBUILT_INT_KERNEL)
endif

define mv-modules
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
mpath=`dirname $$mdpath`;\
ko=`find $$mpath/kernel -type f -name *.ko`;\
for i in $$ko; do mv $$i $(KERNEL_MODULES_OUT)/; done;\
fi
endef

define clean-module-folder
mdpath=`find $(KERNEL_MODULES_OUT) -type f -name modules.dep`;\
if [ "$$mdpath" != "" ];then\
  mpath=`dirname $$mdpath`; rm -rf $$mpath;\
fi
endef

$(KERNEL_OUT):
	mkdir -p $(KERNEL_OUT)

$(KERNEL_CONFIG): $(KERNEL_OUT)
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- $(KERNEL_DEFCONFIG)

$(KERNEL_OUT)/piggy : $(TARGET_PREBUILT_INT_KERNEL)
	$(hide) gunzip -c $(KERNEL_OUT)/arch/arm/boot/compressed/piggy.gzip > $(KERNEL_OUT)/piggy

# $(KERNEL_HEADERS_INSTALL)
$(TARGET_PREBUILT_INT_KERNEL): $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- zImage-dtb dtbs
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- modules
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) INSTALL_MOD_PATH=../../$(KERNEL_MODULES_INSTALL) INSTALL_MOD_STRIP=1 ARCH=arm CROSS_COMPILE=arm-eabi- modules_install
	$(mv-modules)
	$(clean-module-folder)

#$(KERNEL_HEADERS_INSTALL): $(KERNEL_OUT) $(KERNEL_CONFIG)
#	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- headers_install

kerneltags: $(KERNEL_OUT) $(KERNEL_CONFIG)
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- tags

kernelconfig: $(KERNEL_OUT) $(KERNEL_CONFIG)
	env KCONFIG_NOTIMESTAMP=true \
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- menuconfig
	env KCONFIG_NOTIMESTAMP=true \
	$(MAKE) -C $(KERNEL_PATH) O=../../$(KERNEL_OUT) ARCH=arm CROSS_COMPILE=arm-eabi- savedefconfig
	cp $(KERNEL_OUT)/defconfig kernel/$(KERNEL_PATH)/arch/arm/configs/$(KERNEL_DEFCONFIG)

%.dtb: $(TARGET_PREBUILT_INT_KERNEL)
	cp $(KERNEL_OUT)/arch/arm/boot/dts/$(@F) $(PRODUCT_OUT)/$(@F)
