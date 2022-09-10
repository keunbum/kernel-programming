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

static int my_id = 0;

int get_my_id(void) {
	return my_id;
}

static int get_len(int id) {
	int ret = (id == 0);
	while (id > 0) {
		id /= 10;
		++ret;
	}
	eprintk("id's len: %d\n", ret);
	return ret;
}

// returns: 1 -- success; 0 -- fail;
int set_my_id(int id) {
	if (id < 0 || get_len(id) != 9) {
		return 0;
	}
	my_id = id;
	return 1;
}

EXPORT_SYMBOL(get_my_id);
EXPORT_SYMBOL(set_my_id);

static int __init ch1_mod1_init(void) {
	eprintk("[ch1_mod1]: init\n");
	return 0;
}

static void __exit ch1_mod1_exit(void) {
	eprintk("[ch1_mod1]: exit\n");
}
	
module_init(ch1_mod1_init);
module_exit(ch1_mod1_exit);
