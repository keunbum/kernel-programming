#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>

#include "ku_c.h"

#ifndef __KU__
#define __KU__

#define KU_MAXMSG			10
#define KU_MAXVOL			(128 * KU_MAXMSG)

#define KU_NO				1

struct node {
	struct list_head nodes;
	int msgsz;
	struct msg_buf msg;
};

typedef unsigned short ku_pid_t;

struct ku_msq_ds {
	unsigned int pid_cnt;		/* count of pids */
	unsigned int msg_cbytes;	/* current number of bytes on queue */
	unsigned int msg_qbytes;	/* max number of bytes on queue */
	unsigned int msg_qnum;		/* number of messages in queue */
	ku_pid_t msg_lspid;		/* last msgsnd pid */
	ku_pid_t msg_lrpid;		/* last msgrcv pid */
};

#define KU_ULTRA_TRIG 			17
#define KU_ULTRA_ECHO 			18

#define NONE				0
#define FIRST				1
#define SECOND				2
#define YET				3

#define C				58

#endif // __KU__
