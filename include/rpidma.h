/* rpidma.h - For use with ioctl of /dev/rpidma
 * Warren W. Gay VE3WWG
 * Thu Apr 16 21:48:48 2015
 *
 * Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
 * by Warren Gay VE3WWG
 * LGPL2 V2.1
 */

#ifndef _RPIDMA_H
#define _RPIDMA_H 1

#define RPIDMA_DEVICE_PATH "/dev/rpidma"

/*
 * DMA Feature Types
 */
#define RPIDMA_FEAT_FAST	1
#define RPIDMA_FEAT_BULK	2
#define RPIDMA_FEAT_NORM	4
#define RPIDMA_FEAT_LITE	8

/*
 * ioctl(2) Argument for /dev/rpidma:
 */
struct s_rpidma_ioctl {
    int         features;   /* Requested features */
    int         dma_chan;   /* Assigned DMA Channel */
    unsigned    dma_base;   /* Peripheral base addr */
    int         dma_irq;    /* IRQ # assigned */
    unsigned    interrupts; /* # of interrupts */
};

/*
 * ioctl(2) Commands
 */
#define RPIDMA_REQCHAN  100 /* Request (reserve) a DMA chan */
#define RPIDMA_RELCHAN  101 /* Release a reserved DMA chan */
#define RPIDMA_INTINFO	102 /* Request snapshot of interrupts */

#endif

/* End rpidma.h */
