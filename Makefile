ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
include Kbuild
else
# normal makefile
KDIR ?= /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean
	$(MAKE) -C utils clean

install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

utils:
	$(MAKE) -C utils strip

.PHONY: utils
endif
