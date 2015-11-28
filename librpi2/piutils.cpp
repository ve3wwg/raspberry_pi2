//////////////////////////////////////////////////////////////////////
// piutils.cpp -- Raspberry Pi Utilities
// Date: Sat Mar 14 23:19:38 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "piutils.hpp"

void
nswait(unsigned long ns) {
    struct timespec req = { ns / 1000000ul, ns % 1000000ul };
    struct timespec rem;

    while ( nanosleep(&req,&rem) == -1 )
            req.tv_nsec = rem.tv_nsec;
}

void
uswait(unsigned long us) {
    struct timespec req = { us / 1000ul, us % 1000ul * 1000ul };
    struct timespec rem;

    while ( nanosleep(&req,&rem) == -1 )
            req.tv_nsec = rem.tv_nsec;
}

void
mswait(unsigned long ms) {
    struct timespec req = { ms / 1000ul, ms % 1000ul * 1000000ul };
    struct timespec rem;

    while ( nanosleep(&req,&rem) == -1 )
            req.tv_nsec = rem.tv_nsec;
}

uint32_t
sys_page_size() {
    return (uint32_t) sysconf(_SC_PAGESIZE);
}

static const char *
extract(const char *buf) {
    const char *cp = strchr(buf,':');

    if ( cp )
        return cp + 1 + strspn(cp+1," ");
    return buf;
}

//////////////////////////////////////////////////////////////////////
// Return model, architecture, revision and serial number
//////////////////////////////////////////////////////////////////////

bool
model_and_revision(std::string& model,Architecture& arch,uint32_t& revision,std::string& serial) {
    FILE *f = fopen("/proc/cpuinfo","r");
    char buf[256], *cp;
    
    model.clear();
    arch = Unknown;
    revision = ~0u;
    serial.clear();

    if ( !f )
        return false;   // Check errno

    while ( fgets(buf,sizeof buf,f) ) {
        if ( (cp = strrchr(buf,'\n')) != 0 )
            *cp = 0;
        if ( model.size() == 0 && !strncasecmp("model name\t",buf,9) ) {
            model = extract(buf);
        } else if ( revision == ~0u && !strncasecmp(buf,"Revision\t",9) ) {
            char *ep;
            revision = strtoul(extract(buf),&ep,16);
        } else if ( serial.size() == 0 && !strncasecmp(buf,"Serial\t",7) ) {
            serial = extract(buf);
	}
    }
	
    fclose(f);
    
    if ( model.size() <= 0 || serial.size() <= 0 )
        return false;

    if ( strstr(model.c_str(),"ARMv6") != 0 )
        arch = ARMv6;
    else if ( strstr(model.c_str(),"ARMv7") != 0 )
        arch = ARMv7;    
    else {
        arch = Unknown;
        return false;
    }

    return true;
}

// End piutils.cpp
