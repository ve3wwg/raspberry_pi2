//////////////////////////////////////////////////////////////////////
// testinfo.cpp -- Test model_and_revision()
// Date: Sat Apr  4 15:20:52 2015  (C) Warren W. Gay VE3WWG 
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "piutils.hpp"

int
main(int argc,char **argv) {
    std::string model, serial;
    uint32_t revision = ~0u;
    Architecture arch;

    if ( !model_and_revision(model,arch,revision,serial) || arch == Unknown ) {
        fprintf(stderr,"Failed to parse /proc/cpuinfo!\n");
        exit(1);
    }

    printf("Model '%s', revision %08X, serial '%s'\n",
        model.c_str(),
        revision,
        serial.c_str());

    if ( arch == ARMv6 )
        printf("Architecture: ARMv6\n");
    else if ( arch == ARMv7 ) 
        printf("Architecture: ARMv7\n");

    return 0;
}

// End testinfo.cpp
