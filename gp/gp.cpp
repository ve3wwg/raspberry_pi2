//////////////////////////////////////////////////////////////////////
// gp.cpp -- The gp gpio command for the Raspberry Pi
// Date: Thu Apr 30 18:59:27 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include "gpio.hpp"

#include <string>

static GPIO gpio;

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');

    if ( cp )
        cmd = cp + 1;

    fprintf(stderr,
        "Usage: %s [-g gpio] [-options] [-h]\n"
        "where:\n"
	"\t-g gpio\t\tSelects the gpio to operate upon\n"
	"\t-i\t\tConfigure gpio as Input\n"
	"\t-o\t\tConfigure gpio as Output\n"
	"\t-a {0-5}\tChange to Alternate function n\n"
	"\t-p {n|u|d}\tChange pullup to None, Up or Down\n"
	"\t-s n\t\tSet gpio value to 1 or 0 (non-zero=1)\n"
	"\t-r\t\tRead gpio bit\n"
	"\t-x\t\tRead (like -r) but return value as exit status\n"
	"\t-w\t\tRead all 32 gpio bits (-g ignored)\n"
	"\t-A\t\tRead alternate function setting for gpio\n"
	"\t-b n\t\tBlink gpio value for n times (0=forever)\n"
	"\t-m n\t\tMonitor gpio for changes (n seconds)\n"
	"\t-D\t\tDisplay all gpio configuration\n"
        "\t-C\t\tDisplay a chart of GPIO vs Alt functions\n"
	"\t-R n\t\tSet gpio pad slew rate limit on (1) or off (0)\n"
	"\t-H n\t\tSet gpio hysteresis enabled (1) or disabled (0)\n"
	"\t-S n\t\tSet gpio drive strength (0=2 mA .. 7=16 mA)\n"
        "\t-h\t\tThis info.\n\n"
        "\tAll options are executed in sequence.\n\n"
	"Example:\n"
	"\t$ %s -g12 -o -s1 -g13 -ir\n\n"
	"\tSets gpio 12 (-g12) to Output (-o), level to 1 (-s1),\n"
	"\tgpio 13 (-g13) as Input (-i) and reads it's value (-r).\n\n"
	"Note: -R/-H/-S affect groups of gpio: 0-27, 28-45, and 46-53.\n",
        cmd,cmd);
}

static void
display_all() {
    GPIO::IO io;
    bool slew_rate_limited, hysteresis;
    int drive, mA;

    putchar('\n');
    printf("GPIO ALTFUN LEV SLEW HYST DRIVE DESCRIPTION\n");
    printf("---- ------ --- ---- ---- ----- -----------\n");
    for (int gpno=0; gpno < 32; ++gpno ) {
        gpio.alt_function(gpno,io);
        gpio.get_drive_strength(
            gpno,
            slew_rate_limited,
            hysteresis,
            drive);

        mA = 2 + drive * 2;

        printf(" %2d  %-6.6s  %d    %c    %c  %2d mA %s\n",
            gpno,
            GPIO::alt_name(io),
            gpio.read(gpno),
            slew_rate_limited ? 'Y' : 'N',
            hysteresis ? 'Y' : 'N',
            mA,
            GPIO::gpio_alt_func(gpno,io));
    }
    putchar('\n');
    fflush(stdout);
}

static void
monitor(int gpno,int seconds) {
    time_t end = time(0);
    int v0, v, changes = 0;

    if ( seconds <= 0 )
        end += 7 * 24 * 3600;
    else
        end += seconds;
    
    v0 = gpio.read(gpno);
    puts("Monitoring..");
    printf("%06u GPIO %d = %d\n",changes,gpno,v0);

    do  {
        v = gpio.read(gpno);
        if ( v != v0 ) {
            printf("%06u GPIO %d = %d\n",++changes,gpno,v);
            v0 = v;
        } else  {
            usleep(150);
        }
    } while ( time(0) < end );
    puts("Monitoring ended.\n");
}

