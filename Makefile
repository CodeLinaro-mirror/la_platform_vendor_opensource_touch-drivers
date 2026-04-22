
TOUCH_ROOT=$(ROOTDIR)vendor/qcom/opensource/touch-drivers
KBUILD_OPTIONS := TOUCH_ROOT=$(TOUCH_ROOT) CONFIG_MSM_TOUCH=m

ifeq ($(TARGET_SUPPORT),genericarmv8)
	KBUILD_OPTIONS += CONFIG_ARCH_WAIPIO=y
endif

ifeq ($(TARGET_SUPPORT),genericarmv8)
	KBUILD_OPTIONS += CONFIG_ARCH_PINEAPPLE=y
endif

#
# Makefile for the Goodix gt9xx touchscreen driver.
#
#subdir-ccflags-y += -DDEBUG
obj-$(CONFIG_TOUCHSCREEN_GT9XX) += gt9xx_core.o
gt9xx_core-y := gt9xx.o gt9xx_update.o goodix_tool.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
