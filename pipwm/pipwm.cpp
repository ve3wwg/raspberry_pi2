//////////////////////////////////////////////////////////////////////
// pipwm.cpp -- Set Hardware PWM 
// Date: Sun Mar 29 16:31:48 2015  (C) Warren W. Gay VE3WWG 
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
#include <time.h>
#include <assert.h>

#include "gpio.hpp"

static GPIO gpio;

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');
    
    if ( cp )
        cmd = cp+1; // Report basename of command

    fprintf(stderr,
        "Usage: %s [-options]\n"
        "where:\n"
        "\t-A { a | p }\tUse PWM algorithm or MS (MS is default)\n"
        "\t-i divi\t\tClock integer divisor (190)\n"
        "\t-f divf\t\tClock fractional divisor (0)\n"
        "\t-m mash\t\tClock mash config (0)\n"
        "\t-s src\t\tClock source (1)\n"
        "\t-g gpio\t\tPWM gpio pin (12)\n"
	"\t-b\t\tSerial data mode (default PWM mode)\n"
	"\t-c\t\tConfigure and start PWM peripheral\n"
        "\t-t secs\t\tRun PWM for secs\n"
	"\t-M m\t\tm value for PWM ratio (50)\n"
	"\t-S s\t\ts value for PWM ratio (100)\n"
	"\t-I\t\tInvert the PWM signal (not)\n"
	"\t-F\t\tUse FIFO vs Data (D)\n"
        "\t-R\t\tRepeat when FIFO empty (not)\n"
        "\t-Z { 0 | 1 }\tInitial state of PWM (0)\n"
	"\t-D\t\tDisplay PWM status\n"
	"\t-v\t\tVerbose\n"
        "\t-z\t\tStop the PWM peripheral\n"
        "\t-h\t\tThis info.\n\n"
	"Notes:\n"
        "\t* When -t omitted, PWM is left running (with -c)\n"
	"\t* GPIO must be 12 or 18 (PWM 0), 13 or 19 (PWM 1)\n"
	"\t* Only valid configurations allow the PWM to start\n"
	"\t* -s1 is default\n"
	"\t* For -s, src must be one of:\n"
	"\t\t%d - Grounded (no PWM)\n"
	"\t\t%d - Oscillator (19.2 MHz)\n"
	"\t\t%d - PLLA (audio ~393.216 MHz)\n"
	"\t\t%d - PLLC (1000 MHz, affected by overclocking)\n"
	"\t\t%d - PLLD (500 Mhz)\n"
	"\t\t%d - HDMI Aux (216 MHz?)\n\n"
	"\tSee also the piclk command.\n\n"
	"Examples:\n"
	"\tpipwm -g12 -c    # Configure PWM 0 on gpio 12 with defaults using PWM\n"
	"\tpipwm -D         # Display PWM parameters\n",
        cmd,
	GPIO::Gnd,
	GPIO::Oscillator,
	GPIO::PLLA,
	GPIO::PLLC,
	GPIO::PLLD,
	GPIO::HDMI_Aux);
}

