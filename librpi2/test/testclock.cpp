//////////////////////////////////////////////////////////////////////
// testclock.cpp -- Test GPIO clock on GPIO # 4 # 100 Mhz
// Date: Sat Mar 28 15:41:55 2015  (C) Warren W. Gay VE3WWG 
///////////////////////////////////////////////////////////////////////

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "gpio.hpp"

static GPIO gpio;

static void
ready() {
    char buf[32];
    puts("Press RETURN when ready");
    fgets(buf,sizeof buf,stdin);
    putchar('\n');
}

int
main(int argc,char **argv) {
    int rc;

    puts("GPIO # 4 will be configured to generate 100.0 Mhz.\n"
        "Remove what is connected to GPIO 4, and attach a small\n"
        "wire to it to act as an antenna (do not use a long wire).\n");
    ready();

    puts("This test will run for approximately one minute. You should\n"
        "be able to hear 1 second of silence followed by 1 second of\n"
        "noise on an FM receiver tuned to 100.0 Mhz (you may need to\n"
        "turn off your receiver's auto-mute function to hear this).\n");

    for ( int x=0; x<30; ++x ) {
        rc = gpio.start_clock(GPIO_CLOCK,GPIO::PLLD,5,0,0);
        if ( rc ) {
            fprintf(stderr,"%s: Opening GPIO\n",strerror(gpio.get_error()));
            exit(1);
        }

	puts("Clock On..");
        sleep(1);

        gpio.stop_clock(GPIO_CLOCK);
	puts("Clock Off..");
        sleep(1);
    }

    puts("Test complete.\n");
    return 0;
}

// End testclock.cpp

