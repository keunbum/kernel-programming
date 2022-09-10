obj-m:=ku.o
s_lib:=$(obj-m:.o=_s_lib)
app:=$(obj-m:.o=_app)

KERNELDIR:=/lib/modules/$(shell uname -r)/build
KDIR:=~/linux-rpi/
ARM:=ARCH=arm CROSS_COMPILE=/usr/bin/arm-linux-gnueabi-
PWD:=$(shell pwd)
#CC:=gcc

default:
	#$(MAKE) EXTRA_CFLAGS=-DLOCAL -C $(KDIR) M=$(PWD) $(ARM) modules
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) modules
	arm-linux-gnueabi-gcc -c app/$(app).c lib/$(s_lib).c
	arm-linux-gnueabi-gcc -o $(app) $(app).o $(s_lib).o

#	$(MAKE) EXTRA_CFLAGS=-DLOCAL -C $(KERNELDIR) M=$(PWD) modules
#	$(CC) -c app/$(app).c lib/$(s_lib).c
#	$(CC) -o $(app) $(app).o $(s_lib).o
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm $(app)
clean_pi:
	$(MAKE) -C $(KDIR) M=$(PWD) $(ARM) clean
	rm $(app)
test:
	sudo dmesg -C
	sudo insmod $(obj-m:.o=.ko)
	sudo sh mknod.sh
	sudo ./ku_app
test_clean:
	sudo rm /dev/ku_sa
	sudo rmmod $(obj-m:.o=.ko)
scp:
	scp $(obj-m:.o=.ko) $(app) mknod.sh Makefile pi@192.168.1.66:~/Documents/a/2/
