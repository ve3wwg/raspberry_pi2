//////////////////////////////////////////////////////////////////////
// logana.cpp -- Simple Logic Analayzer Class
// Date: Tue Apr 21 22:25:02 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <assert.h>

#include "dmamem.hpp"
#include "gpio.hpp"
#include "piutils.hpp"
#include "rpidma.h"
#include "logana.hpp"

#include <sstream>

LogicAnalyzer::LogicAnalyzer(unsigned arg_ppblk) : pagespblk(arg_ppblk) {
    sampspblk = 0;                  // Until page size is known
    pagesize = 0;
    fd = -1;
    dalloc.features = RPIDMA_FEAT_NORM;
    dalloc.dma_chan = -1;
    dalloc.dma_base = 0;
    dalloc.dma_irq = 0;
}

LogicAnalyzer::~LogicAnalyzer() {
    close();
}

bool
LogicAnalyzer::open() {
    std::stringstream ss;

    if ( gpio.get_error() != 0 )
        return false;               // Need peripheral base

    if ( fd >= 0 )
        close();

    fd = ::open("/dev/rpidma",O_RDONLY);
    if ( fd < 0 ) {
        std::stringstream ss;

        ss << strerror(errno) << ": Opening driver /dev/rpidma";
        errmsg = ss.str();
        return false;
    }

    // Open mailbox interface:

    if ( !dmamem.create(LOGANA_PATH,1) ) {
        ss  << strerror(errno) << ": opening mailbox "
            << LOGANA_PATH;
        errmsg = ss.str();
        return false;
    }

    pagesize = dmamem.get_page_size();
    sampspblk = ( ( pagesize * pagespblk ) / sizeof(uint32_t) ) - 8;

    return true;
}

void
LogicAnalyzer::close() {

    if ( dalloc.dma_chan >= 0 )
        this->abort(0);

    if ( fd >= 0 ) {
        ::close(fd);
        fd = -1;
    }

    dmamem.close();
}

bool
LogicAnalyzer::alloc_dma() {
    int rc;

    if ( dalloc.dma_chan >= 0 )
        free_dma();

    rc = ioctl(fd,RPIDMA_REQCHAN,&dalloc);
    if ( rc != 0 ) {
        std::stringstream ss;
        
        ss  << strerror(errno) << ": ioctl(" << fd
            << ",RPIDMA_REQCHAN)";
        errmsg = ss.str();
        return false;
    }

    return true;
}

bool
LogicAnalyzer::free_dma() {
    int rc;

    if ( dalloc.dma_chan < 0 ) {
        errmsg = "No DMA channel to free";
        return false;
    }

    rc = ioctl(fd,RPIDMA_RELCHAN,0);
    if ( rc ) {
        std::stringstream ss;

        ss << strerror(errno) << ": Releasing DMA channel "
           << dalloc.dma_chan;
        errmsg = ss.str();
        dalloc.dma_chan = -1;
        return false;
    }

    dalloc.dma_chan = -1;
    return true;
}

bool
LogicAnalyzer::alloc_blocks(unsigned blocks) {

    // Release existing blocks
    for ( auto it = dma_blocks.begin(); it != dma_blocks.end(); ++it ) {
        s_block *block = *it;
        dmamem.free(block->data);
        delete block;
    }
    dma_blocks.clear();

    // Create the requested blocks
    dma_blocks.reserve(blocks);
    for ( unsigned ux=0; ux < blocks; ++ux ) {
        s_block *block = new s_block;
        if ( !block )
            return false;

        block->data = dmamem.allocate(pagespblk);
        block->dma_cb = (DMA::CB *)block->data;
        block->samples = ((uint32_t *)block->data) + 8;
	block->dma_cb->clear();
        dma_blocks.push_back(block);
    }

    return true;
}

DMA::CB&
LogicAnalyzer::get_cb() {
    assert(dma_blocks.size() > 0);
    return *dma_blocks[0]->dma_cb;
}

void
LogicAnalyzer::propagate() {

    if ( dma_blocks.size() <= 0 )
        return;

    s_block& first_block = *dma_blocks[0];
    DMA::CB& first_cb = *first_block.dma_cb;

    first_cb.TI.INTEN = 1;                		// Interrupts on
    first_cb.DEST_AD = dmamem.phys_addr(first_block.data) + 8 * sizeof(uint32_t);
    first_cb.TXFR_LEN = sampspblk * sizeof(uint32_t);

    for ( size_t ux=1; ux < dma_blocks.size(); ++ux ) {
        s_block& nth_block = *dma_blocks[ux];
        DMA::CB& nth_cb = *nth_block.dma_cb;

        nth_cb.TI = first_cb.TI;
        nth_cb.SOURCE_AD = first_cb.SOURCE_AD;
        nth_cb.DEST_AD = dmamem.phys_addr(nth_block.data) + 8 * sizeof(uint32_t);
	nth_cb.TI.DEST_DREQ = 0;
        nth_cb.TXFR_LEN = sampspblk * sizeof(uint32_t);
	nth_cb.TI.INTEN = 1;
    }

    // Chain the control blocks
    for ( unsigned ux=0; ux < dma_blocks.size(); ++ux ) {
        s_block& block = *dma_blocks[ux];

        if ( ux+1 < dma_blocks.size() ) {
            s_block& next = *dma_blocks[ux+1];

            // Point to next control block
            block.dma_cb->NEXTCONBK = dmamem.phys_addr(next.data);
        } else  {
            block.dma_cb->NEXTCONBK = 0;
        }
    }
}

