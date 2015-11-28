//////////////////////////////////////////////////////////////////////
// testfifo.cpp -- Test the PWM and FIFO facility of the GPIO class
// Date: Fri Apr  3 22:56:13 2015  (C) Warren W. Gay VE3WWG 
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
static bool opt_serialize = false;	// Else PWM
static bool opt_pwm = false;		// Else MS Algorithm
static int opt_n = 30;
static int opt_m = 70;
static int opt_idiv = 190;
static int opt_fdiv = 0;
static int opt_Mash = 0;
static int opt_gpio = 18;

static void usage(const char *cmd);

int
main(int argc,char **argv) {
    int rc;
    GPIO::s_PWM_status s;
    int optch;
    bool opt_errs = false;

    while ( (optch = getopt(argc,argv,"psg:n:m:i:f:M:h")) != -1 ) {
        switch ( optch ) {
        case 'g':
            opt_gpio = atoi(optarg);
            break;
        case 'i':
            opt_idiv = atoi(optarg);
            break;
        case 'f':
            opt_fdiv = atoi(optarg);
            break;
        case 'p':
            opt_pwm = true;
            break;
        case 'n':
            opt_n = atoi(optarg);
            break;
        case 'm':
            opt_m = atoi(optarg);
            break;
        case 'M':
            opt_Mash = atoi(optarg);
            break;
        case 's':
            opt_serialize = true;
            break;
        case 'h':               // -h   Help
            usage(argv[0]);
            exit(0);
            break;
        case '?':
            fprintf(stderr,"Unsupported option -%c\n",optopt);
            opt_errs = true;
            break;
        case ':':
            printf("Option -%c requires an argument.\n",optopt);
            opt_errs = true;
            break;
        default:
            fprintf(stderr,"Unsupported option: -%c\n",optch);
            opt_errs = true;
        }
    }

    if ( opt_errs ) {
        usage(argv[0]);
        exit(1);
    }

    rc = gpio.start_clock(opt_gpio,GPIO::Oscillator,opt_idiv,opt_fdiv,opt_Mash,true);
    assert(!rc);	

    if ( rc ) {
        fprintf(stderr,"%s: Opening GPIO %d for PWM use.\n",
            strerror(rc),opt_gpio);
        exit(1);
    }

    rc = gpio.pwm_configure(
        opt_gpio,
        opt_serialize ? GPIO::Serialize : GPIO::PWM_Mode,
        false,              // repeat
        0,                  // state
        false,              // invert
        true,               // fifo
        opt_pwm ? GPIO::PwmAlgorithm : GPIO::MSAlgorithm
    );
    assert(!rc);

    rc = gpio.pwm_ratio(opt_gpio,opt_n,opt_n);
    assert(!rc);

    rc = gpio.pwm_enable(opt_gpio,true);
    assert(!rc);

    puts("PWM with FIFO begins..");

    rc = gpio.pwm_status(opt_gpio,s);
    assert(!rc);

    printf("Initial Status PWM on GPIO %d:\n",opt_gpio);
    printf("  fifo_full:    %d\n",s.fifo_full);
    printf("  fifo_empty:   %d\n",s.fifo_empty);
    printf("  fifo_werr:    %d\n",s.fifo_werr);
    printf("  fifo_rerr:    %d\n",s.fifo_rerr);
    printf("  gap_occurred: %d\n",s.gap_occurred);
    printf("  bus_error:    %d\n",s.bus_error);
    printf("  chan_state:   %d\n",s.chan_state);

    static uint32_t data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    static uint32_t count = 11;

    gpio.pwm_write_fifo(opt_gpio,data,count);
    printf("%u words written to FIFO.\n",count);

    int sc = 0;

    do  {
        rc = gpio.pwm_status(opt_gpio,s);
        assert(!rc);
        if ( ++sc > 100000 )
            break;
    } while ( !s.chan_state );

    printf("Status PWM on GPIO %d:\n",opt_gpio);
    printf("  fifo_full:    %d\n",s.fifo_full);
    printf("  fifo_empty:   %d\n",s.fifo_empty);
    printf("  fifo_werr:    %d\n",s.fifo_werr);
    printf("  fifo_rerr:    %d\n",s.fifo_rerr);
    printf("  gap_occurred: %d\n",s.gap_occurred);
    printf("  bus_error:    %d\n",s.bus_error);
    printf("  chan_state:   %d\n",s.chan_state);

    sleep(20);

    // Shutdown:

    puts("Shutdown..");

    rc = gpio.pwm_enable(opt_gpio,false);
    assert(!rc);

    do  {
        rc = gpio.pwm_status(opt_gpio,s);
        assert(!rc);
    } while ( s.chan_state );

    printf("Status PWM on GPIO %d:\n",opt_gpio);
    printf("  fifo_full:    %d\n",s.fifo_full);
    printf("  fifo_empty:   %d\n",s.fifo_empty);
    printf("  fifo_werr:    %d\n",s.fifo_werr);
    printf("  fifo_rerr:    %d\n",s.fifo_rerr);
    printf("  gap_occurred: %d\n",s.gap_occurred);
    printf("  bus_error:    %d\n",s.bus_error);
    printf("  chan_state:   %d\n",s.chan_state);

    rc = gpio.stop_clock(opt_gpio);
    assert(!rc);

    puts("Test complete.\n");

    return 0;
}

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');

    if ( cp )
        cmd = cp+1;

    fprintf(stderr,
        "Usage: %s [-g gpio] [-i idiv] [-f fdiv] [-p] [-n n] [-m m] [-M n] [-s] [-h]\n"
        "where:\n"
        "\t-g gpio\t\tGPIO pin to use (12, 13, 18, or 19)\n"
        "\t-i idiv\t\tClock idiv value (default 190)\n"
        "\t-f fdiv\t\tClock fdiv value (default 0)\n"
        "\t-n n\t\tValue n for PWM ratio (default 30)\n"
        "\t-m m\t\tValue m for PWM ratio (default 70)\n"
        "\t-p\t\tUse PWM algorithm (MS by default)\n"
        "\t-M n\t\tMash value to use (default 0)\n"
        "\t-s\t\tSerialize the data (PWM is default)\n"
        "\t-h\t\tThis help info.\n",
        cmd);
}

// End testfifo.cpp
