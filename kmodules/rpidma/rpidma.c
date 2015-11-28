/* Kernel Module rpidma.c
 * Mon Apr 13 21:44:28 2015
 * Warren W. Gay VE3WWG
 *
 * Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
 * by Warren Gay VE3WWG
 * LGPL2 V2.1
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <mach/dma.h>

#include "rpidma.h"

#define DMA_CS          0x00
#define DMA_CONBLK_AD   0x04
#define DMA_TI          0x08
#define DMA_SOURCE_AD   0x0c
#define DMA_DEST_AD     0x10
#define DMA_TXFR_LEN    0x14
#define DMA_STRIDE      0x18
#define DMA_NEXTCONBK   0x1C
#define DMA_DEBUG       0x20

#define DEVICE_NAME "rpidma"

struct s_dmares {
    int         rpi_features;   /* For user */
    int         dma_chan;	/* Allocated channel */
    void __iomem *dma_base;     /* Peripheral base */
    int		dma_irq;	/* IRQ */
    unsigned    interrupts;     /* Interrupt count */
};

static struct s_rpidma {
    struct cdev cdev;
    char        name[8];	/* Driver name */
} rpidma_dev;

static int rpidma_open(struct inode *,struct file *);
static int rpidma_release(struct inode *,struct file *);
static long rpidma_ioctl(struct file *,unsigned cmd,unsigned long arg);

static const struct file_operations rpidma_fops = {
    .owner = THIS_MODULE,
    .open = rpidma_open,
    .release = rpidma_release,
    .unlocked_ioctl = rpidma_ioctl,
};

static struct class *rpidma_class;
static dev_t rpidma_dev_no;

/*
 * Translate RPIDMA_FEAT_* into driver bit values:
 */
static unsigned
bcm_dma_features(unsigned features) {
    unsigned bcm_features = 0;
    
    if ( features & RPIDMA_FEAT_FAST )
        bcm_features |= BCM_DMA_FEATURE_FAST;
    if ( features & RPIDMA_FEAT_BULK )
        bcm_features |= BCM_DMA_FEATURE_BULK;
    if ( features & RPIDMA_FEAT_NORM )
        bcm_features |= BCM_DMA_FEATURE_NORMAL;
    if ( features & RPIDMA_FEAT_LITE )
        bcm_features |= BCM_DMA_FEATURE_LITE;
    if ( !bcm_features )
        bcm_features |= BCM_DMA_FEATURE_NORMAL;
    return bcm_features;
}

/*
 * Module startup:
 */
static int __init
rpidma_start(void) {
    int rc;

    rc = alloc_chrdev_region(&rpidma_dev_no,0,1,DEVICE_NAME);
    if ( rc < 0 ) {
        printk(KERN_DEBUG "Can't register device %s\n",
            DEVICE_NAME);
        return -1;
    }

    rpidma_class = class_create(THIS_MODULE,DEVICE_NAME);
    strcpy(rpidma_dev.name,DEVICE_NAME);

    cdev_init(&rpidma_dev.cdev,&rpidma_fops);
    rpidma_dev.cdev.owner = THIS_MODULE;
    rc = cdev_add(&rpidma_dev.cdev,rpidma_dev_no,1);
    if ( rc ) {
        printk(KERN_DEBUG "Bad cdev (dev_no = %u, rc=%d)\n",
            (unsigned)rpidma_dev_no,rc);
        return rc;
    }

    device_create(rpidma_class,NULL,rpidma_dev_no,&rpidma_dev.cdev,"%s",rpidma_dev.name);
    printk(KERN_INFO "Module rpidma loaded.\n");
    return 0;
}

/*
 * Module unload:
 */
static void __exit
rpidma_end(void) {

    unregister_chrdev_region(rpidma_dev_no,1);
    device_destroy(rpidma_class,MKDEV(MAJOR(rpidma_dev_no),0));
    cdev_del(&rpidma_dev.cdev);
    class_destroy(rpidma_class);

    printk(KERN_INFO "Module rpidma unloaded.\n");
}

/*
 * Driver open:
 */
