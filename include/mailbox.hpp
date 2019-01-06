//////////////////////////////////////////////////////////////////////
// mailbox.hpp -- Kernel Mailbox Class
// Date: Sat Apr  4 09:23:17 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef MAILBOX_HPP
#define MAILBOX_HPP

#include "gpio.hpp"

#include <string>


class Mailbox	{
    std::string path;           // Pathname of the device
    uint32_t    pages;          // Mailbox size in pages

protected:

    uint32_t    page_size;      // System page size
    int         fd;             // Mailbox fd
    uint32_t    mem_ref;        // From alloc()
    uint32_t    bus_addr;       // from lock()
    uint8_t     *virt_addr;     // mmap addr
    uint32_t    mem_flag;
    uint32_t    dram_phys_base;

public:

    Mailbox();
    ~Mailbox();

    inline const char *get_pathname() { return path.c_str(); }
    inline uint32_t get_page_size() { return page_size; }
    inline uint32_t get_mem_flag() { return mem_flag; }

    bool create(uint32_t pages);
    bool close();

    bool property(void *buf);

    uint32_t alloc(uint32_t size,uint32_t align,uint32_t flags);
    uint32_t release(uint32_t handle);
    uint32_t lock(uint32_t handle);
    uint32_t unlock(uint32_t handle);
    uint32_t execute(uint32_t code,uint32_t r0,uint32_t r1,uint32_t r2,uint32_t r3,uint32_t r4,uint32_t r5);
    uint32_t qpu_enable(bool enable);
    uint32_t execute_qpu(uint32_t n_qpus,uint32_t control,bool noflush,uint32_t timeout_ms);
    uint8_t *virt(uint32_t bus_addr,size_t pages);

    uint32_t get_dma_channels();

    // Static methods:
    static void *map(off_t offset,size_t bytes);    
    static int unmap(void *addr,size_t bytes);
    static off_t to_phys_addr(uint32_t bus_addr);
};

#endif // MAILBOX_HPP

// End mailbox.hpp
