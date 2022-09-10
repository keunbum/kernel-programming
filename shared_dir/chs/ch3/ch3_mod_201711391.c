/*
 *	Source for creating a kernel module binary file(.ko).
 *	Set a unique file name that is different from existing modules.
 *	Let's use the 'make' command with a pre-written 'Makefile'.
 *	It's a kernel-specific command, so be more careful than usual.
*/
#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include <linux/uaccess.h>

#include "ch3.h"

#define CH3_DEV_NAME			"ch3_dev"
#define CH3_DEV_PATH 			"/dev/"CH3_DEV_NAME

#define CH3_NO				16

#define CH3_MSG_SIZE 128

typedef struct msg_st msg_st;
typedef struct msg_list node;

struct msg_list {
	struct list_head list;
	msg_st msg;
};


spinlock_t ch3_spinlock;

// header
static node msg_list_head;

static void ch3_debug_list(const struct list_head *head)
{
	node *ptr = NULL;

	printk(KERN_CONT "list: ");
	list_for_each_entry(ptr, head, list) {
		if (ptr->list.prev != head) {
			printk(KERN_CONT " -> ");
		}
		printk(KERN_CONT "%s(%d)", ptr->msg.str, ptr->msg.len);
	}
	printk(KERN_DEBUG "");
}

static int ch3_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	int ret = 0;

	spin_lock(&ch3_spinlock);
	if (minor >= CH3_NO) {
		ret = -ENXIO;
		goto out;
	}

	printk(KERN_NOTICE "ch3_dev%d: open()\n", minor);

out:
	spin_unlock(&ch3_spinlock);
	return 0;
}

static int ch3_release(struct inode *inode, struct file *file)
{
	/* maybe free operation? */
	unsigned int minor = iminor(inode);

	printk(KERN_NOTICE "ch3_dev%d: release()\n", minor);

	return 0;
}

static long ch3_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int minor = iminor(file_inode(file));
	msg_st *p_data = (msg_st *) arg;
	msg_st *msg = NULL;
	node *cur = NULL;

	spin_lock(&ch3_spinlock);
#ifdef CH3_LOCAL
	printk(KERN_DEBUG "ch3_dev%d: ioctl(), ", minor);
#endif
	if (minor >= CH3_NO) {
		ret = -ENODEV;
		goto out;
	}
	switch (cmd) {
		/* add(save) the received msg_st to the end of the msg_list */
		// if msg_st.len is greater than 128 -> assume is doesn't happen
		// out of kernel memory -> assume it doesn't happen
		case CH3_IOCTL_WRITE:
			cur = (node *) kmalloc(sizeof(node), GFP_KERNEL);
			if (copy_from_user(&cur->msg, p_data, sizeof(msg_st))) {
				ret = EFAULT;
				goto out;
			}
			list_add_tail(&cur->list, &msg_list_head.list);
			break;
		/* pass and delete msg_st at the beginning of msg_list */
		// if the size of the buffer pointed to by arg is less than sizeof(msg_st) -> assume it doesn't happen
		// if msg_list_head is empty -> pass msg_st of '0' + '\0' to the buffer pointed to by arg
		case CH3_IOCTL_READ:
			cur = list_first_entry_or_null(&msg_list_head.list, node, list);
			msg = cur ? &cur->msg : &msg_list_head.msg;
			if (copy_to_user(p_data, msg, sizeof(msg_st))) {
				ret = -EFAULT;
				goto out;
			}
			if (cur) {
				list_del(&cur->list);
				kfree(cur);
			}
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
#ifdef CH3_LOCAL
	ch3_debug_list(&msg_list_head.list);
#endif

out:
	spin_unlock(&ch3_spinlock);

	return ret;
}

static const struct file_operations ch3_fops = {
	.open = ch3_open,
	.release = ch3_release,
	.unlocked_ioctl = ch3_ioctl, /* trailing commas */
};

static dev_t dev = -1;
static struct cdev *cdev = NULL;

static int __init ch3_init_module(void)
{
	int err = 0;

	if ((err = alloc_chrdev_region(&dev, 0, CH3_NO, CH3_DEV_NAME)) < 0) {
		printk(KERN_ERR "ch3_dev: alloc_chrdev_region() failed\n");
		return err;
	}

	cdev = cdev_alloc();
	if (IS_ERR(cdev)) {
		printk(KERN_ERR "ch3_dev: cdev_alloc() failed\n");
		err = PTR_ERR(cdev);
		goto un_reg;
	}
	cdev_init(cdev, &ch3_fops);

	if ((err = cdev_add(cdev, dev, CH3_NO)) < 0) {
		printk(KERN_ERR "ch3_dev: cdev_add() failed\n");
		goto un_alloc;
	}

	INIT_LIST_HEAD(&msg_list_head.list);
	msg_list_head.msg.str[0] = '0';
	msg_list_head.msg.str[1] = '\0';
	msg_list_head.msg.len = 1;
	
	printk(KERN_INFO "ch3_dev: init()\n");
	return 0;

un_alloc:
	cdev_del(cdev);

un_reg:
	unregister_chrdev_region(dev, CH3_NO);

	return err;
}

static void __exit ch3_cleanup_module(void)
{
	node *cur = NULL;
	struct list_head *ptr = NULL, *tmp = NULL;

	// no exception handling is required
	list_for_each_safe(ptr, tmp, &msg_list_head.list) {
		cur = list_entry(ptr, node, list);
		list_del(ptr);
		kfree(cur);
	}
	cdev_del(cdev);
	unregister_chrdev_region(dev, CH3_NO);

	printk(KERN_INFO "ch3_dev: exit()\n");
}
	
module_init(ch3_init_module);
module_exit(ch3_cleanup_module);

MODULE_LICENSE("GPL"); /* to avoid kernel contamination */

/* you actually read the stuff from the bottom. */

/* stuff you should look for
 	* error return handling
	* invalid argument passing
	* null ptr exception
	* race condition
	* free dynamic allocation
 */
