//////////////////////////////////////////////////////////////////////
// piclk.cpp -- Control GPIO clock on GPIO # 4
// Date: Sat Mar 28 15:41:55 2015  (C) Warren W. Gay VE3WWG 
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
#include <signal.h>
#include <assert.h>

#include "gpio.hpp"

static GPIO gpio;
static volatile bool quit_flag = false;

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');
    
    if ( cp )
        cmd = cp+1; // Report basename of command

    fprintf(stderr,
	"Usage: %s [-g gpio] [-i divi] [-f divf] [-m mash] [-e {1:0}] [-s src] [-q] [-z] [-D] [-v] [-h]\n"
        "where:\n"
	"\t-g gpio\t\tclock chosen by gpio # (default 4)\n"
        "\t-i divi\t\tDIVI divisor value (5)\n"
        "\t-f divf\t\tDIVF divisor value (0)\n"
        "\t-m mash\t\tMash value 0-3 (0)\n"
        "\t-e enable\tEnable/disable on gpio (1)\n"
	"\t-s src\t\tSelect clock source (%d)\n"
	"\t-q\t\tDon't start the clock (used with -D)\n"
        "\t-z\t\tStop the clock peripheral\n"
	"\t-b\t\tBlink on/off in .5 second intervals\n"
	"\t-D\t\tDisplay clock settings\n"
	"\t-v\t\tVerbose\n"
        "\t-h\t\tThis info.\n\n"
	"Notes:\n"
        "\t* Clock drives gpio 4, when enabled (-e1).\n"
        "\t* Defaults to 100 MHz (-i5 -f0 -m0 -e1)\n"
        "\t* Enabled on gpio pin by default (-e1)\n"
        "\t* Most other options ignored when -z is used.\n"
	"\t* GPCLK0 on gpio 4 needs Alt0 (use -e1)\n"
	"\t* PWM0 on gpio 12 or 13 needs Alt0 (use -e1)\n"
	"\t* PWM1 on gpio 18 or 19 needs Alt5 (use -e1)\n"
	"\t* GPCLK0 output is a clock (C), vs PWM output (P)\n"
	"\t* Max operating frequency on gpio pin is approx 125 MHz\n"
	"\t  at about 1.2V in amplitude, with no load.\n"
	"\t* For -s, src must be one of:\n"
	"\t\t%d - Grounded (no clock)\n"
	"\t\t%d - Oscillator (19.2 MHz)\n"
	"\t\t%d - PLLA (audio ~393.216 MHz)\n"
	"\t\t%d - PLLC (1000 MHz, affected by overclocking)\n"
	"\t\t%d - PLLD (500 Mhz, default)\n"
	"\t\t%d - HDMI Aux (216 MHz?)\n\n"
	"\tSee also the pipwm command.\n",
        cmd,
	GPIO::PLLD,
	GPIO::Gnd,
	GPIO::Oscillator,
	GPIO::PLLA,
	GPIO::PLLC,
	GPIO::PLLD,
	GPIO::HDMI_Aux);
}

static void
sighandler(int signo) {
    quit_flag = true;
    write(1,"\nQuitting..\n",12);
}

static void
display_clocks() {
    static const int gpios[] = { GPIO_CLOCK, 12, 13, 18, 19, -1 };
    GPIO::IO io;

    puts(" CLOCK  GPIO ALTFUN ON DIVI DIVF MASH ENAB SRC");
    puts(" -----  ---- ------ -- ---- ---- ---- ---- ---");

    for ( int x=0; gpios[x] >= 0; ++x ) {
        int clk_gpio = gpios[x];
        GPIO::Source src;
        unsigned divi, divf, mash;
        bool enabled;
        const char *clock_name, *alt;
        char on;

        if ( !gpio.alt_function(clk_gpio,io) )
            alt = GPIO::alt_name(io);
        else
            alt = "?";

        switch( clk_gpio ) {
        case 12:
            clock_name = "PWMCLK";
            on = io == GPIO::Alt0 ? 'P' : '-';
            break;
	case 13:
            clock_name = "PWMCLK";
            on = io == GPIO::Alt0 ? 'P' : '-';
            break;
        case 18:
            clock_name = "PWMCLK";
            on = io == GPIO::Alt5 ? 'P' : '-';
            break;
	case 19:
            clock_name = "PWMCLK";
            on = io == GPIO::Alt5 ? 'P' : '-';
            break;
        case GPIO_CLOCK:
            clock_name = "GPCLK0";
            on = io == GPIO::Alt0 ? 'C' : '-';
            break;
        default:
            clock_name = "?";
        }

        int rc = gpio.config_clock(
            clk_gpio,
            src,
            divi,
            divf,
            mash,
            enabled);
        assert(!rc);

        printf(" %-6.6s  %2d  %-6.6s  %c %4u %4u %4u   %c  %s\n",
            clock_name,clk_gpio,alt,on,
            divi,divf,mash,
            enabled ? 'Y' : 'N',
            GPIO::source_name(src));
    }
}

