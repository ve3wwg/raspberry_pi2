//////////////////////////////////////////////////////////////////////
// burn.cpp -- CPU Burner for Raspberry Pi 2
// Date: Sat Feb 14 23:09:35 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
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