uint32_t
LogicAnalyzer::get_gplev0() {
    uint32_t phys = GPIO::peripheral_base();

    return phys + 0x0034; // GPLEV0
}

bool
LogicAnalyzer::start() {

    assert(dalloc.dma_chan <= 0);

    if ( !alloc_dma() )
        return false;

    dma.set_channel(dalloc.dma_chan);
    dma.cs().RESET = 1;             // Reset DMA controller
    dma.cs().END = 1;
    dma.cs().INT = 1;

    uswait(50);                     // Wait for reset

    dma.debug().READL_ERROR = 1;    // Clear errors..
    dma.debug().FIFO_ERROR = 1;
    dma.debug().READ_ERROR = 1;
    
    s_block& block = *dma_blocks[0];
    uint32_t phy_cb0 = dmamem.phys_addr(block.data);

    dma.conblk_ad() = phy_cb0;      // Load CB
    dma.cs().ACTIVE = 1;            // Enable!
    return true;
}

bool
LogicAnalyzer::end() {

    assert(dalloc.dma_chan >= 0);

    dma.set_channel(dalloc.dma_chan);
    DMA::s_DMA_CS& status = dma.cs();

    while ( get_interrupts() < dma_blocks.size() )
        usleep(10);

    unsigned s = status.END;        // Capture status
    free_dma();                     // Release reservation on DMA

    return !!s;                     // True if status.END == 1
}

bool
LogicAnalyzer::abort(DMA::s_DMA_CS *status) {

    if ( dalloc.dma_chan < 0 )
        return false;

    dma.set_channel(dalloc.dma_chan);
    DMA::s_DMA_CS& cs = dma.cs();

    if ( status )
        *status = cs;               // Capture status

    cs.ACTIVE = 0;
    cs.RESET = 1;                   // Force DMA reset
    cs.END = 1;

    free_dma();                     // Release DMA & IRQ

    return true;
}

unsigned
LogicAnalyzer::get_interrupts() {
    int rc;
    
    assert(fd >= 0);                // Driver must be open

    rc = ioctl(fd,RPIDMA_INTINFO,&dalloc);
    assert(!rc);

    return dalloc.interrupts;
}

uint32_t *
LogicAnalyzer::get_samples(unsigned blockx,size_t *n_samples) {

    if ( n_samples )
        *n_samples = sampspblk;

    if ( blockx >= dma_blocks.size() )
        return 0;                   // Bad blockx

    // Skip over DMA CB:
    return ((uint32_t *)dma_blocks[blockx]->data) + 8;
}

void
LogicAnalyzer::dump_cb() {

    printf("DUMP of %u DMA CBs:\n",unsigned(dma_blocks.size()));

    for ( size_t ux=0; ux < dma_blocks.size(); ++ux ) {
        s_block& block = *dma_blocks[ux];
        uint32_t phy_addr = dmamem.phys_addr(block.data);
        DMA::CB& cb = *block.dma_cb;

        printf("  CB # %2u @ phy addr 0x%08X\n",
            unsigned(ux),
            unsigned(phy_addr));

        DMA::s_TI& ti = cb.TI;

        printf("    TI.INTEN :          %u\n",ti.INTEN);
        printf("    TI.TDMODE :         %u\n",ti.TDMODE);
        printf("    TI.WAIT_RESP :      %u\n",ti.WAIT_RESP);
        printf("    TI.DEST_INC :       %u\n",ti.DEST_INC);
        printf("    TI.DEST_WIDTH :     %u\n",ti.DEST_WIDTH);
        printf("    TI.DEST_DREQ :      %u\n",ti.DEST_DREQ);
        printf("    TI.DEST_IGNORE :    %u\n",ti.DEST_IGNORE);
        printf("    TI.SRC_INC :        %u\n",ti.SRC_INC);
        printf("    TI.SRC_WIDTH :      %u\n",ti.SRC_WIDTH);
        printf("    TI.SRC_DREQ :       %u\n",ti.SRC_DREQ);
        printf("    TI.SRC_IGNORE :     %u\n",ti.SRC_IGNORE);
        printf("    TI.BURST_LENGTH :   %u\n",ti.BURST_LENGTH);
        printf("    TI.PERMAP :         %u\n",ti.PERMAP);
        printf("    TI.WAITS :          %u\n",ti.WAITS);
        printf("    TI.NO_WIDE_BURSTS : %u\n",ti.NO_WIDE_BURSTS);
        
        printf("    SOURCE_AD :         0x%08X\n",cb.SOURCE_AD);
        printf("    DEST_AD :           0x%08X\n",cb.DEST_AD);
        printf("    TXFR_LEN :          %u\n",cb.TXFR_LEN);
        printf("    STRIDE :            0x%08X\n",cb.STRIDE);
        printf("    NEXTCONBK :         0x%08X\n",cb.NEXTCONBK);
    }        

    printf("END DMA CB DUMP.\n");
}

// End logana.cpp
