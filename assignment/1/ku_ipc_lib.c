#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>

#include "ku_ipc.h"

/* some part of the ku_ipc_local.h */
/* ------------------------------------------------------------------------------------------ */
	
#define KU_DEV_NAME			"ku_ipc"
#define KU_DEV_PATH 			"/dev/"KU_DEV_NAME

#define KU_MINOR			0xC1
#define KU_BASIC			0x20
#define KU_OP				0x80

typedef struct _KU_READWRITE {
	int msqid;
	struct msg_buf *msgp;
	int msgsz;
	long msgtyp;
	int msgflg;
} KU_READWRITE;

#define KU_OPEN				_IOWR(KU_MINOR,KU_BASIC+1,KU_READWRITE)
#define KU_CLOSE			_IOWR(KU_MINOR,KU_BASIC+2,KU_READWRITE)
#define KU_WRITE			_IOWR(KU_MINOR,KU_OP+1,KU_READWRITE)
#define KU_READ				_IOWR(KU_MINOR,KU_OP+2,KU_READWRITE)

/* ------------------------------------------------------------------------------------------ */

static int dev_id;

int ku_msgget(int key, int msgflg)
{
	KU_READWRITE data = {
		.msqid 	= key,
		.msgflg = msgflg,
	};
	dev_id = open(KU_DEV_PATH, O_RDWR);
	if (dev_id < 0) {
		perror("ku_msgget: open() failed");
		return -1;
	}
	return ioctl(dev_id, KU_OPEN, &data);
}

int ku_msgclose(int msqid)
{
	KU_READWRITE data = {
		.msqid = msqid,
	};
	return ioctl(dev_id, KU_CLOSE, &data);
}

int ku_msgsnd(int msqid, void *msgp, int msgsz, int msgflg)
{
	KU_READWRITE data = {
		.msqid 	= msqid,
		.msgp 	= msgp,
		.msgsz 	= msgsz,
		.msgflg = msgflg,
	};
	return ioctl(dev_id, KU_WRITE, &data);
}

int ku_msgrcv(int msqid, void *msgp, int msgsz, long msgtyp, int msgflg)
{
	KU_READWRITE data = {
		.msqid 	= msqid,
		.msgp 	= msgp,
		.msgsz 	= msgsz,
		.msgtyp = msgtyp,
		.msgflg = msgflg,
	};
	return ioctl(dev_id, KU_READ, &data);
}
