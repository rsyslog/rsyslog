/*
 * Loadable driver which provides the ability to generate a kernel
 * protection fault.  Mainly useful for testing the address translation
 * capabilities of klogd.
 *
 * Fri Oct 27 14:34:27 CDT 1995:  Dr. Wettstein
 *
 *	Initial version.
 */

#define NEW_MODULES

/* Kernel includes. */
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/fs.h>

/* Standard module stuff. */
#if defined(NEW_MODULES)
#include <linux/module.h>
#else
#include <linux/module.h>
#include <linux/version.h>
char kernel_version[] = UTS_RELEASE;
#endif


static int major = 32;


#ifdef MODULE
static int oops_ioctl(struct inode *, struct file *, unsigned int cmd, unsigned long arg);
static int oops_open(struct inode * node, struct file * file);
static void oops(void);

static struct symbol_table these_symbols = {
#include <linux/symtab_begin.h>
	X(oops_open),
	X(oops_ioctl),
	X(oops),
#include <linux/symtab_end.h>
};

/* driver specific module definitions */
static struct file_operations oops_fops1 = {
	NULL,		/* hw_lseek */
	NULL,		/* hw_read */
	NULL,		/* write */
	NULL,		/* hw_readdir */
	NULL,		/* hw_select */
	oops_ioctl,	/* hw_ioctl */
	NULL,		/* mmap */
	oops_open,	/* hw_open */
	NULL,		/* hw_release */
	NULL		/* fsync */
};

static int oops_open(struct inode * node, struct file * file)
{
	printk("Called oops_open.\n");
	return(0);
}


static int oops_ioctl(struct inode * node, struct file * file, \
		      unsigned int cmd, unsigned long arg)
{
	
	printk("Called oops_ioctl.\n");
	printk("Cmd: %d, Arg: %ld\n", cmd, arg);
	if ( cmd == 1 )
	{
		oops();
	}
		
	return(0);
}

static void oops()

{
	auto unsigned long *p = (unsigned long *) 828282828;
	*p = 5;
	return;
}

	
int
init_module(void)
{
	printk("oops: Module initilization.\n");
	if (register_chrdev(major, "oops", &oops_fops1)) {
		printk("register_chrdev failed.");
		return -EIO;
	}

	printk("oops: Registering symbols.\n");
  	register_symtab(&these_symbols);
	
	return 0;
}


void
cleanup_module(void)
{
	/* driver specific cleanups, ususally "unregister_*()" */
	printk("oops: Module unloadeding.\n");
	if (unregister_chrdev(major, "oops") != 0)
		printk("cleanup_module failed\n");
	else
		printk("cleanup_module succeeded\n");

	return;

}
#endif /* MODULE */
