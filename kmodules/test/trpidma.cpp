//////////////////////////////////////////////////////////////////////
// trpidma.cpp -- Test rpidma driver
// Date: Tue Apr 14 11:04:31 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "rpidma.h"

int
main(int argc,char **argv) {
    int fd = open(RPIDMA_DEVICE_PATH,O_RDONLY);
    s_rpidma_ioctl io;
    int rc;
	
    // Open /dev/rpidma driver
    if ( fd == -1 ) {
        fprintf(stderr,"%s: opening %s (driver loaded?)\n",
            strerror(errno),
            RPIDMA_DEVICE_PATH);
        return 2;
    }

    // Ask for a normal DMA channel:
    io.features = RPIDMA_FEAT_NORM;
    rc = ioctl(fd,RPIDMA_REQCHAN,&io);
    if ( rc ) {
        fprintf(stderr,"%s: rc=%d, ioctl(%d,RPIDMA_REQCHAN,)\n",
            strerror(errno),rc,fd);
        close(fd);
        exit(1);
    } else {
        printf("Got DMA chan %d, base %08X, IRQ %d\n",
            io.dma_chan,
            io.dma_base,
            io.dma_irq);
    }

    // Ask for Interrupt info:
    sleep(1);
    rc = ioctl(fd,RPIDMA_INTINFO,&io);
    assert(!rc);

    printf("%u Interrupts on IRQ %d\n",io.interrupts,io.dma_irq);

    // Release the DMA channel:
    rc = ioctl(fd,RPIDMA_RELCHAN,0);
    if ( rc ) {
        fprintf(stderr,"%s: rc=%d, ioctl(%d,RPIDMA_RELCHAN,0)\n",
            strerror(errno),rc,fd);
        close(fd);
        return 2;
    }

    // Close the driver
    printf("DMA channel released.\n");
    close(fd);

    return 0;
}

// End trpidma.cpp