static void
disp_chart() {
    GPIO::IO alts[] = { GPIO::Alt0, GPIO::Alt1, GPIO::Alt2,
         GPIO::Alt3, GPIO::Alt4, GPIO::Alt5 };
    unsigned w[6];
    
    // Determine column widths
    for ( int x=0; x<6; ++x ) {
        GPIO::IO alt = alts[x];

        for ( int g=0; g<32; ++g ) {
            const char *desc = GPIO::gpio_alt_func(g,alt);

            if ( !g || strlen(desc) > w[x] )
                w[x] = strlen(desc);
        }
        if ( w[x] < 4 )
            w[x] = 4;
    }

    // Display chart    
    printf("\nGPIO ");
    for ( int x=0; x<6; ++x )
        printf("%-*.*sALT%d ",w[x]-4,w[x]-4,"",x);
    putchar('\n');
    
    fputs("---- ",stdout);
    for ( int x=0; x<6; ++x ) {
        const std::string line(w[x],'-');

	printf("%s ",line.c_str());
    }
    putchar('\n');

    for ( int g=0; g<32; ++g ) {
        printf(" %2d  ",g);

        for ( int x=0; x<6; ++x ) {
            GPIO::IO alt = alts[x];
            const char *desc = GPIO::gpio_alt_func(g,alt);

            printf("%*.*s ",w[x],w[x],desc);
        }
        putchar('\n');
    }
}