static void
display_pwm() {
    static const int gpios[] = { 12, 13, 18, 19, -1 };
    GPIO::IO io;

    putchar('\n');
    puts(" CLOCK  GPIO ALTFUN ON DIVI DIVF MASH ENAB SRC            M    S  M/P E S/P R S I F");
    puts(" -----  ---- ------ -- ---- ---- ---- ---- ------------ ---- ---- --- - --- - - - -");

    for ( int x=0; gpios[x] >= 0; ++x ) {
        int clk_gpio = gpios[x];
        GPIO::Source src;
        unsigned divi, divf, mash, m, s;
        bool enabled;
        const char *clock_name, *alt;
	GPIO::s_PWM_status status;
	GPIO::s_PWM_control control;
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

	rc = gpio.get_pwm_ratio(clk_gpio,m,s);

	gpio.pwm_control(clk_gpio,control);
	gpio.pwm_status(clk_gpio,status);

        printf(" %-6.6s  %2d  %-6.6s  %c %4u %4u %4u   %c  %-12.12s %4u %4u %s %c %s %c %d %c %c\n",
            clock_name,
            clk_gpio,
            alt,
            on,
            divi,
            divf,
            mash,
            enabled ? 'Y' : 'N',
            GPIO::source_name(src),
            m,
            s,
            control.MSENx ? "M/S" : "PWM",
            control.PWENx ? 'Y' : 'N',
            control.MODEx ? "PWM" : "Ser",
            control.RPTLx ? 'Y' : 'N',
            control.SBITx ? 1 : 0,
            control.POLAx ? 'Y' : 'N',
            control.USEFx ? 'F' : 'D');
    }

    puts(
        "\nPWM Legend:\n\n"
        "  CLOCK ..       Clock peripheral name\n"
        "  GPIO ... [-g:] GPIO pin for PWM output\n"
        "  ALTFUN .       Current GPIO alternate function state\n"
        "  ON .....       P=PWM clock (C=GP0CLK)\n"
        "  DIVI ... [-i:] Integer clock divisor\n"
        "  DIVF ... [-f:] Fractional clock divisor\n"
        "  MASH ... [-m:] Clock mash value (0,1,2 or 3)\n"
        "  ENAB ... [-c]  Clock enabled\n"
        "  SRC .... [-s:] Clock source\n"
        "  M ...... [-M:] M value of PWM M and S parameters\n"
        "  S ...... [-S:] S Value of PWM M and S parameters\n"
        "  M/P .... [-A:] M/S or PWM mode\n"
        "  E ...... [-c]  PWM enabled\n"
        "  S/P .... [-b]  Serial or PWM data\n"
        "  R ...... [-R]  Empty FIFO repeats\n"
        "  S ...... [-Z:] Initial state of PWM\n"
        "  I ...... [-I]  Inverted\n"
        "  F ...... [-F]  FIFO enabled (F) or Data (D)\n");
}

