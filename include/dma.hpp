///////////////////////////////////////////////////////////////////////
// dma.hpp -- DMA Class
// Date: Sun Apr  5 22:26:22 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef DMA_HPP
#define DMA_HPP

#include "dmamem.hpp"

class DMA : public DmaMem {
public:
    enum DREQ {
        DREQ_0 = 0, // DREQ = 1, always on so use this channel if no DREQ is required.
        DREQ_1,     // DSI
        DREQ_2,     // PCM TX
        DREQ_3,     // PCM RX
        DREQ_4,     // SMI
        DREQ_5,     // PWM
        DREQ_6,     // SPI TX
        DREQ_7,     // SPI RX
        DREQ_8,     // BSC/SPI Slave TX
        DREQ_9,     // BSC/SPI Slave RX
        DREQ_10,    // unused
        DREQ_11,    // e.MMC
        DREQ_12,    // UART TX
        DREQ_13,    // SD HOST
        DREQ_14,    // UART RX.
        DREQ_15,    // DSI
        DREQ_16,    // SLIMBUS MCTX.
        DREQ_17,    // HDMI
        DREQ_18,    // SLIMBUS MCRX
        DREQ_19,    // SLIMBUS DC0
        DREQ_20,    // SLIMBUS DC1
        DREQ_21,    // SLIMBUS DC2
        DREQ_22,    // SLIMBUS DC3
        DREQ_23,    // SLIMBUS DC4
        DREQ_24,    // Scaler FIFO 0 & SMI *
        DREQ_25,    // Scaler FIFO 1 & SMI *
        DREQ_26,    // Scaler FIFO 2 & SMI *
        DREQ_27,    // SLIMBUS DC5
        DREQ_28,    // SLIMBUS DC6
        DREQ_29,    // SLIMBUS DC7
        DREQ_30,    // SLIMBUS DC8
        DREQ_31,    // SLIMBUS DC9
    };

    enum DMA_Reg {
        DMA_CS = 0,     // Control and status
        DMA_CONBLK_AD,  // Control block addr
        DMA_TI,         // Transfer info
        DMA_SOURCE_AD,  // Source address
        DMA_DEST_AD,    // Destination address
        DMA_TXFR_LEN,   // Transfer length
        DMA_STRIDE,     // 2D Stride
        DMA_NEXTCONBK,  // Next CB
        DMA_DEBUG       // Debug
    };

    struct s_DMA_CS {
        uint32_v    ACTIVE : 1;     // Enable DMA/Status
        uint32_v    END : 1;        // Set when complete/clear
        uint32_v    INT : 1;        // Interrupt status
        uint32_v    DREQ : 1;       // 1=Requesting data
        uint32_v    PAUSED : 1;     // 1=Paused
        uint32_v    DREQ_STOPS_DMA : 1; // 1=Paused
        uint32_v    WAITING : 1;    // 1=DMA channel waiting
        uint32_v    : 1;
        uint32_v    ERROR : 1;      // 1=Error
        uint32_v    : 7;
        uint32_v    PRIORITY : 4;   // AXI Priority (0 lowest)
        uint32_v    PANICPRI : 4;   // Panic priority
        uint32_v    : 4;
        uint32_v    WAIT_WRITES : 1; // Wait outstanding writes
        uint32_v    DISDEBUG : 1;   // 1= DMA will not for debug
        uint32_v    ABORT : 1;      // 1=abort current xfer
        uint32_v    RESET : 1;      // 1=Reset the DMA
    };

    struct s_TI {
        uint32_v    INTEN : 1;
        uint32_v    TDMODE : 1;
        uint32_v    : 1;
        uint32_v    WAIT_RESP : 1;
        uint32_v    DEST_INC : 1;
        uint32_v    DEST_WIDTH : 1;
        uint32_v    DEST_DREQ : 1;
        uint32_v    DEST_IGNORE : 1;
        uint32_v    SRC_INC : 1;
        uint32_v    SRC_WIDTH : 1;
        uint32_v    SRC_DREQ : 1;
        uint32_v    SRC_IGNORE : 1;
        uint32_v    BURST_LENGTH : 4;
        uint32_v    PERMAP : 5;
        uint32_v    WAITS : 5;
        uint32_v    NO_WIDE_BURSTS : 1;
        uint32_v    : 5;
    };

    struct s_DEBUG {
        uint32_v    READL_ERROR : 1;
        uint32_v    FIFO_ERROR : 1;
        uint32_v    READ_ERROR : 1;
        uint32_v    : 1;
        uint32_v    OUTWR : 4;
        uint32_v    DMA_ID : 8;
        uint32_v    DMA_STATE : 9;
        uint32_v    VERSION : 3;
        uint32_v    LITE : 1;
        uint32_v    : 3;
    };

    struct CB {
        s_TI        TI;         // 0
        uint32_v    SOURCE_AD;  // 1
        uint32_v    DEST_AD;    // 2
        uint32_v    TXFR_LEN;   // 3
        uint32_v    STRIDE;     // 4
        uint32_v    NEXTCONBK;  // 5
        uint32_v    mbz6;       // 6
        uint32_v    mbz7;       // 7

        CB();
        void clear();
    };

private:

    uint32_v    *p_cs;
    uint32_v    *p_conblk_ad;
    uint32_v    *p_ti;
    uint32_v    *p_source_ad;
    uint32_v    *p_dest_ad;
    uint32_v    *p_txfr_len;
    uint32_v    *p_stride;
    uint32_v    *p_nextconbk;
    uint32_v    *p_debug;

    uint32_v    *p_int_status;
    uint32_v    *p_int_enable;

    int         channel;
    int         errcode;

public:

    DMA();
    ~DMA();

    inline int get_channel() { return channel; }
    bool set_channel(int ch);

    inline uint32_v& u_cs() { return *p_cs; }
    inline uint32_v& u_i() { return *p_ti; }
    inline uint32_v& u_txfr_len() { return *p_txfr_len; }
    inline uint32_v& u_stride() { return *p_stride; }
    inline uint32_v& u_debug() { return *p_debug; }

    inline uint32_v& u_int_status() { return *p_int_status; }
    inline uint32_v& u_int_enable() { return *p_int_enable; }

    inline s_DMA_CS& cs() { return *(s_DMA_CS*)p_cs; }
    inline uint32_v& conblk_ad() { return *p_conblk_ad; }
    inline uint32_v& source_ad() { return *p_source_ad; }
    inline uint32_v& dest_ad() { return *p_dest_ad; }
    inline uint32_v& nextconbk() { return *p_nextconbk; }
    inline s_DEBUG& debug() { return *(s_DEBUG *)p_debug; }
};

#endif // DMA_HPP

// End dma.hpp
