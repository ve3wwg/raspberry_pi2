/* Kernel Module rpidma4x.c for Linux 4.X
 * Sun Dec 13 13:26:26 2015
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

#include <linux/module.h>
#include <linux/platform_data/dma-bcm2708.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

#include "rpidma.h"

#define DEVICE_NAME "rpidma4x"

struct s_dmares {
    struct dma_chan *dma_chan;      /* Allocated DMA channel */
    struct dma_slave_config config; /* DMA config */
    struct scatterlist  *sg_list;   /* Scatter/Gather list */
    unsigned            n_sg;       /* # items in sg_list */
    struct dma_async_tx_descriptor *tx_desc; /* DMA tx descriptor */
    dma_cookie_t        cookie;     /* Cookie for submission */
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
    printk(KERN_INFO "Module rpidma4x loaded.\n");
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

    printk(KERN_INFO "Module rpidma4x unloaded.\n");
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

    res->dma_chan = 0;
    res->sg_list = 0;
    res->n_sg = 0;
    res->tx_desc = 0;
    res->cookie = 0;

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
        if ( res->dma_chan ) {
            dmaengine_terminate_all(res->dma_chan);
            dma_release_channel(res->dma_chan);
            res->dma_chan = 0;
	}
        if ( res->sg_list ) {
            kfree(res->sg_list);
            res->sg_list = 0;
            res->n_sg = 0;
        }
        kfree(res);
    }

    return 0;
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
    dma_cap_mask_t mask;
    enum dma_status dma_status;
    struct scatterlist *sglist, *sgent;
    uint32_t *usr_ptr = 0;
    int rc, x;

    switch ( cmd ) {
    case RPIDMA_START:
        if ( copy_from_user(&sarg,(char *)arg,sizeof sarg) )
            return -EFAULT;

        if ( res->dma_chan ) {
            /* Release existing channel */
            dmaengine_terminate_all(res->dma_chan);
            dma_release_channel(res->dma_chan);
            res->dma_chan = 0;
        }

        res->config.direction = DMA_DEV_TO_MEM;
        res->config.src_addr = sarg.src_addr;
        res->config.dst_addr = 0;
        res->config.src_addr_width = 4;
        res->config.dst_addr_width = 4;
        res->config.src_maxburst = 1;
        res->config.dst_maxburst = 1;
        res->config.device_fc = 0;
        res->config.slave_id = 0;           /* No DREQ */

	dma_cap_zero(mask);
	res->dma_chan = dma_request_channel(mask,0,0);

        if ( !res->dma_chan )
            return -EBUSY;

        rc = dmaengine_slave_config(res->dma_chan,&res->config);
        if ( rc < 0 )
            return -rc;

        /* Access list of user mode buffers */
        usr_ptr = kmalloc(sarg.n_dst * sizeof(uint32_t),GFP_KERNEL);
        if ( copy_from_user(usr_ptr,(char *)sarg.pdst_addr,sarg.n_dst * sizeof(uint32_t)) )
            return -EFAULT;

        /* Allocate a new scatter list */
        if ( res->sg_list )
            kfree(res->sg_list);

        res->n_sg = sarg.n_dst;
        res->sg_list = kmalloc(res->n_sg * sizeof(struct scatterlist),GFP_KERNEL);
        sg_init_table(res->sg_list,res->n_sg);

        sglist = res->sg_list;
        for_each_sg(sglist,sgent,res->n_sg,x) {
            // Cheat since we can't provide a proper input mapping
            sg_dma_address(sgent) = usr_ptr[x];
            sg_dma_len(sgent) = sarg.page_sz;
        }

        kfree(usr_ptr);
        usr_ptr = 0;

        res->tx_desc = dmaengine_prep_slave_sg(res->dma_chan,res->sg_list,res->n_sg,DMA_DEV_TO_MEM,0);
        res->cookie = dmaengine_submit(res->tx_desc);

        dma_async_issue_pending(res->dma_chan);
        return 0;

    case RPIDMA_STATUS:
        if ( !res->dma_chan )
            return -ENOENT;

        dma_status = dma_async_is_tx_complete(res->dma_chan,res->cookie,0,0);
        if ( dma_status == DMA_ERROR )
            return -EIO;
        
        if ( dma_status == DMA_COMPLETE )
            return 1;                   /* DMA has completed */
        return 0;                       /* DMA has not started / in progress */

    case RPIDMA_CANCEL:
        if ( res->dma_chan ) {
            /* Release existing channel */
            dmaengine_terminate_all(res->dma_chan);
            dma_release_channel(res->dma_chan);
            res->dma_chan = 0;
        }
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
