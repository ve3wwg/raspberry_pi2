//////////////////////////////////////////////////////////////////////
// burn.cpp -- CPU Burner for Raspberry Pi 2
// Date: Sat Feb 14 23:09:35 2015  (C) Warren W. Gay VE3WWG 
///////////////////////////////////////////////////////////////////////

#include <math.h>

int
main(int argc,char **argv) {
    double x = 0.0;

    for (;;) {
        volatile double y = sin(x);
        (void)y;
        x += 0.0001;
    }
    return 0;
}

// End burn.cpp
