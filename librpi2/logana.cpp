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

    dalloc.slave_id = 0;        // No DREQ
    dalloc.page_sz = 0;
    dalloc.src_addr = 0;
    dalloc.n_dst = 0;
    dalloc.pdst_addr = nullptr;
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

    fd = ::open("/dev/rpidma4x",O_RDONLY);
    if ( fd < 0 ) {
        ss << strerror(errno) << ": Opening driver /dev/rpidma4x";
        errmsg = ss.str();
        return false;
    }

    // Open mailbox interface:

    if ( !dmamem.create(1) ) {
        ss  << strerror(errno) << ": opening mailbox /dev/vcio";
        errmsg = ss.str();
        return false;
    }

    pagesize = dmamem.get_page_size();
    sampspblk = ( ( pagesize * pagespblk ) / sizeof(uint32_t) );

    return true;
}

void
LogicAnalyzer::close() {

    if ( fd >= 0 ) {
        ::close(fd);
        fd = -1;
    }

    dma_blocks.clear();
    sg_list.clear();
    dmamem.close();
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

    sg_list.clear();

    // Create the requested blocks
    dma_blocks.reserve(blocks);
    for ( unsigned ux=0; ux < blocks; ++ux ) {
        s_block *block = new s_block;
        if ( !block ) {
            return false;
	}

        block->data = dmamem.allocate(pagespblk);
        block->dma_cb = (DMA::CB *)block->data;
        block->samples = ((uint32_t *)block->data) + 8;
	block->dma_cb->clear();
        dma_blocks.push_back(block);
        sg_list.push_back(dmamem.phys_addr(block->data));
    }

    return true;
}

uint32_t
LogicAnalyzer::get_gplev0() {
    uint32_t phys = GPIO::peripheral_base();

    return phys + 0x0034; // GPLEV0
}

bool
LogicAnalyzer::start(unsigned long src_addr) {
    s_rpidma_ioctl rpidma;    
    int rc;

    assert(fd >= 0);                // Driver must be open
        
    rpidma.slave_id = 0;
    rpidma.page_sz = pagesize * sizeof(uint32_t);	// Bytes
    rpidma.src_addr = src_addr;
    
    rpidma.n_dst = sg_list.size();
    rpidma.pdst_addr = (uint32_t *)sg_list.data();

    rc = ioctl(fd,RPIDMA_START,&rpidma);
    assert(!rc);

    return true;
}

//////////////////////////////////////////////////////////////////////
// Return 0 == incomplete, 1 == completed or < 0 for error
//////////////////////////////////////////////////////////////////////

int
LogicAnalyzer::is_completed() {

    return ioctl(fd,RPIDMA_STATUS,0);
}

uint32_t *
LogicAnalyzer::get_samples(unsigned blockx,size_t *n_samples) {

    if ( n_samples )
        *n_samples = sampspblk;

    if ( blockx >= dma_blocks.size() )
        return 0;                   // Bad blockx

    // Skip over DMA CB:
    return (uint32_t *)dma_blocks[blockx]->data;
}

// End logana.cpp
