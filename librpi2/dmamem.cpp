//////////////////////////////////////////////////////////////////////
// dmamem.cpp -- DMA Memory class DmaMem Implementation
// Date: Sun Apr  5 16:37:42 2015  (C) Warren W. Gay VE3WWG 
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
#include <sys/ioctl.h>
#include <assert.h>

#include "gpio.hpp"
#include "dmamem.hpp"

//////////////////////////////////////////////////////////////////////
// Constructor
//////////////////////////////////////////////////////////////////////

DmaMem::DmaMem() : Mailbox() {
	;
}

//////////////////////////////////////////////////////////////////////
// Destructor: Here we release all allocations made
//////////////////////////////////////////////////////////////////////

DmaMem::~DmaMem() {

    for ( auto it = vtab.cbegin(); it != vtab.cend(); ) {
        const void *virtm = it->first;
        ++it;           // NB: free() will delete current entry
        free(virtm);    // Release *it
    }
    vtab.clear();
}

//////////////////////////////////////////////////////////////////////
// Allocate n pages
//////////////////////////////////////////////////////////////////////

void *
DmaMem::allocate(size_t pages,uint32_t mflags) {
    s_alloc node;

    if ( mflags == ~0u )
        mflags = mem_flag;

    node.pages = uint32_t(pages);
    node.mem_h = alloc(node.pages*page_size,page_size,mflags);
    if ( node.mem_h == ~0u )
        return 0;       // Failed

    node.lock_h = lock(node.mem_h);
    if ( node.lock_h == ~0u ) {
        int er = errno;
        release(node.mem_h);
        errno = er;
        return 0;       // Failed
    }

    node.virtm = map(to_phys_addr(node.lock_h),node.pages*page_size);
    if ( !node.virtm ) {
        int er = errno;
        unlock(node.lock_h);
        release(node.mem_h);
        errno = er;
        return 0;       // Failed
    }

    vtab[node.virtm] = node;

    return node.virtm;
}

//////////////////////////////////////////////////////////////////////
// Free an allocated chunk of memory
//////////////////////////////////////////////////////////////////////

bool
DmaMem::free(const void *addr) {

    auto it = vtab.find((void *)addr);
    if ( it == vtab.end() )
        return false;       // This allocation unknown

    s_alloc& node = it->second;

    if ( unmap(node.virtm,node.pages*page_size) )
        return false;       // Can't unmap

    if ( unlock(node.lock_h) )
        return false;       // Failed to unlock
    
    release(node.mem_h);
    vtab.erase(it);

    return true;
}

//////////////////////////////////////////////////////////////////////
// Return the bus handle
//////////////////////////////////////////////////////////////////////

DmaMem::bush_t
DmaMem::bus_handle(void *virtm) {
	auto it = vtab.find(virtm);
	if ( it == vtab.end() )
		return bush_t(~0);
	s_alloc& alloc = it->second;
	return alloc.lock_h;
}

//////////////////////////////////////////////////////////////////////
// Return the DMA (Physical) Address of this memory
//////////////////////////////////////////////////////////////////////

off_t
DmaMem::phys_addr(const void *addr) {

    auto it = vtab.find((void *)addr);
    if ( it == vtab.end() )
        return ~0u;         // Not found

    s_alloc& node = it->second;
    return to_phys_addr(node.lock_h);
}

// End dmamem.cpp
