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
#include <linux/gpio.h>
#include <linux/interrupt.h>

#include "ku.h"

#define ku_fmt(fmt)			"ku: "fmt

#define ku_err(fmt, ...)		printk(KERN_ERR ku_fmt(fmt), ##__VA_ARGS__)
#define ku_notice(fmt, ...)		printk(KERN_NOTICE ku_fmt(fmt), ##__VA_ARGS__)
#define ku_info(fmt, ...)		printk(KERN_INFO ku_fmt(fmt), ##__VA_ARGS__)
#define ku_cont(fmt, ...)		printk(KERN_CONT fmt, ##__VA_ARGS__)

#ifdef LOCAL
#define ku_debug(fmt, ...)		printk(KERN_DEBUG ku_fmt(fmt), ##__VA_ARGS__)
#else
#define ku_debug(fmt, ...)		(void) 0 
#endif

static spinlock_t 				ku_spinlock;
static struct ku_msq_ds 			msq;
static struct list_head				msq_head;
static wait_queue_head_t 			ku_wq_rcv;

static int get_msq_list_free_bytes(void) 	{ return msq.msg_qbytes - msq.msg_cbytes; }
static int get_msq_list_free_num(void) 		{ return KU_MAXMSG - msq.msg_qnum; }
static int is_msq_full(int msgsz)		{ return (get_msq_list_free_bytes() < msgsz || get_msq_list_free_num() == 0); }

/* sensor ------------------------------------------------------------------------------------------------------ */

#define KU_ULTRA_TRIG 				17
#define KU_ULTRA_ECHO 				18

#define KU_ULTRA_NONE				0
#define KU_ULTRA_FIRST				1
#define KU_ULTRA_SECOND				2
#define KU_ULTRA_YET				3

#define KU_ULTRA_C				58


static int ku_echo_irq;
static int ku_echo_flag = KU_ULTRA_YET;

static ktime_t echo_start;
static ktime_t echo_stop;

/* actuator  ------------------------------------------------------------------------------------------------------ */

#define KU_NOTE_ITER				100
#define KU_SPEAKER				12
#define KU_NOTES				7

/* timer  ------------------------------------------------------------------------------------------------------ */

#define KU_TERM					msecs_to_jiffies(100)

#define secs					(jiffies_to_msecs(jiffies) / 1000)

struct timer_node {
	struct timer_list timer;
};

static struct timer_node ku_ultra_timer;

/* ------------------------------------------------------------------------------------------------------------ */

static void ku_debug_list(void)
{
#ifdef LOCAL
	int i;
	struct node *v;
	ku_debug("%s: ", __func__);
	list_for_each_entry(v, &msq_head, nodes) {
		if (v->nodes.prev != &msq_head) {
			ku_cont(" -> ");
		}
		ku_cont("[%u](", v->msg.distance);
		forn(i, v->msgsz) {
			ku_cont("%c", v->msg.timestamp[i]);
		}
		ku_cont(")");
	}
#endif
}

static void ku_create_msq(void)
{
	ku_debug("%s()", __func__);
	msq.pid_cnt 		= 0;
	msq.msg_cbytes 		= 0;
	msq.msg_qbytes 		= KU_MAXVOL;
	msq.msg_qnum 		= 0;
	msq.msg_lspid		= 0;
	msq.msg_lrpid		= 0;
}

static void ku_clear_msq_list(void)
{
	struct node *v;
	struct list_head *ptr;
	struct list_head *tmp;
	ku_debug("%s()", __func__);
	list_for_each_safe(ptr, tmp, &msq_head) {
		v = list_entry(ptr, struct node, nodes);
		list_del(ptr);
		kfree(v);
	}
}

static void ku_clear_msq(void)
{
	ku_debug("%s()", __func__);
	ku_clear_msq_list();
	msq.pid_cnt 		= 0;
	msq.msg_cbytes 		= 0;
	msq.msg_qbytes 		= 0;
	msq.msg_qnum 		= 0;
	msq.msg_lspid		= 0;
	msq.msg_lrpid		= 0;
}

static int ku_msq_get(int msgflg)
{
	ku_debug("%s(%d)\n", __func__, msgflg);
	return 0;
}

static int ku_msq_close(void)
{
	ku_debug("%s()\n", __func__);
	return 0;
}