int
main(int argc,char **argv) {
    static const char options[] = "g:i:f:m:e:s:qzbDvh";
    int rc, optch, opt_i=5, opt_f=0, opt_m=0, opt_g = 4;
    int opt_s = int(GPIO::PLLD);
    bool opt_z = false, opt_b = false, opt_v = false;
    bool opt_e = true, opt_errs = false, opt_D = false;
    bool opt_q = false;

    if ( argc <= 1 ) {
        usage(argv[0]);
        exit(0);
    }

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 'g':   // -g gpio
            opt_g = atoi(optarg);
            switch ( opt_g ) {
            case GPIO_CLOCK:
            case 12:
            case 13:
            case 18:
            case 19:
            case 28:
                break;
            default:
                fprintf(stderr,
                    "-g %d is not supported\n",
                    opt_g);
                exit(2);
            }
            break;
        case 'i':   // -i divi
            opt_i = atoi(optarg);
            if ( opt_i < 0 || opt_i > 0x0FFF ) {
                fprintf(stderr,
                    "idiv in -i %d must be range 0..%d\n",
                    opt_i,0x0FFF);
                exit(2);
            }
            break;
        case 'f':   // -f divf
            opt_f = atoi(optarg);
            if ( opt_f < 0 || opt_f > 0x0FFF ) {
                fprintf(stderr,
                    "fdiv in -f %d must be range 0..%d\n",
                    opt_f,0x0FFF);
                exit(2);
            }
            break;
        case 'm':   // -m mash
            opt_m = atoi(optarg);
            if ( opt_m < 0 || opt_m > 3 ) {
                fprintf(stderr,
                    "Mash in -m %d must be range 0..3\n",
                    opt_m);
                exit(2);
            }
            break;
        case 'e':
            opt_e = atoi(optarg) != 0;
            break;
        case 's':   // -s src
            opt_s = atoi(optarg);
            if ( opt_s < 0 || opt_s > 7 ) {
                fprintf(stderr,
                    "src in -s %d must be range 0..7\n",
                    opt_s);
                exit(2);
            }
            break;
        case 'b':
            opt_b = true;
            break;
        case 'D':   // -D
            opt_D = true;
            break;
        case 'v':   // -v
            opt_v = true;
            break;
        case 'q':   // -q
            opt_q = true;
            break;
        case 'z':   // -z
            opt_z = true;
            break;
        case 'h':   // -h
            usage(argv[0]);
            exit(0);
        case '?':
            printf("Unsupported option -%c\n",optopt);
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
        exit(2);
    }

    if ( opt_z ) {
        // Stop the clock
        gpio.stop_clock(opt_g);
        if ( opt_v )
            printf("Clock on gpio %d has been stopped.\n",
                opt_g);
        if ( opt_D )
            display_clocks();
        exit(0);
    }

    if ( opt_b && !opt_e ) {
        fprintf(stderr,"WARNING: -b implies -e1\n");
        opt_e = true;
    }

    if ( !opt_z && !opt_q ) {
        // Start the clock
        rc = gpio.start_clock(
            opt_g,
            GPIO::Source(opt_s),
            opt_i,
            opt_f,
            opt_m,
            opt_e);
        if ( rc ) {
            fprintf(stderr,"%s: Opening GPIO\n",strerror(gpio.get_error()));
            exit(1);
        }

        if ( opt_v ) {
            puts("Clock started..");
            if ( opt_g == GPIO_CLOCK ) {
                if ( opt_e )
                    printf("and driving gpio %d.\n",
                        opt_g);
                else
                    printf("and not driving gpio %d.",
                        opt_g);
            }
        }
    }

    if ( opt_b ) {
        signal(SIGINT,sighandler);
        puts("Press ^C to quit..");

        // Execute a blink on/off cycle
	for (;;) {
            usleep(500000);
            gpio.stop_clock(opt_g);
            if ( opt_v )
                printf("Clock off (stopped: -g %d)\n",
                    opt_g);
    
            if ( quit_flag )
                break;

            usleep(500000);

            gpio.start_clock(
                opt_g,
                GPIO::Source(opt_s),
                opt_i,
                opt_f,
                opt_m,
                opt_e);
            if ( opt_v )
                printf("Clock on (running: -g %d)\n",
                    opt_g);
        }
    }

    if ( opt_D )
        display_clocks();

    return 0;
}

// End piclk.cpp
