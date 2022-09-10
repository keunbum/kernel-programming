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

#define ku_fmt(fmt)			"ku_ipc: "fmt

#define ku_err(fmt, ...)		printk(KERN_ERR ku_fmt(fmt), ##__VA_ARGS__)
#define ku_notice(fmt, ...)		printk(KERN_NOTICE ku_fmt(fmt), ##__VA_ARGS__)
#define ku_info(fmt, ...)		printk(KERN_INFO ku_fmt(fmt), ##__VA_ARGS__)
#define ku_cont(fmt, ...)		printk(KERN_CONT fmt, ##__VA_ARGS__)

#ifdef LOCAL
#define ku_debug(fmt, ...)		printk(KERN_DEBUG ku_fmt(fmt), ##__VA_ARGS__)
#else
#define ku_debug(fmt, ...)		(void) 0 
#endif

#include "ku_ipc.h"

/* ku_ipc_local.h */
/* ------------------------------------------------------------------------------------------ */

#define KU_MSGMNI			10
#define KU_MSGMAX			128

#define KU_DEV_NAME			"ku_ipc"
#define KU_DEV_PATH 			"/dev/"KU_DEV_NAME

#define KU_NO				1

#define KU_MINOR			0xC1
#define KU_BASIC			0x20
#define KU_OP				0x80

struct msg_buf {
	long type;
	char text[KU_MSGMAX];
};

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
	unsigned int msg_qnum;		/* current number of messages in queue */
	ku_pid_t msg_lspid;		/* last msgsnd pid */
	ku_pid_t msg_lrpid;		/* last msgrcv pid */
};

#define forn(i, n)			for (i = 0; i < (int) (n); i++)

/* ------------------------------------------------------------------------------------------ */

static dev_t 					dev;
static struct cdev 				*cdev;
static spinlock_t 				ku_spinlocks[KU_MSGMNI];
static struct ku_msq_ds 			msqs[KU_MSGMNI];
static struct list_head				msq_heads[KU_MSGMNI];
static wait_queue_head_t 			ku_wqs_snd[KU_MSGMNI];
static wait_queue_head_t 			ku_wqs_rcv[KU_MSGMNI];

static int is_valid_id(int id)			{ return 0 <= id && id < KU_MSGMNI; }
static int is_created_msq(int id)		{ return msqs[id].pid_cnt > 0; }
static int msq_update_pid(int id, int by)	{ return msqs[id].pid_cnt += by; }
static int msq_add_pid(int id) 			{ return msq_update_pid(id, 1); }
static int msq_sub_pid(int id) 			{ return msq_update_pid(id, -1); }
static int get_msq_list_free_bytes(int id) 	{ return msqs[id].msg_qbytes - msqs[id].msg_cbytes; }
static int get_msq_list_free_num(int id) 	{ return KUIPC_MAXMSG - msqs[id].msg_qnum; }
static int is_msq_full(int id, int msgsz)	{ return (get_msq_list_free_bytes(id) < msgsz || get_msq_list_free_num(id) == 0); }

static void ku_debug_list(int id)
{
#ifdef LOCAL
	int i;
	struct node *v;
	ku_debug("%s: msq%d: ", __func__, id);
	list_for_each_entry(v, &msq_heads[id], nodes) {
		if (v->nodes.prev != &msq_heads[id]) {
			ku_cont(" -> ");
		}
		forn(i, v->msgsz) {
			ku_cont("%c", v->msg.text[i]);
		}
		ku_cont("[%d](%ld)", v->msgsz, v->msg.type);
	}
#endif
}

static void ku_create_msq(int id)
{
	ku_debug("%s(%d)", __func__, id);
	msqs[id].pid_cnt 		= 0;
	msqs[id].msg_cbytes 		= 0;
	msqs[id].msg_qbytes 		= KUIPC_MAXVOL;
	msqs[id].msg_qnum 		= 0;
	msqs[id].msg_lspid		= 0;
	msqs[id].msg_lrpid		= 0;
}

static void ku_clear_msq_list(int id)
{
	struct node *v;
	struct list_head *ptr;
	struct list_head *tmp;
	ku_debug("%s(%d)", __func__, id);
	list_for_each_safe(ptr, tmp, &msq_heads[id]) {
		v = list_entry(ptr, struct node, nodes);
		list_del(ptr);
		kfree(v);
	}
}