static int ku_msq_push(int cm, s64 time)
{
	struct node *v = (struct node *) kmalloc(sizeof(struct node), GFP_KERNEL);
	v->msg.distance = cm;
	sprintf(v->msg.timestamp, "%lld", time);
	v->msgsz = (int) strlen(v->msg.timestamp);
	ku_debug("%s: (%d, %s[%d])\n", __func__, v->msg.distance, v->msg.timestamp, v->msgsz);
	if (is_msq_full(v->msgsz)) {
		kfree(v);
		ku_debug("%s: no space to push\n", __func__);
		return -1;
	}
	list_add_tail(&v->nodes, &msq_head);
	ku_debug_list();
	// you should update msq_ds info
	msq.msg_cbytes	+= v->msgsz;
	msq.msg_qnum	+= 1;
	msq.msg_lspid	= current->pid;
	wake_up(&ku_wq_rcv);
	//spin_unlock(&ku_spinlock);
	return 0;
}

static int ku_msq_write(int cm, s64 time)
{		
	ku_debug("%s()\n", __func__);
	//spin_lock(&ku_spinlock);
	return ku_msq_push(cm, time);
}

/*
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
*/

static int ku_msq_pop(struct node *v, void *msgp, int msgsz, int msgflg)
{
	int readsz = v->msgsz;
	ku_debug("%s()\n", __func__);
	spin_lock(&ku_spinlock);
	if (readsz > msgsz) {
		if ((msgflg & KU_MSG_NOERROR) == 0) {
			ku_debug("%s kernel size too long", __func__);
			spin_unlock(&ku_spinlock);
			return -1;
		}
		readsz = msgsz;
	}
	if (copy_to_user(msgp, &v->msg, sizeof(unsigned int) + readsz)) {
		ku_err("%s failed", __func__);
		spin_unlock(&ku_spinlock);
		return -1;
	}
	list_del(&v->nodes);
	kfree(v);
	ku_debug_list();
	// you should update msq_ds info
	msq.msg_cbytes	-= readsz;
	msq.msg_qnum	-= 1;
	msq.msg_lrpid	= current->pid;
	spin_unlock(&ku_spinlock);
	return readsz;
}

static int ku_msq_read(void *msgp, int msgsz, int msgflg)
{
	struct node *v;
	ku_debug("%s()\n", __func__);
	if ((v = list_first_entry_or_null(&msq_head, struct node, nodes))) {
		return ku_msq_pop(v, msgp, msgsz, msgflg);
	}
	if (msgflg & KU_NOWAIT) {
		ku_debug("%s: there's no msg in queue\n", __func__);
		return -1;
	}
	wait_event(ku_wq_rcv, (v = list_first_entry_or_null(&msq_head, struct node, nodes)));
	return ku_msq_pop(v, msgp, msgsz, msgflg);
}

static int ku_d2n(int distance)
{
	return (distance + 90) * 11;
}

static int ku_play(int distance)
{
	int i;
	int note = ku_d2n(distance);
	ku_debug("%s()\n", __func__);
	gpio_set_value(KU_SPEAKER, 0);
	forn(i, KU_NOTE_ITER) {
		gpio_set_value(KU_SPEAKER, 1);
		udelay(note);
		gpio_set_value(KU_SPEAKER, 0);
		udelay(note);
	}
	return 0;
}

