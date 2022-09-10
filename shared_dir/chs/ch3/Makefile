# Don't forget to clean files!
obj-m:=ch3_mod_201711391.o
app:=$(obj-m:mod_201711391.o=app_201711391)

#KBUILD_EXTRA_SYMBOLS:=MODPATH/Module.symvers
KERNELDIR:=/lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
	gcc -o $(app) $(app).c
debug:
	$(MAKE) EXTRA_CFLAGS=-DCH3_LOCAL -C $(KERNELDIR) M=$(PWD) modules
	gcc -DCH3_LOCAL -o $(app) $(app).c
test:
	sudo insmod ch3_mod_201711391.ko
	sudo sh mknod.sh
	sudo sh mknod2.sh
#	sudo ./ch3_app_201711391 < in
#	dmesg | tail --lines=80
clean:
	sudo rm /dev/ch3_dev
	sudo rm /dev/ch3_dev2
	sudo rmmod ch3_mod_201711391
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm $(app)