static int
rpidma_open(struct inode *inode,struct file *file) {
    struct s_rpidma *devp;
    struct s_dmares *res = kmalloc(sizeof *res,GFP_KERNEL);

    if ( !res )
        return -ENOMEM;

    res->rpi_features = 0;
    res->dma_chan = -1;
    res->dma_base = 0;
    res->dma_irq = -1;
    res->interrupts = 0;

    devp = container_of(inode->i_cdev,struct s_rpidma,cdev);
    file->private_data = res;
    return 0;
}

/*
 * Driver close:
 */
static int
rpidma_release(struct inode *inode,struct file *file) {
    struct s_dmares *res = (struct s_dmares *)file->private_data;

    if ( res ) {
	if ( res->dma_chan >= 0 ) {
            writel(1<<31,res->dma_base+DMA_CS);	/* RESET = 1 */
	    free_irq(res->dma_irq,res);
	    bcm_dma_chan_free(res->dma_chan);
	}
        kfree(res);
    }

    return 0;
}

/*
 * Interrupt handler
 */
static irqreturn_t
dma_int_handler(int irq,void *vdata) {
    struct s_dmares *res = (struct s_dmares *)vdata;

    writel(0x04,res->dma_base+DMA_CS);	/* Reset INT */
    writel(0x01,res->dma_base+DMA_CS);	/* ACTIVE = 1 */
    ++res->interrupts;

    return IRQ_HANDLED;    
}

/*
 * ioctl(2) Commands:
 */
static long
rpidma_ioctl(
  struct file *file,
  unsigned cmd,
  unsigned long arg) {
    struct s_dmares *res = (struct s_dmares *)file->private_data;
    struct s_rpidma_ioctl sarg;
    int rc;

    switch ( cmd ) {
    /*
     * Reserve a DMA controller: arg is &struct s_rpidma_ioctl
     */
    case RPIDMA_REQCHAN:
        if ( copy_from_user(&sarg,(char *)arg,sizeof sarg) )
            return -EFAULT;

        if ( res->dma_chan >= 0 ) {
            /* Release existing channel */
            bcm_dma_chan_free(res->dma_chan);
            res->dma_chan = -1;
        }

        res->rpi_features = sarg.features;

	// E.g. DMA chan 2, base 0xF3007200

        res->dma_chan = bcm_dma_chan_alloc(
            bcm_dma_features(sarg.features),
            &res->dma_base,
            &res->dma_irq);

        if ( res->dma_chan < 0 )
            return res->dma_chan;

	res->interrupts = 0;

        sarg.dma_chan = res->dma_chan;
        sarg.dma_base = (unsigned) res->dma_base;
        sarg.dma_irq = res->dma_irq;

        if ( copy_to_user((char *)arg,&sarg,sizeof sarg) )
            return -EFAULT;

	rc = request_irq(
            res->dma_irq,
            dma_int_handler,
            0, /* SA_INTERRUPT */
            DEVICE_NAME,
            res);

        if ( rc != 0 )
            printk(KERN_DEBUG "Unable to request_irq(%d)\n",res->dma_irq);

        return 0;

    /*
     * Return updated info: (interrupt count mainly)
     */
    case RPIDMA_INTINFO:
        sarg.features = res->rpi_features;
        sarg.dma_chan = res->dma_chan;
        sarg.dma_base = (unsigned) res->dma_base;
        sarg.dma_irq = res->dma_irq;
        sarg.interrupts = res->interrupts;

        if ( copy_to_user((char *)arg,&sarg,sizeof sarg) )
            return -EFAULT;

        return 0;

    /*
     * Release reserved DMA controller (no arg)
     */
    case RPIDMA_RELCHAN:
        if ( res->dma_chan < 0 ) 
            return -ENOENT;     /* No DMA to release */

        free_irq(res->dma_irq,res);
        bcm_dma_chan_free(res->dma_chan);
        res->dma_chan = -1;
        return 0;
    
    default :
        ;
    };
    
    return -EINVAL;
}

module_init(rpidma_start);
module_exit(rpidma_end);

MODULE_LICENSE("GPL");

/* End rpidma.c */
