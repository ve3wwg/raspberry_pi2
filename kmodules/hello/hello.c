/* Kernel Module hello.c
 * Mon Apr 13 21:36:52 2015
 * Warren W. Gay VE3WWG
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init
hello_start(void) {
	printk(KERN_INFO "Module hello loaded..\n");
	printk(KERN_INFO "Hello world!!\n");
	return 0;
}

static void __exit
hello_end(void) {
	printk(KERN_INFO "Module hello unloaded.\n");
}

module_init(hello_start);
module_exit(hello_end);

MODULE_LICENSE("PUBLIC DOMAIN");

/*
 * End hello.c
 */

