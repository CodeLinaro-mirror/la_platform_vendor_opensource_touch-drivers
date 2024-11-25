CUR_MKFILE=$(abspath $(lastword $(MAKEFILE_LIST)))
TOUCH_ROOT=$(dir $(CUR_MKFILE))
KBUILD_OPTIONS := TOUCH_ROOT=$(TOUCH_ROOT)
KBUILD_OPTIONS += MODNAME?=touch_dlkm

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
