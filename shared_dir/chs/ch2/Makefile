# Don't forget to clean files!
obj-m:=ch2_mod_201711391.o
app:=$(obj-m:mod_201711391.o=app_201711391)

#KBUILD_EXTRA_SYMBOLS:=MODPATH/Module.symvers
KERNELDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	gcc -o $(app) $(app).c
debug:
	$(MAKE) EXTRA_CFLAGS=-DLOCAL -C $(KERNELDIR) M=$(PWD) modules
test:
	sudo insmod ch2_mod_201711391.ko
	sudo sh mknod.sh
	sudo ./ch2_app_201711391
	sudo rm /dev/ch2
	sudo rmmod ch2_mod_201711391
	dmesg | tail --lines=20
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm $(app)
