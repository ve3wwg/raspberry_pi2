///////////////////////////////////////////////////////////////////////
// pispy.cpp -- Raspberry Pi Logic Anaylizer
// Date: Tue Apr 21 21:18:24 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <assert.h>

#include "dmamem.hpp"
#include "gpio.hpp"
#include "piutils.hpp"
#include "logana.hpp"
#include "vcdout.hpp"

#define PAGES   4

#define TRIG_R  1   // Rising
#define TRIG_F  2   // Falling
#define TRIG_H  4   // High
#define TRIG_L  8   // Low

static int opt_blocks = 8;
static bool opt_verbose = false;
static GPIO gpio;

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');

    if ( cp )
        cmd = cp + 1;

    fprintf(stderr,
        "Usage: %s [-b blocks] [-R gpio] [-F gpio] [-H gpio] [-L gpio] [-T n] [-x] [-z]\n"
        "where:\n"
        "\t-b blocks\tHow many %uk blocks to sample (8)\n"
        "\t-R gpio\t\tTrigger on rising edge\n"
        "\t-F gpio\t\tTrigger on falling edge\n"
        "\t-H gpio\t\tTrigger on level High\n"
        "\t-L gpio\t\tTrigger on level Low\n"
	"\t-T tries\tRetry trigger attempt n times (100)\n"
	"\t-v\t\tVerbose\n"
	"\t-x\t\tDon't try to execute gtkwave\n"
	"\t-z\t\tDon't suppress gtkwave messages\n"
        "\t-h\t\tThis info.\n\n"
	"Notes:\n"
        "\t* Only one gpio may be specified as a trigger, but rising, falling\n"
	"\t  high and low may be combined.\n"
	"\t* To run command with all defaults (no options), specify '--' in\n"
	"\t  place of any options.\n"
	"\t* If gtkwave fails to launch, examine file .gtkwave.out in the\n"
	"\t  current directory.\n",
        cmd,
        PAGES*4);
}

static bool
got_trigger(int trigger_gpio,int triggers,uint32_t *dblock,size_t samps) {
    uint32_t mask = 1 << trigger_gpio;
    uint32_t bits_a, bits_b;
    
    for ( unsigned ux = 0; ux < samps; ++ux ) {
        bits_a = dblock[ux];
        if ( triggers & TRIG_H && bits_a & mask )
            return true;    // Triggered on High
        if ( triggers & TRIG_L && !(bits_a & mask) )
            return true;    // Triggered on Low
        if ( ux == 0 )
            continue;

        bits_b = dblock[ux-1];
        if ( triggers & TRIG_R && !(bits_b & mask) && (bits_a & mask) )
            return true;    // Triggered on rising edge
        if ( triggers & TRIG_F && (bits_b & mask) && !(bits_a & mask) )
            return true;    // Triggered on falling edge
    }

    return false;           // No trigger found
}

