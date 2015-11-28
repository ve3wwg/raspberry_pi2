//////////////////////////////////////////////////////////////////////
// dma.cpp -- DMA Class Implementation
// Date: Sun Apr  5 22:29:23 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <mutex>

#include "mailbox.hpp"
#include "dma.hpp"

#define DMA_BASE_OFFSET     0x00007000
#define DMA15_BASE_OFFSET   0x00E05000

// Physical Addresses:
#define DMA_CHAN0       0x7E007000  // Phy address of DMA channel 0
#define DMA_OFFSET      0x00000100  // DMA_CHAN1 = DMA_CHAN0 + DMA_OFFSET
#define DMA_INT_STATUS  0x7E007FE0
#define DMA_INT_ENABLE  0x7E007FF0
#define DMA_CHAN15      0x7EE05000  // This is isolated from the others

#define DMAOFF(o)	(((o)-0x7E000000-DMA_BASE_OFFSET)/sizeof(uint32_t))
#define DMAREG(o)	(*(udma+DMAOFF(o)))

#define DMA15OFF(o)	(((o)-0x7E000000-DMA15_BASE_OFFSET)/sizeof(uint32_t))
#define DMA15REG(o)	(*(udma15+DMA15OFF(o)))

static uint32_v *udma = 0;      // Pointer to DMA register 0
static uint32_v *udma15 = 0;
static std::mutex memlock;
static int usage_count = 0;

//////////////////////////////////////////////////////////////////////
// Inline method to access DMA n register regno
//////////////////////////////////////////////////////////////////////

inline uint32_v&
dma_register(int chan,DMA::DMA_Reg reg) {
    uint32_t regoff;

    assert(chan >= 0 && chan < 15);

    if ( chan < 15 ) {
        regoff = DMAOFF(DMA_CHAN0) + uint32_t(chan) * (DMA_OFFSET/sizeof(uint32_t));
        regoff += uint32_t(reg);
        return *(udma + regoff);
    } else  {
        assert(chan == 15);
        regoff = DMAOFF(DMA_CHAN15);
        regoff += uint32_t(reg);
        return *(udma + regoff);
    }
}

DMA::DMA() {

    errcode = 0;
    memlock.lock();
    if ( !udma ) {
        uint32_t peri_base = GPIO::peripheral_base();

        udma = (uint32_v *)Mailbox::map(peri_base+DMA_BASE_OFFSET,page_size);
        if ( !udma ) {
            errcode = errno;
            memlock.unlock();
            return;
        }

        udma15 = (uint32_v *)Mailbox::map(peri_base+DMA15_BASE_OFFSET,page_size);
        if ( !udma15 ) {
            errcode = errno;
            memlock.unlock();
            return;
        }
        ++usage_count;
    }
    memlock.unlock();

    channel = -1;
    p_cs = p_conblk_ad = p_ti = p_source_ad = p_dest_ad
        = p_txfr_len = p_stride = p_nextconbk = p_debug
        = p_int_status = p_int_enable = 0;
}

DMA::~DMA() {

    memlock.lock();
    if ( --usage_count <= 0 ) {
        if ( udma != 0 ) {
            Mailbox::unmap((void*)udma,page_size);
            udma = 0;
        }
        if ( udma15 != 0 ) {
            Mailbox::unmap((void *)udma15,page_size);
            udma = 0;
        }
    }
    memlock.unlock();

    channel = -1;
    p_cs = p_conblk_ad = p_ti = p_source_ad = p_dest_ad
        = p_txfr_len = p_stride = p_nextconbk = p_debug
        = p_int_status = p_int_enable = 0;
}

DMA::CB::CB() {
    clear();
}

void
DMA::CB::clear() {
    memset(&TI,0,8 * sizeof TI);
}

bool
DMA::set_channel(int ch) {

    if ( ch < 0 || ch >= 15 )	// Ch 15 not supported
        return false;

    channel = ch;
    p_cs = &dma_register(ch,DMA_CS);
    p_conblk_ad = &dma_register(ch,DMA_CONBLK_AD);
    p_ti = &dma_register(ch,DMA_TI);
    p_source_ad = &dma_register(ch,DMA_SOURCE_AD);
    p_dest_ad = &dma_register(ch,DMA_DEST_AD);
    p_txfr_len = &dma_register(ch,DMA_TXFR_LEN);
    p_stride = &dma_register(ch,DMA_STRIDE);
    p_nextconbk = &dma_register(ch,DMA_NEXTCONBK);
    p_debug = &dma_register(ch,DMA_DEBUG);
    
    p_int_status = &DMAREG(DMA_INT_STATUS);
    p_int_enable = &DMAREG(DMA_INT_ENABLE);

    return true;
}

// End dma.cpp
