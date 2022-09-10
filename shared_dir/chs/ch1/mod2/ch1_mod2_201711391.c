/*
 *	Source for creating a kernel module binary file(.ko).
 *	Set a unique file name that is different from existing modules.
 *	Let's use the 'make' command with a pre-written 'Makefile'.
 *	It's a kernel-specific command,
 *	so let's be more careful than usual.
*/
#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("DUAL BSD/GPL"); /* simply to avoid warnings */

#ifdef LOCAL
#define eprintk(...) printk(KERN_DEBUG "[LOCAL DEBUG]: " __VA_ARGS__)
#else 
#define eprintk(...) (void) 0
#endif

extern int get_my_id(void);
extern int set_my_id(int);

static int __init ch1_mod2_init(void) {
	eprintk("[ch1_mod2]: init\n");
	printk(KERN_NOTICE "My id : %d\n", get_my_id());
#ifndef LOCAL
	set_my_id(201711391);
#else
	eprintk("set_my_id() %s\n", set_my_id(201711391) ? "succeeded" : "failed");
#endif
	printk(KERN_NOTICE "My id : %d\n", get_my_id());
	return 0;
}

static void __exit ch1_mod2_exit(void) {
	eprintk("[ch1_mod2]: exit\n");
}
	
module_init(ch1_mod2_init);
module_exit(ch1_mod2_exit);