static long ku_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	KU_READWRITE *data = (KU_READWRITE *) arg;
	ku_debug("%s: process%i cmd(%u) with data:(%p, %d, %d)\n",
		__func__,
		current->pid,
		cmd,
		data->msgp,
		data->msgsz,
		data->msgflg
	);
	switch (cmd) {
		case KU_OPEN:
			ret = ku_msq_get(data->msgflg);
			break;
		case KU_CLOSE:
			ret = ku_msq_close();
			break;
		case KU_READ:
			ret = ku_msq_read(data->msgp, data->msgsz, data->msgflg);
			break;
		case KU_WRITE:
			ret = ku_play(data->msgp->distance);
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

/* sensor ------------------------------------------------------------------------------------------------------ */

static irqreturn_t ku_ultra_isr(int irq, void *dev_id) {
	int cm;
	s64 time;
	ktime_t cur_time;
	unsigned long flags;
	spin_lock_irqsave(&ku_spinlock, flags);
	cur_time = ktime_get();
	ku_debug("%s(%d) with echo_flag: %d\n", __func__, irq, ku_echo_flag);
	if (ku_echo_flag == KU_ULTRA_FIRST) {
		if (gpio_get_value(KU_ULTRA_ECHO) == 1) {
			echo_start = cur_time;
			ku_echo_flag = KU_ULTRA_SECOND;
		}
	} else
	if (ku_echo_flag == KU_ULTRA_SECOND) {
		if (gpio_get_value(KU_ULTRA_ECHO) == 0) {
			echo_stop = cur_time;
			time = ktime_to_us(ktime_sub(echo_stop, echo_start));
			cm = (int) time / KU_ULTRA_C;
			ku_echo_flag = KU_ULTRA_YET;
			ku_debug("%s: detect %dcm\n", __func__, cm);
			ku_msq_write(cm, echo_stop);
			ku_echo_flag = KU_ULTRA_NONE;
		}
	}
	spin_unlock_irqrestore(&ku_spinlock, flags);
	return IRQ_HANDLED;
}

static void ku_trigger_ultra(void) {
	ku_debug("%s()\n", __func__);
	echo_start = ktime_set(0, 1);
	echo_stop = ktime_set(0, 1);
	ku_echo_flag = KU_ULTRA_NONE;
	gpio_set_value(KU_ULTRA_TRIG, 1);
	udelay(10);
	gpio_set_value(KU_ULTRA_TRIG, 0);
	ku_echo_flag = KU_ULTRA_FIRST;
}

static int __init ku_init_ultra(void) {
	int err;
	ku_debug("%s()\n", __func__);
	gpio_request_one(KU_ULTRA_TRIG, GPIOF_OUT_INIT_LOW, "KU_ULTRA_TRIG");
	gpio_request_one(KU_ULTRA_ECHO, GPIOF_IN, "KU_ULTRA_ECHO");
	ku_echo_irq = gpio_to_irq(KU_ULTRA_ECHO);
	err = request_irq(ku_echo_irq, ku_ultra_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "KU_ULTRA_ECHO", NULL);
	if (err < 0) {
		ku_err("%s: unable to request IRQ: %d with ERR: %d\n", __func__, ku_echo_irq, err);
		free_irq(ku_echo_irq, NULL);
		return err;
	}
	return 0;
}

/* actuator  ------------------------------------------------------------------------------------------------------ */

static int __init ku_init_speaker(void)
{
	ku_debug("%s()\n", __func__);
	gpio_request_one(KU_SPEAKER, GPIOF_OUT_INIT_LOW, "KU_SPEAKER");
	gpio_set_value(KU_SPEAKER, 0);
	return 0;
}

/* timer ------------------------------------------------------------------------------------------------------ */

static void ku_timer_handler(struct timer_list *t)
{
	unsigned long flags;
	spin_lock_irqsave(&ku_spinlock, flags);
	{
		ku_debug("%s: expired, %u secs\n", __func__, secs);
		ku_trigger_ultra();
		mod_timer(&ku_ultra_timer.timer, jiffies + KU_TERM);
	}
	spin_unlock_irqrestore(&ku_spinlock, flags);
}

static void __init ku_init_timer(void)
{
	ku_debug("%s()\n", __func__);
	timer_setup(&ku_ultra_timer.timer, ku_timer_handler, 0);
	ku_ultra_timer.timer.expires = jiffies + KU_TERM;
	add_timer(&ku_ultra_timer.timer);
}

static dev_t 					dev;
static struct cdev 				*cdev;

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
	int err;
	ku_notice("%s()\n", __func__);
	if ((err = ku_init_cdev()) < 0) {
		return err;
	}
	spin_lock_init(&ku_spinlock);
	INIT_LIST_HEAD(&msq_head);
	init_waitqueue_head(&ku_wq_rcv);
	ku_create_msq();
	if ((err = ku_init_ultra()) < 0) {
		return err;
	}
	if ((err = ku_init_speaker()) < 0) {
		return err;
	}
	ku_init_timer();
	return 0;
}

/* reverse order of init */
static void __exit ku_cleanup_module(void)
{
	ku_notice("%s()\n", __func__);

	del_timer(&ku_ultra_timer.timer);

	gpio_set_value(KU_SPEAKER, 0);
	gpio_free(KU_SPEAKER);

	free_irq(ku_echo_irq, NULL);

	ku_clear_msq();

	cdev_del(cdev);
	unregister_chrdev_region(dev, KU_NO);
}

module_init(ku_init_module);
module_exit(ku_cleanup_module);

MODULE_LICENSE("GPL");

/* read stuff as if you were wrong once. --> "why is this wrong??"

 * basic strategy:
 	* understand what the problem asks for
	* simplify. a step by step approach
  	* readability is important
	* keep calm
	* do smth instead of nothing and stay organized
	* DON'T GET STUCK ON ONE APPROACH

 * stuff you should look for
  	* special cases
  	* error return handling
	* memory leak
  	* invalid argument passing
	* invalid pointer operation
	* invalid lock/unlock
*/
