//////////////////////////////////////////////////////////////////////
// mtop.cpp -- Matrix "Top" for Raspberry Pi 2
// Date: Sat Feb 14 21:12:54 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include "mtop.hpp"
#include "matrix.hpp"
#include "mtop.hpp"
#include "piutils.hpp"

// Defaults for GPIO assignments

#define CLK_GPIO        16
#define DIN_GPIO        26
#define LOAD_GPIO       21

static volatile bool shutdown = false;

static void
sig_handler(int sig) {
    shutdown = true;
}

static int
vbarpct(double pct) {
    int vbar = pct * 8.0 / 100.0;

    if ( pct > 1.0 )
        ++vbar;
    return vbar;
}

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');
    
    if ( cp )
        cmd = cp+1; // Report basename of command

    printf(
        "Usage: %s [-options]\n"
        "where:\n"
        "\t-c clk_gpio\tSpecifies CLK gpio pin to use (%d)\n"
        "\t-d din_gpio\tSpecifies DIN gpio pin to use (%d)\n"
        "\t-l load_gpio\tSpecifies LOAD gpio pin to use (%d)\n"
	"\t-m meter_gpio\tSpecifies the GPIO to use for the meter (none)\n"
        "\t\t\tMeter gpio choices: 12, 13, 18 or 19 only\n"
        "\n"
        "The mtop command outputs 8 columns of activity in the\n"
        "matrix:\n\n"
        "   1   - CPU 1 utilization (leftmost)\n"
        "   2   - CPU 2 utilization\n"
        "   3   - CPU 3 utilization\n"
        "   4   - CPU 4 utilization\n"
        "   5   - Total memory utilization (includes disk cache)\n"
        "   6&7 - Total CPU utilizaton (all cores)\n"
        "   8   - Relative disk I/O activity (rightmost)\n\n"
        "Note:\n"
        "       Memory utilization can show 100%% due to disk\n"
        "       cache activity. This memory is reclaimed for\n"
        "       application use as needed by the kernel.\n",
        cmd,
        CLK_GPIO,
        DIN_GPIO,
        LOAD_GPIO);

    exit(0);
}

int
main(int argc,char **argv) {
    static const char options[] = "c:d:l:m:vh";
    int opt_clk = CLK_GPIO, opt_din = DIN_GPIO, opt_load = LOAD_GPIO;
    int opt_meter = 0;
    bool opt_errs = false, opt_verbose = false;
    MTop mtop;                      // CPU & memory utilization
    Diskstat dstat;                 // Disk I/O
    std::vector<double> cpus;       // Extracted CPU %
    int rc, optch;

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 'c':   // -c clk_gpio
            opt_clk = atoi(optarg);
            break;
        case 'd':   // -d din_gpio
            opt_din = atoi(optarg);
            break;
        case 'l':   // -l load_gpio
            opt_load = atoi(optarg);
            break;
        case 'm':
            opt_meter = atoi(optarg);
            switch ( opt_meter ) {
            case 12:
            case 13:
            case 18:
            case 19:
                // OK
                break;
            default:
                printf("GPIO # for the meter must be 12, 13, 18 or 19.\n");
                opt_errs = true;
            }
            break;
        case 'v':
            opt_verbose = true;
            break;
        case 'h':   // -h
            usage(argv[0]);
            break;
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

    if ( opt_verbose ) {
        printf(
            "CLK:  %d\n"
            "DIN:  %d\n"
            "LOAD: %d\n",
            opt_clk,opt_din,opt_load);
    }

    Matrix matrix(opt_clk,opt_din,opt_load);
    if ( opt_meter > 0 )
        matrix.set_meter(opt_meter);

    signal(SIGINT,sig_handler);	
    signal(SIGTERM,sig_handler);	

    rc = matrix.test(1);            // Put matrix into test mode
    if ( rc ) {
        fprintf(stderr,"%s: GPIO open failed.\n",strerror(rc));
        exit(1);

    }

    rc = mtop.sample(cpus);         // Take 1st CPU sample
    mswait(600);
    matrix.test(0);                 // Turn off test mode
    assert(!rc);                    // Make sure samples were taken

    // Loop forever
    while ( !shutdown ) {
        rc = mtop.sample(cpus);     // Take a 2nd/nth CPU sample
        assert(rc > 0);
	double total_cpu_pct = mtop.total_cpu_pct();

	if ( opt_meter > 0 )
		matrix.set_deflection(total_cpu_pct);

        for ( size_t x=1; x<=8; ++x ) {
            if ( x <= 4 ) // 1st four rows are CPUs 1-4 %
                matrix.display(x-1,vbarpct(cpus[x]));
            else if ( x == 5 ) // row 5 is memory % used
                matrix.display(x-1,vbarpct(mtop.memory_pct()));
            else if ( x >= 6 && x <= 7 ) // 6-7 is total CPU %
                matrix.display(x-1,vbarpct(cpus[0]));
            else if ( x == 8 ) // row 8 is disk i/o
                matrix.display(x-1,vbarpct(dstat.pct_io()));
        }

        // Reducing this sleep increases CPU % used,
        // but makes display more responsive
        mswait(80);
    }

    putchar('\n');
    matrix.Pi();

    for ( int x=0; x<6; ++x ) {
        matrix.config_intensity(9);
        mswait(60);
        matrix.config_intensity(0);
        mswait(30);
    }

    return 0;
}

// End mtop.cpp
