# Don't forget to clean files!
obj-m:=ch1_mod2_201711391.o

KBUILD_EXTRA_SYMBOLS:=/home/keunbum/Documents/embedded_system/assignments/ch1/mod1/Module.symvers
KERNELDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
debug:
	$(MAKE) EXTRA_CFLAGS=-DLOCAL -C $(KERNELDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