int
main(int argc,char **argv) {
    int rc, gpno = 12;
    GPIO::s_PWM_status s;
    static const char options[] = "A:i:f:m:s:g:M:S:Pt:vczDIFRZ:h";
    int opt_i = 190, opt_f = 0, opt_m = 0;
    int opt_s = GPIO::Oscillator, opt_t = 0;
    int opt_M = 30, opt_S = 100, opt_Z = 0;
    bool opt_errs = false, opt_verbose = false, opt_z = false;
    bool opt_D = false, opt_A_MS = true, opt_c = false;
    bool opt_Invert = false, opt_Fifo = false, opt_Repeat = false;
    bool opt_b = false;
    int optch;

    if ( argc <= 1 ) {
        usage(argv[0]);
        return 0;
    }

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 'A':
            switch ( optarg[0] ) {
            case 'm':
            case 'M':
                opt_A_MS = true;
                break;
            case 'p':
            case 'P':
                opt_A_MS = false;
                break;
            default :
                fprintf(stderr,
                    "Invalid argument: -A %s\n",
                    optarg);
                exit(2);
            }
            break;
        case 'i':   // -i divi
            opt_i = atoi(optarg);
            break;
        case 'f':   // -f divf
            opt_f = atoi(optarg);
            break;
        case 'm':   // -m mash
            opt_m = atoi(optarg);
            break;
        case 's':   // -s src
            opt_s = atoi(optarg);
            break;
        case 'g':   // -g gpio
            gpno = atoi(optarg);
            break;
        case 'M':   // -M m
            opt_M = atoi(optarg);
            break;
        case 'S':   // -S s
            opt_S = atoi(optarg);
            break;
        case 'I':   // -I
            opt_Invert = true;
            break;
        case 'F':
            opt_Fifo = true;
            break;
        case 'R':
            opt_Repeat = true;
            break;
        case 'Z':
            opt_Z = !!atoi(optarg);
            break;
        case 'b':
            opt_b = true;
            break;
        case 't':   // -t secs
            opt_t = atoi(optarg);
            break;
        case 'v':   // -v
            opt_verbose = true;
            break;
        case 'c':   // -c
            opt_c = true;
            break;
        case 'z':   // -z
            opt_z = true;
            break;
        case 'D':   // -D
            opt_D = true;
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

    if ( opt_c && opt_z ) {
        fprintf(stderr,
            "Ambiguous command: using both -c and -z\n");
        exit(2);
    }

    if ( !opt_c && !opt_z && !opt_D ) {
        fprintf(stderr,
            "Nothing to do: Supply -c, -z or -D\n");
        exit(2);
    }

    if ( !opt_z ) {
        if ( opt_i < 0 || opt_f < 0 || opt_m < 0 || gpno < 0 )
            ++opt_errs;
    } else  {
        if ( gpno < 0 )
            ++opt_errs;
    }

    if ( opt_errs ) {
        usage(argv[0]);
        exit(1);
    }

    if ( opt_c ) {
        rc = gpio.start_clock(
            gpno,
            GPIO::Source(opt_s),
            opt_i,
            opt_f,
            opt_m,
            true
        );
        assert(!rc);	

        if ( rc ) {
            fprintf(stderr,"%s: Opening GPIO %d for PWM use.\n",
                strerror(rc),gpno);
            exit(1);
        }

        rc = gpio.pwm_configure(
            gpno,
            opt_b ? GPIO::Serialize : GPIO::PWM_Mode,
            opt_Repeat,
            opt_Z,
            opt_Invert,
            opt_Fifo,
            opt_A_MS ? GPIO::MSAlgorithm : GPIO::PwmAlgorithm
        );

        if ( rc != 0 ) {
            fprintf(stderr,"%s: Configuring PWM on gpio %d\n",
                strerror(errno),
                gpno);
            exit(2);
        }

        rc = gpio.pwm_ratio(gpno,opt_M,opt_S);
        assert(!rc);

        rc = gpio.pwm_enable(gpno,true);
        assert(!rc);

        time_t t0 = time(0);

        do  {
            rc = gpio.pwm_status(gpno,s);
            assert(!rc);
            if ( time(0) - t0 >= 3 ) {
                puts("Timed out..");
                break;              // Timeout
            }
        } while ( !s.chan_state );

        if ( opt_verbose ) {
            printf("Status PWM on GPIO %d:\n",gpno);
            printf("  fifo_full:    %d\n",s.fifo_full);
            printf("  fifo_empty:   %d\n",s.fifo_empty);
            printf("  fifo_werr:    %d\n",s.fifo_werr);
            printf("  fifo_rerr:    %d\n",s.fifo_rerr);
            printf("  gap_occurred: %d\n",s.gap_occurred);
            printf("  bus_error:    %d\n",s.bus_error);
            printf("  chan_state:   %d\n",s.chan_state);
        }
    }

    if ( opt_c && opt_t > 0 )
        sleep(opt_t);

    if ( opt_z || opt_t > 0 ) {
        // Shutdown:
        rc = gpio.pwm_enable(gpno,false);
        assert(!rc);

        do  {
            rc = gpio.pwm_status(gpno,s);
            assert(!rc);
        } while ( s.chan_state );

        if ( opt_verbose ) {
            printf("Shutdown Status PWM on GPIO %d:\n",gpno);
            printf("  fifo_full:    %d\n",s.fifo_full);
            printf("  fifo_empty:   %d\n",s.fifo_empty);
            printf("  fifo_werr:    %d\n",s.fifo_werr);
            printf("  fifo_rerr:    %d\n",s.fifo_rerr);
            printf("  gap_occurred: %d\n",s.gap_occurred);
            printf("  bus_error:    %d\n",s.bus_error);
            printf("  chan_state:   %d\n",s.chan_state);
        }

        rc = gpio.stop_clock(gpno);
        assert(!rc);
    }

    if ( !opt_z && opt_c ) {
        // Report status if not -z
        if ( s.chan_state ) {
            if ( opt_verbose )
                puts("PWM left running..\n");
        } else {
            puts("PWM not running?\n");
            if ( opt_D )
                display_pwm();
            return 2;
        }
    }

    if ( opt_D )
        display_pwm();

    return 0;
}

// End pipwm.cpp

