#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

#include "../ku_c.h"

static int dev_id;

int ku_s_open(int msgflg)
{
	printf("ku_s_open()\n");
	KU_READWRITE data = {
		.msgflg = msgflg,
	};
	dev_id = open(KU_DEV_PATH, O_RDWR);
	if (dev_id < 0) {
		perror("ku_s_open: open() failed");
		return -1;
	}
	return ioctl(dev_id, KU_OPEN, &data);
}

int ku_s_close()
{
	printf("ku_s_close()\n");
	KU_READWRITE data = {
	};
	return ioctl(dev_id, KU_CLOSE, &data);
}

int ku_s_read(void *msgp, int msgsz, int msgflg)
{
	printf("ku_s_read()\n");
	KU_READWRITE data = {
		.msgp 	= msgp,
		.msgsz 	= msgsz,
		.msgflg = msgflg,
	};
	return ioctl(dev_id, KU_READ, &data);
}

int ku_a_write(void *msgp, int msgsz)
{
	KU_READWRITE data = {
		.msgp 	= msgp,
		.msgsz 	= msgsz,
	};
	return ioctl(dev_id, KU_WRITE, &data);
}