static void ku_clear_msq(int id)
{
	ku_debug("%s(%d)", __func__, id);
	ku_clear_msq_list(id);
	msqs[id].pid_cnt 		= 0;
	msqs[id].msg_cbytes 		= 0;
	msqs[id].msg_qbytes 		= 0;
	msqs[id].msg_qnum 		= 0;
	msqs[id].msg_lspid		= 0;
	msqs[id].msg_lrpid		= 0;
}

static int ku_msq_get(int id, int msgflg)
{
	ku_debug("%s(%d)\n", __func__, id);
	if (msgflg == 0) {
		if (!is_created_msq(id)) {
			ku_err("%s: msq%d not created yet", __func__, id);
			return -1;
		}
		msq_add_pid(id);
		return id;
	}
	if (msgflg == KU_IPC_CREAT) {
		if (!is_created_msq(id)) {
			ku_create_msq(id);
		}
		msq_add_pid(id);
		return id;
	}
	if (msgflg == (KU_IPC_CREAT | KU_IPC_EXCL)) {
		if (is_created_msq(id)) {
			ku_err("%s: msq%d already created", __func__, id);
			return -1;
		}
		ku_create_msq(id);
		msq_add_pid(id);
		return id;
	}
	ku_err("%s: invalid message flags", __func__);
	return -1;
}

static int ku_msq_close(int id)
{
	ku_debug("%s(%d)\n", __func__, id);
	if (!is_created_msq(id)) {
		ku_err("%s: msq%d not created", __func__, id);
		return -1;
	}
	if (msq_sub_pid(id) == 0) {
		ku_clear_msq(id);
	}
	return 0;
}

static int ku_msq_push(int id, void *msgp, int msgsz)
{
	struct node *v = (struct node *) kmalloc(sizeof(struct node), GFP_KERNEL);
	ku_debug("%s(%d)\n", __func__, id);
	if (copy_from_user(&v->msg, msgp, sizeof(long) + msgsz)) {
		ku_err("%s failed", __func__);
		kfree(v);
		spin_unlock(&ku_spinlocks[id]);
		return -1;
	}
	v->msgsz = msgsz;
	list_add_tail(&v->nodes, &msq_heads[id]);
	ku_debug_list(id);
	// you should update msq_ds info
	msqs[id].msg_cbytes	+= msgsz;
	msqs[id].msg_qnum	+= 1;
	msqs[id].msg_lspid	= current->pid;
	wake_up(&ku_wqs_rcv[id]);
	spin_unlock(&ku_spinlocks[id]);
	return 0;
}

static int ku_msq_write(int id, void *msgp, int msgsz, int msgflg)
{		
	ku_debug("%s(%d)\n", __func__, id);
	if (msgflg & KU_IPC_NOWAIT) {
		spin_lock(&ku_spinlocks[id]);
		if (is_msq_full(id, msgsz)) {
			ku_err("%s: there's no space", __func__);
			spin_unlock(&ku_spinlocks[id]);
			return -1;
		}
		return ku_msq_push(id, msgp, msgsz);
	}
	wait_event(ku_wqs_snd[id], !is_msq_full(id, msgsz));
	spin_lock(&ku_spinlocks[id]);
	return ku_msq_push(id, msgp, msgsz);
}

static struct node *ku_msq_list_find(int id, long msgtyp)
{
	struct node *v;
	struct list_head *head = &msq_heads[id];
	ku_debug("%s(%d)\n", __func__, id);
	list_for_each_entry(v, head, nodes) {
		if (v->msg.type == msgtyp) {
			return v;
		}
	}
	return NULL;
}

static int ku_msq_pop(int id, void *msgp, int msgsz, int msgflg, struct node *v)
{
	int readsz = v->msgsz;
	ku_debug("%s(%d)\n", __func__, id);
	if (readsz > msgsz) {
		if ((msgflg & KU_MSG_NOERROR) == 0) {
			ku_err("%s kernel size too long", __func__);
			spin_unlock(&ku_spinlocks[id]);
			return -1;
		}
		readsz = msgsz;
	}
	if (copy_to_user(msgp, &v->msg, sizeof(long) + readsz)) {
		ku_err("%s failed", __func__);
		spin_unlock(&ku_spinlocks[id]);
		return -1;
	}
	list_del(&v->nodes);
	kfree(v);
	ku_debug_list(id);
	// you should update msq_ds info
	msqs[id].msg_cbytes	-= readsz;
	msqs[id].msg_qnum	-= 1;
	msqs[id].msg_lrpid	= current->pid;
	wake_up(&ku_wqs_snd[id]);
	spin_unlock(&ku_spinlocks[id]);
	return readsz;
}

