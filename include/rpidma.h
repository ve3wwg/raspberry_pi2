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

#define RPIDMA_DEVICE_PATH "/dev/rpidma4x"

/* Linux 4.X version */

struct s_rpidma_ioctl {
    uint32_t    slave_id;   /* Slave ID to assign */
    uint32_t    page_sz;    /* Size of each page */
    uint32_t    src_addr;   /* One source address */
    uint32_t    n_dst;      /* # of destination addresses */
    uint32_t    *pdst_addr; /* Ptr to first designation address */

};

/*
 * ioctl(2) Commands
 */
#define RPIDMA_START    200 /* Allocate and start DMA */
#define RPIDMA_STATUS   201 /* Query completion status */
#define RPIDMA_CANCEL   202 /* Cancel DMA operation, if any */

#endif

/* End rpidma.h */
