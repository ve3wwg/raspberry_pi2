///////////////////////////////////////////////////////////////////////
// mailbox.cpp -- Kernel Mailbox Class Implementation
// Date: Sat Apr  4 09:31:50 2015  (C) Warren W. Gay VE3WWG 
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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "mailbox.hpp"
#include "piutils.hpp"

#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM,0,char *)
#define BUS2PHYS(x) ((x)&~0xC0000000)

// mem_flag:
#define MEM_FLAG_DISCARDABLE    (1 << 0)    // can be resized to 0 at any time. Use for cached data
#define MEM_FLAG_NORMAL         (0 << 2)    // normal allocating alias. Don't use from ARM
#define MEM_FLAG_DIRECT         (1 << 2)    // 0xC alias uncached
#define MEM_FLAG_COHERENT       (2 << 2)    // 0x8 alias. Non-allocating in L2 but coherent
#define MEM_FLAG_L1_NONALLOCATING (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) // Allocating in L2
#define MEM_FLAG_ZERO           (1 << 4)    // initialise buffer to all zeros
#define MEM_FLAG_NO_INIT        (1 << 5)    // don't initialise (default is initialise to all ones
#define MEM_FLAG_HINT_PERMALOCK (1 << 6)    // Likely to be locked for long periods of time.

Mailbox::Mailbox() {
    page_size = sys_page_size();
    pages = 0;
    fd = -1;
    mem_ref = ~0u;
    bus_addr = ~0u;
    virt_addr = 0;
    mem_flag = 0u;
    dram_phys_base = 0u;
}

Mailbox::~Mailbox() {
    close();
}

bool
Mailbox::create(uint32_t pages) {

    {
        std::string model, serial;
        uint32_t revision;
        Architecture arch;

        model_and_revision(model,arch,revision,serial);

        if ( arch == ARMv7 ) {
            mem_flag = MEM_FLAG_DIRECT;
            dram_phys_base = 0xc0000000;
        } else if ( arch == ARMv6 ) {
            mem_flag = MEM_FLAG_L1_NONALLOCATING;
            dram_phys_base = 0x40000000;
        }
    }

    path = "/dev/vcio";     // This changed in Linux 4.x
    this->pages = pages;

    fd = ::open(path.c_str(),O_RDWR);
    if ( fd == -1 )
        return false;

    mem_ref = alloc(pages*page_size,page_size,mem_flag);
    if ( mem_ref == ~0u ) {
        int er = errno;
        close();
        errno = er;
        return false;
    }

    bus_addr = lock(mem_ref);
    if ( bus_addr == ~0u ) {
        int er = errno;
        close();
        errno = er;
        return false;
    }

    virt_addr = virt(bus_addr,pages);
    if ( !virt_addr ) {
        int er = errno;
        close();
        errno = er;
        return false;
    }

    return true;
}

bool
Mailbox::close() {
    int rc = -1;

    if ( virt_addr != 0 ) {
        unmap(virt_addr,pages*page_size);
        virt_addr = 0;
    }

    if ( bus_addr != ~0u ) {
        unlock(bus_addr);
        bus_addr = ~0u;
    }

#if 0
    // This fails: prolly released by the driver at close()
    if ( mem_ref != ~0u ) {
        if ( release(mem_ref) == ~0u )
            fprintf(stderr,"mem_ref=%08X: Mailbox release failed.\n",
                mem_ref);
        mem_ref = ~0u;
    }
#endif

    if ( fd >= 0 ) {
        rc = ::close(fd);
        fd = -1;
    }

    return !rc;
}

bool
Mailbox::property(void *buf) {
    int rc;

    if ( fd < 0 ) {
        errno = EINVAL;
        return false;
    }

    rc = ioctl(fd,IOCTL_MBOX_PROPERTY,buf);
    return !rc;
}

uint32_t
Mailbox::alloc(uint32_t size,uint32_t align,uint32_t flags) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;               // final size
    msg[x++] = 0x00000000;      // process request
    
    msg[x++] = 0x3000c;         // tag id
    msg[x++] = 12;              // size of the buffer
    msg[x++] = 12;              // size of the data
    msg[x++] = size;
    msg[x++] = align;
    msg[x++] = flags;           // MEM_FLAG_L1_NONALLOCATING
    
    msg[x++] = 0x00000000;      // end tag
    msg[0] = x * sizeof msg[0]; // Actual size
    
    if ( property(msg) )
        return msg[5]; // Return handle
    return ~0u;
}