int
main(int argc,char **argv) {
    static const char options[] = "g:a:p:s:b:iorwADm:xR:H:S:Ch";
    int optch, gpno = -1, arg, er, xrc = 0;
    GPIO::IO io;
    GPIO::Pull pull;
    uint32_t u32;
    const char *cp = 0;
    bool slew, hyst;
    int drive;

    if ( argc <= 1 ) {
        usage(argv[0]);
        exit(0);
    }

    if ( gpio.get_error() ) {
        fprintf(stderr,
            "%s: Opening gpio\n",
            strerror(gpio.get_error()));
        exit(3);
    }

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 'g':
            if ( optarg[0] == '-' ) {
                fprintf(stderr,
                    "-g requires a gpio # argment.\n");
                exit(2);                    
            }
            gpno = atoi(optarg);
            break;
        case 'A':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-a %s)\n",
                    optarg);
                exit(2);
            }
            if ( (er = gpio.alt_function(gpno,io)) != 0 ) {
                fprintf(stderr,"%s: -g %d -A\n",
                    strerror(er),
                    gpno);
                exit(2);
            }
            cp = GPIO::alt_name(io);
            printf("GPIO %d : %s (%d)\n",gpno,cp,int(io));
            break;
        case 'a':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-a %s)\n",
                    optarg);
                exit(2);
            }
            arg = atoi(optarg);
            if ( arg < 0 || arg > 5 ) {
                fprintf(stderr,
                    "ALT # must be between 0 and 5: -a %s\n",
                    optarg);
                exit(2);
            }
            switch ( arg ) {
            case 0:
                io = GPIO::Alt0;
                break;
            case 1:
                io = GPIO::Alt1;
                break;
            case 2:
                io = GPIO::Alt2;
                break;
            case 3:
                io = GPIO::Alt3;
                break;
            case 4:
                io = GPIO::Alt4;
                break;
            case 5:
                io = GPIO::Alt5;
                break;
            }
            if ( (er = gpio.configure(gpno,io)) != 0 ) {
                fprintf(stderr,
                    "%s: Setting -g %d -a %d\n",
                    strerror(er),
                    gpno,
                    arg);
                exit(2);
            }
            break;
        case 'p':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-p %s)\n",
                    optarg);
                exit(2);
            }
            arg = *optarg;
            if ( arg >= 'a' && arg <= 'z' )
                arg = toupper(arg);

            switch ( arg ) {
            case 'N':   // None
                pull = GPIO::None;
                break;
            case 'U':
                pull = GPIO::Up;
                break;
            case 'D':
                pull = GPIO::Down;
                break;
            default :
                fprintf(stderr,
                    "Pullup argument must be N, U or D "
                    "(None, Up or Down): -g %d -p %s\n",
                    gpno,
                    optarg);
                exit(2);
            }
            if ( (er = gpio.configure(gpno,pull)) != 0 ) {
                fprintf(stderr,
                    "%s: Setting -g %d -p %c\n",
                    strerror(er),
                    gpno,
                    arg);
                exit(2);
            }
            break;
        case 's':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-s %s)\n",
                    optarg);
                exit(2);
            }
            arg = !!atoi(optarg);
            if ( (er = gpio.write(gpno,arg)) != 0 ) {
                fprintf(stderr,
                    "%s: Setting -g %d -s %d\n",
                    strerror(er),
                    gpno,
                    arg);
                exit(2);
            }
            break;
        case 'b':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-b %s)\n",
                    optarg);
                exit(2);
            }
            arg = atoi(optarg);
            u32 = !arg ? ~0u : uint32_t(arg);
            arg = er = 0;
            for ( uint32_t ux=0; !er && ux < u32; ++ux ) {
                arg ^= 1;
                if ( (er = gpio.write(gpno,arg)) != 0 )
                    break;
		printf("GPIO %d = %d (-b)\n",gpno,arg);
		usleep(500000);
                er = gpio.write(gpno,arg ^= 1);
		printf("GPIO %d = %d (-b)\n",gpno,arg);
		usleep(500000);
            }
            if ( er ) {
                fprintf(stderr,
                    "Setting gpio %d = %d (-b %s)\n",
                    gpno,
                    arg,
                    optarg);
                exit(2);
            }
            gpio.write(gpno,0);
            break;
        case 'm':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-m %s)\n",
                    optarg);
                exit(2);
            }
            arg = atoi(optarg);
            monitor(gpno,arg);
            break;
        case 'i':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-i)\n");
                exit(2);
            }
            if ( (er = gpio.configure(gpno,GPIO::Input)) != 0 ) {
                fprintf(stderr,
                    "%s: Setting -g %d -i\n",
                    strerror(er),
                    gpno);
                exit(2);
            }
            break;
        case 'o':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-o)\n");
                exit(2);
            }
            if ( (er = gpio.configure(gpno,GPIO::Output)) != 0 ) {
                fprintf(stderr,
                    "%s: Setting -g %d -o\n",
                    strerror(er),
                    gpno);
                exit(2);
            }
            break;
        case 'r':
	case 'x':
            if ( gpno < 0 ) {
                fprintf(stderr,
                    "No gpio specified with -g (-%c %s)\n",
                    optch,
                    optarg);
                exit(2);
            }
            arg = gpio.read(gpno);
            printf("GPIO %d = %d (-r)\n",gpno,arg);
            if ( optch == 'x' )
                xrc = arg;
            break;
        case 'w':
            u32 = gpio.read();
            printf("GPIO: 0x%08X (-w)\n",u32);
            break;
        case 'D':
            display_all();
            break;
        case 'R':
            if ( gpno < 0 || gpno > 53 ) {
                fprintf(stderr,
                    "No gpio specified/invalid with -g (-R %s)\n",
                    optarg);
                exit(2);
            }
            if ( gpio.get_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: obtaining -g %d pad control info (-R).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            slew = !!atoi(optarg);
            if ( gpio.set_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: setting -g %d pad control (-R).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            break;
        case 'H':
            if ( gpno < 0 || gpno > 53 ) {
                fprintf(stderr,
                    "No gpio specified/invalid with -g (-H %s)\n",
                    optarg);
                exit(2);
            }
            if ( gpio.get_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: obtaining -g %d pad control info (-H).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            hyst = !!atoi(optarg);
            if ( gpio.set_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: setting -g %d pad control (-H).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            break;
        case 'S':
            if ( gpno < 0 || gpno > 53 ) {
                fprintf(stderr,
                    "No gpio specified/invalid with -g (-S %s)\n",
                    optarg);
                exit(2);
            }
            if ( gpio.get_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: obtaining -g %d pad control info (-H).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            drive = atoi(optarg);
            if ( drive < 0 || drive > 7 ) {
                fprintf(stderr,"-g %d drive strength must be between "
                    "0 (2 mA) and 7 (16 mA) (-S %d)\n",
                    gpno,
                    drive);
                exit(2);                    
            }
            if ( gpio.set_drive_strength(gpno,slew,hyst,drive) ) {
                fprintf(stderr,"%s: setting -g %d pad control (-H).\n",
                    strerror(errno),
                    gpno);
                exit(2);                    
            }
            break;
        case 'C':
            disp_chart();
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
        case '?':
            printf("Unsupported option -%c\n",optopt);
            exit(2);
        case ':':
            printf("Option -%c requires an argument.\n",optopt);
            exit(2);
        default:
            fprintf(stderr,"Unsupported option: -%c\n",optch);
            exit(2);
        }
    }

    return xrc;
}

// End gp.cpp
