//////////////////////////////////////////////////////////////////////
// dmamem.hpp -- DMA Memory class DmaMem 
// Date: Sun Apr  5 16:36:53 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef DMAMEM_HPP
#define DMAMEM_HPP

#include "mailbox.hpp"

#include <unordered_map>

//////////////////////////////////////////////////////////////////////
// Optional flags for allocate()
//////////////////////////////////////////////////////////////////////

#define MEM_FLAG_DISCARDABLE    (1 << 0)    // can be resized to 0 at any time. Use for cached data
#define MEM_FLAG_NORMAL         (0 << 2)    // normal allocating alias. Don't use from ARM
#define MEM_FLAG_DIRECT         (1 << 2)    // 0xC alias uncached
#define MEM_FLAG_COHERENT       (2 << 2)    // 0x8 alias. Non-allocating in L2 but coherent
#define MEM_FLAG_L1_NONALLOCATING (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) // Allocating in L2
#define MEM_FLAG_ZERO           (1 << 4)    // initialise buffer to all zeros
#define MEM_FLAG_NO_INIT        (1 << 5)    // don't initialise (default is initialise to all ones
#define MEM_FLAG_HINT_PERMALOCK (1 << 6)    // Likely to be locked for long periods of time.

//////////////////////////////////////////////////////////////////////
// The DmaMem class 
//////////////////////////////////////////////////////////////////////

class DmaMem : public Mailbox {
public:
    typedef uint32_t    memh_t;     // Memory handle
    typedef uint32_t    bush_t;     // Memory Bus handle
    typedef uint8_t     virtm_t;    // Virtual Pointer

private:
    struct s_alloc {
        size_t      pages;          // Allocated size
        memh_t      mem_h;          // Memory handle
        bush_t      lock_h;         // Bus handle
        void        *virtm;         // Mapped memory
    };

    // Table of allocations
    std::unordered_map<void*,s_alloc> vtab;

public:	DmaMem();
    ~DmaMem();

    void *allocate(size_t pages,uint32_t mflags=~0u);
    bool free(const void *addr);

    bush_t bus_handle(void *virtm);

    off_t phys_addr(const void *addr);
    inline uint32_t get_page_size() { return page_size; }
};


#endif // DMAMEM_HPP

// End dmamem.hpp