uint32_t
Mailbox::release(uint32_t handle) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    
    msg[x++] = 0x3000f;     // the tag id
    msg[x++] = 4;           // size of the buffer
    msg[x++] = 4;           // size of the data
    msg[x++] = handle;
    
    msg[x++] = 0x00000000;  // end tag
    msg[x] = x * sizeof msg[0]; // Actual size
    
    // Note: Seems to fail on 3.18.7-v7+ (flags?)

    if ( property(msg) )
        return msg[5];      // Success

    return ~0u;
}

uint32_t
Mailbox::lock(uint32_t handle) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    
    msg[x++] = 0x3000d;     // tag id
    msg[x++] = 4;           // size of the buffer
    msg[x++] = 4;           // size of the data
    msg[x++] = handle;
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0]; // Actual size
    
    if ( property(msg) )
        return bus_addr = msg[5];

    return ~0u;
}

uint8_t *
Mailbox::virt(uint32_t bus_addr,size_t pages) {
    virt_addr = (uint8_t *)Mailbox::map(to_phys_addr(bus_addr),pages*page_size);
    return virt_addr;
}

uint32_t
Mailbox::unlock(uint32_t handle) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    
    msg[x++] = 0x3000e;     // tag id
    msg[x++] = 4;           // size of the buffer
    msg[x++] = 4;           // size of the data
    msg[x++] = handle;
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0]; // Actual size
    
    if ( property(msg) )
        return msg[5];

    return ~0u;
}

uint32_t
Mailbox::execute(uint32_t code,uint32_t r0,uint32_t r1,uint32_t r2,uint32_t r3,uint32_t r4,uint32_t r5) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    
    msg[x++] = 0x30010;     // tag id
    msg[x++] = 28;          // size of the buffer
    msg[x++] = 28;          // size of the data
    msg[x++] = code;
    msg[x++] = r0;
    msg[x++] = r1;
    msg[x++] = r2;
    msg[x++] = r3;
    msg[x++] = r4;
    msg[x++] = r5;
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0];
    
    if ( property(msg) )
        return msg[5];
    return 0u;
}

uint32_t
Mailbox::qpu_enable(bool enable) {
    uint32_t msg[32];
    int x=0;
    
    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    
    msg[x++] = 0x30012;     // tag id
    msg[x++] = 4;           // size of buffer
    msg[x++] = 4;           // size of data
    msg[x++] = enable ? 1 : 0;
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0];
    
    if ( property(msg) )
        return msg[5];
    return ~0u;
}

uint32_t
Mailbox::execute_qpu(uint32_t n_qpus,uint32_t control,bool noflush,uint32_t timeout_ms) {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    msg[x++] = 0x30011;     // tag id
    msg[x++] = 16;          // size of buffer
    msg[x++] = 16;          // size of data
    msg[x++] = n_qpus;
    msg[x++] = control;
    msg[x++] = noflush ? 1 : 0;
    msg[x++] = timeout_ms;  // ms
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0];
    
    if ( property(msg) )
        return msg[5];
    return ~0u;
}

uint32_t
Mailbox::get_dma_channels() {
    uint32_t msg[32];
    int x=0;

    msg[x++] = 0;           // size
    msg[x++] = 0x00000000;  // process request
    msg[x++] = 0x60001;     // tag id
    msg[x++] = 4;           // size of buffer
    msg[x++] = 0;           // size of data
    msg[x++] = 0;           // u32 response area
    
    msg[x++] = 0x00000000;  // end tag
    msg[0] = x * sizeof msg[0];
    
    if ( property(msg) )
        return msg[5];
    return 0;
}

//////////////////////////////////////////////////////////////////////
// Static Methods
//////////////////////////////////////////////////////////////////////

void *
Mailbox::map(off_t offset,size_t bytes) {
    int fd = -1;

    // Needs root access:
    fd = ::open("/dev/mem",O_RDWR|O_SYNC);
    if ( fd < 0 )
        return 0;               // Failed (see errno)

    void *map = (char *) mmap(
        NULL,               // Any address
        bytes,              // # of bytes
        PROT_READ|PROT_WRITE,
        MAP_SHARED,         // Shared
        fd,                 // /dev/mem
        offset
    );
    
    if ( (long)map == -1L ) {
        int er = errno;
        ::close(fd);
        errno = er;
        return 0;
    }
    
    ::close(fd);

    return map;
}

int
Mailbox::unmap(void *addr,size_t bytes) {

    return munmap((caddr_t)addr,bytes);
}

off_t
Mailbox::to_phys_addr(uint32_t bus_addr) {
    return BUS2PHYS(bus_addr);
}

// End mailbox.cpp