static int ku_msq_read(int id, void *msgp, int msgsz, long msgtyp, int msgflg)
{
	struct node *v;
	ku_debug("%s(%d)\n", __func__, id);
	if (msgflg & KU_IPC_NOWAIT) {
		spin_lock(&ku_spinlocks[id]);
		if (!(v = ku_msq_list_find(id, msgtyp))) {
			spin_unlock(&ku_spinlocks[id]);
			ku_err("%s: there's no msgtyp\n", __func__);
			return -1;
		}
		return ku_msq_pop(id, msgp, msgsz, msgflg, v);
	}
	wait_event(ku_wqs_rcv[id], (v = list_last_entry(&msq_heads[id], struct node, nodes))->msg.type == msgtyp);
	spin_lock(&ku_spinlocks[id]);
	return ku_msq_pop(id, msgp, msgsz, msgflg, v);
}

static long ku_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	KU_READWRITE *data = (KU_READWRITE *) arg;
	int id = data->msqid;
	ku_debug("%s: process%i cmd(%u) with data:(%d, %p, %d, %ld, %d)\n",
		__func__,
		current->pid,
		cmd,
		data->msqid,
		data->msgp,
		data->msgsz,
		data->msgtyp,
		data->msgflg
	);
	if (!is_valid_id(id)) {
		ku_err("%s: %d is invalid msqid\n", __func__, id);
		return -1;
	}
	switch (cmd) {
		case KU_OPEN:
			spin_lock(&ku_spinlocks[id]);
			ret = ku_msq_get(id, data->msgflg);
			spin_unlock(&ku_spinlocks[id]);
			break;
		case KU_CLOSE:
			spin_lock(&ku_spinlocks[id]);
			ret = ku_msq_close(id);
			spin_unlock(&ku_spinlocks[id]);
			break;
		case KU_WRITE:
			ret = ku_msq_write(id, data->msgp, data->msgsz, data->msgflg);
			break;
		case KU_READ:
			ret = ku_msq_read(id, data->msgp, data->msgsz, data->msgtyp, data->msgflg);
			break;
		default:
			ret = -ENOIOCTLCMD;
			ku_err("%s: no ioctl cmd\n", __func__);
			break;
	}
	return ret;
}

static const struct file_operations ku_fops = {
	.unlocked_ioctl = ku_ioctl,
};

static int __init ku_init_cdev(void)
{
	int err;
	if ((err = alloc_chrdev_region(&dev, 0, KU_NO, KU_DEV_NAME)) < 0) {
		ku_err("%s: failed\n", __func__);
		return err;
	}
	cdev = cdev_alloc();
	if (IS_ERR(cdev)) {
		ku_err("%s: failed\n", __func__);
		err = PTR_ERR(cdev);
		goto unreg_cdev;
	}
	cdev_init(cdev, &ku_fops);
	if ((err = cdev_add(cdev, dev, KU_NO)) < 0) {
		ku_err("%s: failed\n", __func__);
		goto del_cdev;
	}
	ku_debug("%s()\n", __func__);
	return 0;
del_cdev:
	cdev_del(cdev);
unreg_cdev:
	unregister_chrdev_region(dev, KU_NO);
	return err;
}

static int __init ku_init_module(void)
{
	int id;
	int err = ku_init_cdev();
	ku_notice("%s()\n", __func__);
	if (err < 0) {
		return err;
	}
	forn(id, KU_MSGMNI) {
		INIT_LIST_HEAD(&msq_heads[id]);
		spin_lock_init(&ku_spinlocks[id]);
		init_waitqueue_head(&ku_wqs_snd[id]);
		init_waitqueue_head(&ku_wqs_rcv[id]);
	}
	return 0;
}

static void __exit ku_cleanup_module(void)
{
	int id;
	ku_notice("%s()\n", __func__);
	forn(id, KU_MSGMNI) {
		ku_clear_msq(id);
	}
	cdev_del(cdev);
	unregister_chrdev_region(dev, KU_NO);
}

module_init(ku_init_module);
module_exit(ku_cleanup_module);

MODULE_LICENSE("GPL");

/* read stuff as if there was a bug. --> "why is this wrong??"

 * basic strategy:
 	* understand what the problem asks for
	* simplify. a step by step approach
  	* readability is important

 * stuff you should look for
  	* special cases(zero size msg?)
  	* error return handling
	* memory leak
  	* invalid argument passing
	* invalid pointer operation
	* invalid lock/unlock
	* invalid blocking handling
*/