int
main(int argc,char **argv) {
    static const char options[] = "b:R:F:H:L:T:xzvh";
    bool opt_errs = false, opt_x = false, opt_z = false;
    LogicAnalyzer logana(PAGES);
    int optch, trigger = 0, trigger_gpio = -1;
    int opt_T = 100;

    if ( argc <= 1 ) {
        usage(argv[0]);
        exit(0);
    }

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 'b':
            opt_blocks = atoi(optarg);
            break;
        case 'R':
            trigger |= TRIG_R;
            if ( !optarg || optarg[0] == '-' ) {
                fprintf(stderr,
                    "Invalid gpio: -%c %s\n",
                    optch,optarg);
                exit(2);
            }
            trigger_gpio = atoi(optarg);
            break;
        case 'F':
            trigger |= TRIG_F;
            if ( !optarg || optarg[0] == '-' ) {
                fprintf(stderr,
                    "Invalid gpio: -%c %s\n",
                    optch,optarg);
                exit(2);
            }
            trigger_gpio = atoi(optarg);
            break;
        case 'H':
            trigger |= TRIG_H;
            if ( !optarg || optarg[0] == '-' ) {
                fprintf(stderr,
                    "Invalid gpio: -%c %s\n",
                    optch,optarg);
                exit(2);
            }
            trigger_gpio = atoi(optarg);
            break;
        case 'L':
            trigger |= TRIG_L;
            if ( !optarg || optarg[0] == '-' ) {
                fprintf(stderr,
                    "Invalid gpio: -%c %s\n",
                    optch,optarg);
                exit(2);
            }
            trigger_gpio = atoi(optarg);
            break;
        case 'T':
            opt_T = atoi(optarg);
            break;
        case 'v':
            opt_verbose = true;
            break;
        case 'x':
            opt_x = true;
            break;
        case 'z':
            opt_z = true;
            break;
        case 'h':
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

    if ( opt_blocks <= 0 )
        ++opt_errs;

    if ( trigger && ( trigger_gpio < 0 || trigger_gpio > 31 ) ) {
        fprintf(stderr,"Trigger gpio must be a value from 0 through 31 (%d)\n",
            trigger_gpio);
        ++opt_errs;
    }

    if ( opt_errs ) {
        usage(argv[0]);
        exit(1);
    }

    unlink(".gtkwave.out");

    if ( !logana.open() ) {
        fprintf(stderr,"%s\n",logana.error());
        fprintf(stderr,"Make sure that the rpidma.ko module is loaded.\n");
        exit(1);
    }

    if ( !logana.alloc_blocks(opt_blocks) ) {
        fprintf(stderr,
		"Unable to allocate %d x %dk blocks\n",
		opt_blocks,
		PAGES * 4);
        exit(2);
    }

    if ( opt_verbose )
        printf("%d x %dk blocks allocated.\n",
		opt_blocks,
		PAGES * 4);

    //////////////////////////////////////////////////////////////////
    // Prepare for DMA
    //////////////////////////////////////////////////////////////////

    static const uint32_t GPIO_GPLEV0 = 0x7E200034;
    int int_count, tries = 0;
    int safety;

    while ( ++tries < opt_T ) {
        int_count = 0;

        {   // Ready the DMA control block
            DMA::CB& dma_cb = logana.get_cb();

            // Set up control block:
            dma_cb.clear();

            dma_cb.TI.NO_WIDE_BURSTS = 1;
            dma_cb.TI.WAITS = 0;
            dma_cb.TI.SRC_WIDTH = 0;            // 32-bits
            dma_cb.TI.SRC_INC = 0;
            dma_cb.TI.DEST_WIDTH = 0;           // 32-bits
            dma_cb.TI.DEST_INC = 1;
            dma_cb.TI.WAIT_RESP = 1;

            // Configure the transfer:
            dma_cb.TI.SRC_DREQ = 0;
            dma_cb.TI.DEST_DREQ = 0;       	        // See PERMAP 
            // dma_cb.TI.PERMAP = DMA::DREQ_2;
            dma_cb.SOURCE_AD = GPIO_GPLEV0;

            logana.propagate();

            if ( tries == 1 && opt_verbose ) {
                printf("GPLEV0 = 0x%08X\n",unsigned(dma_cb.SOURCE_AD));
                logana.dump_cb();
            }
        }

        // Start capture
        if ( !logana.start() ) {
            fprintf(stderr,"Unable to start DMA.\n");
            logana.close();
            exit(5);
        }

        if ( !trigger ) {
            if ( opt_verbose )
                puts("No triggers..");
            break;
        }

        // See if we can spot the trigger, by waiting
        // to capture one block:
        do  {
            int_count = logana.get_interrupts();
            if ( !int_count )
                usleep(10);
        } while ( !int_count );
            
        size_t samps;
        uint32_t *dblock = logana.get_samples(0,&samps);

        assert(samps > 0);

        if ( opt_verbose && tries == 1 )
            puts("Sampling for trigger(s)");

        if ( got_trigger(trigger_gpio,trigger,dblock,samps) ) {
            if ( opt_verbose )
                puts("Got trigger.");
            break;
	}

        // Restart DMA, and try again
        DMA::s_DMA_CS status;
        logana.abort(&status);
    }

    if ( tries >= opt_T ) {
        fprintf(stderr,"No trigger after %d tries.\n",tries);
        logana.close();
        exit(6);
    }

    // Wait for DMA completion
    safety = 500000;

    while ( --safety > 0 && !logana.end() ) {
        usleep(10);
        int_count = logana.get_interrupts();
    }

    if ( opt_verbose ) {
        int_count = logana.get_interrupts();

        printf("Interrupts: %u (%u blocks)\n",
            int_count,
            unsigned(logana.get_blocks()));
    }

    if ( safety <= 0 ) {
        printf("Timed out: aborted.\n");

        if ( opt_verbose ) {
            DMA::s_DMA_CS status;
            if ( logana.abort(&status) ) {
                printf("Terminated DMA status:\n");
                printf("  DMA.CS.ACTIVE :           %u\n",status.ACTIVE);
                printf("  DMA.CS.END :              %u\n",status.END);
                printf("  DMA.CS.INT :              %u\n",status.INT);
                printf("  DMA.CS.DREQ :             %u\n",status.DREQ);
                printf("  DMA.CS.PAUSED :           %u\n",status.PAUSED);
                printf("  DMA.CS.DREQ_STOPS_DMA :   %u\n",status.DREQ_STOPS_DMA);
                printf("  DMA.CS.WAITING :          %u\n",status.WAITING);
                printf("  DMA.CS.ERROR :            %u\n",status.ERROR);
                printf("  DMA.CS.PRIORITY :         %u\n",status.PRIORITY);
                printf("  DMA.CS.PANICPRI :         %u\n",status.PANICPRI);
                printf("  DMA.CS.WAIT_WRITES :      %u\n",status.WAIT_WRITES);
                printf("  DMA.CS.DISDEBUG :         %u\n",status.DISDEBUG);
            }
        }
        logana.close();
        exit(13);
    }

    VCD_Out vcdout;
    size_t samps;

    printf("Captured: writing capture.vcd\n");

    // Create VCD file:
    if ( !vcdout.open("captured.vcd",80.5,"ns","vcdout.cpp") ) {
        fprintf(stderr,"%s: writing %s\n",
            strerror(errno),
            vcdout.get_pathname());
        exit(14);
    }

    // Define GPIO signals:
    for ( int x=0; x<32; ++x ) {
        char name[32];

        snprintf(name,sizeof name,"gpio%d",x);
        vcdout.define_binary(x,name);
    }

    // Write out capture data:
    unsigned t;
    vcdout.set_time(t=0);

    for ( unsigned ux=0; ux < unsigned(opt_blocks); ++ux ) {
        uint32_t *dblock = logana.get_samples(ux,&samps);

        for ( unsigned uy=0; uy < samps; ++uy ) {
            uint32_t yblock = dblock[uy];

            for ( unsigned uz=0; uz < 32; ++uz )
                vcdout.set_value(uz,!!(yblock & (1<<uz)));
            vcdout.set_time(++t);
        }
    }

    vcdout.close();
    logana.close();

    // Run gtkwave, unless -x given, or DISPLAY not
    // defined
    if ( opt_x || !getenv("DISPLAY") )
        return 0;

    // Save access to stderr
    fflush(stdout);
    fflush(stderr);
    int was_stderr = dup(2);

    // Who are we really?
    uid_t ruid, euid, suid;
    gid_t rgid, egid, sgid;

    getresuid(&ruid,&euid,&suid);
    getresgid(&rgid,&egid,&sgid);

    // Fix ownership of capture file
    chown("captured.vcd",ruid,rgid);

    // Lose root privileges so gtkwave won't refuse
    setresuid(ruid,ruid,ruid);
    setresgid(rgid,rgid,rgid);

    if ( opt_verbose )
        puts("exec /usr/bin/gtkwave -f captured.vcd");

    // Direct stdout/stderr to /dev/null to eliminate
    // pesky Gtk messages.
    if ( !opt_z ) {
        int dev_null = open(".gtkwave.out",O_WRONLY|O_CREAT,0666);

	if ( dev_null == -1 )
        	dev_null = open("/dev/null",O_WRONLY);

        if ( dev_null ) {
            close(1);
            close(2);
            dup2(dev_null,1);
            dup2(dev_null,2);
            close(dev_null);
        }
    }

    // Execute /usr/bin/gtkwave
    execl("/usr/bin/gtkwave","-f","captured.vcd",0);

    // Exec failed: report error
    FILE *errout = fdopen(was_stderr,"w");
    fprintf(errout,
        "%s: exec(/usr/bin/gtkwave)\n",
        strerror(errno));

    return 0;
}

// End pispy.cpp
