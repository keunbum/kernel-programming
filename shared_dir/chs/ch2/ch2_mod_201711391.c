/*
 *	Source for creating a kernel module binary file(.ko).
 *	Set a unique file name that is different from existing modules.
 *	Let's use the 'make' command with a pre-written 'Makefile'.
 *	It's a kernel-specific command, so let's be more careful than usual.
*/
#include <linux/init.h>
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>


#define CH2_DEV_NAME		"ch2"

#define CH2_NO			1

/* it's not registered in the ioctl-number doc and matches project name. */
#define CH2_MINOR		0xC2

#define CH2_BASIC		0x10
#define CH2_OP			0x40

typedef struct _CH2_READWRITE {
	long retval;
	long val;
//	const char *exp;
} CH2_READWRITE;

#define CH2_GET			_IO(CH2_MINOR, CH2_BASIC+1)
#define CH2_SET			_IOW(CH2_MINOR,CH2_BASIC+2,CH2_READWRITE)

#define CH2_ADD			_IOWR(CH2_MINOR,CH2_OP+1,CH2_READWRITE)
#define CH2_MUL			_IOWR(CH2_MINOR,CH2_OP+3,CH2_READWRITE)


#define CH2_FUNC(func,x,...)					\
  do {								\
    if ((x = func(__VA_ARGS__)) < 0) {				\
      perror("["CH2_DEV_NAME"_mod]: " #func"() failed");	\
      return 1;							\
    }								\
  } while (0)

#define CH2_OPEN(x,...)		CH2_FUNC(open,x,__VA_ARGS__)
#define CH2_IOCTL(x,...)	CH2_FUNC(ioctl,x,__VA_ARGS__)

#define CH2_PRINTK(flag,...)	printk(flag "["CH2_DEV_NAME"_mod]: " __VA_ARGS__)

#define CH2_DEV_PATH 		"/dev/"CH2_DEV_NAME



static long result = 0;

/* maybe someday I will have to implement it myself instead of mknod..? */
static int ch2_open(struct inode *inode, struct file *file)
{
	CH2_PRINTK(KERN_NOTICE, "open()\n");
	return 0;
}

static int ch2_release(struct inode *inode, struct file *file)
{
	CH2_PRINTK(KERN_NOTICE, "release()\n");
	return 0;
}

static long ch2_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
	CH2_READWRITE *p_data = (CH2_READWRITE *) arg;

	switch (cmd) {

		case CH2_GET:
			return result;

		case CH2_SET:
			CH2_PRINTK(KERN_NOTICE, "ch2_ioctl: from %ld to %ld\n", result, p_data->val);
			result = p_data->val;
			break;

		case CH2_ADD:
			CH2_PRINTK(KERN_NOTICE, "ch2_ioctl: %ld + %ld = %ld\n", result, p_data->val, result + p_data->val);
			result += p_data->val;
			p_data->retval = result;
			break;

		case CH2_MUL:
			CH2_PRINTK(KERN_NOTICE, "ch2_ioctl: %ld * %ld = %ld\n", result, p_data->val, result * p_data->val);
			result *= p_data->val;
			p_data->retval = result;
			break;

		default:
			return -ENOIOCTLCMD;

	}
	return 0;
}

static const struct file_operations ch2_fops = {
	.open = ch2_open,
	.release = ch2_release,
	.unlocked_ioctl = ch2_ioctl, /* trailing commas */
};

static dev_t dev = -1;
static struct cdev *cdev = NULL;

// returns 0 -- succeed; negative -- failed;
static int __init ch2_init_module(void)
{
	int err = 0;

	if (alloc_chrdev_region(&dev, 0, CH2_NO, CH2_DEV_NAME)) {
		CH2_PRINTK(KERN_ERR, "alloc_chrdev_region() failed\n");
		return -EINVAL;
	}

	cdev = cdev_alloc();
	if (!cdev) {
		CH2_PRINTK(KERN_ERR, "cdev_alloc() failed\n");
		err = -ENOMEM; /* not sure exactly what error value to return */
		goto un_reg;
	}
	
	cdev_init(cdev, &ch2_fops);

	if ((err = cdev_add(cdev, dev, CH2_NO)) < 0) {
		CH2_PRINTK(KERN_ERR, "cdev_add() failed\n");
		goto un_alloc;
	}

	CH2_PRINTK(KERN_INFO, "init()\n");

	return 0;

un_alloc:
	cdev_del(cdev);

un_reg:
	unregister_chrdev_region(dev, CH2_NO);

	return err;
}

static void __exit ch2_cleanup_module(void)
{
	CH2_PRINTK(KERN_INFO, "exit()\n");

	// no exception handling is required
	cdev_del(cdev);
	unregister_chrdev_region(dev, CH2_NO);
}
	
module_init(ch2_init_module);
module_exit(ch2_cleanup_module);

MODULE_LICENSE("GPL"); /* to avoid kernel contamination */

/* you actually read the stuff from the bottom. */

/* stuff you should look for
 	* error return handling
	* invalid argument passing
	* null ptr exception
 */
