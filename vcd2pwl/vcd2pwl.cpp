///////////////////////////////////////////////////////////////////////
// vcd2pwl.cpp -- Convert VCD trace to LTspice PWL file
// Date: Wed May  6 21:01:25 2015  (C) Warren W. Gay VE3WWG 
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
#include <assert.h>

#include <string>
#include <unordered_map>

static void filter(const char *trace);

static double slew_rate = 471.3;	// V / us
static bool opt_verbose = false;
static double opt_Volts = 3.0;

static void
usage(const char *cmd) {
    const char *cp = strrchr(cmd,'/');
    
    if ( cp )
        cmd = cp+1; // Report basename of command

    fprintf(stderr,
	"Usage: %s -t trace_name [-h]\n"
        "where:\n"
	"\t-t trace_name\t\tName of the trace to convert.\n"
	"\t-s slewrate\t\tSlew rate to use (-s 471.3 V/us)\n"
	"\t           \t\tUnits are Volts / microsecond\n"
	"\t-V n\t\t\tMultiply logic 1 by n volts (-V3.0)\n"
	"\t-v\t\t\tVerbose\n"
        "\t-h\t\t\tThis info.\n\n"
        "\tThis filter converts one trace from a VCD file into\n"
        "\ta PWL file, for use by LTspice.\n",
        cmd);
}

int
main(int argc,char **argv) {
    static const char options[] = "t:s:V:vh";
    const char *opt_t = 0;
    bool opt_errs = false;
    int optch;

    if ( argc <= 1 ) {
        usage(argv[0]);
        exit(0);
    }

    while ( (optch = getopt(argc,argv,options)) != -1 ) {
        switch ( optch ) {
        case 't':
            opt_t = optarg;
            break;
        case 's':
            slew_rate = atof(optarg);
            if ( slew_rate <= 0.0 ) {
                fprintf(stderr,
                    "Invalid: -s %s\n",
                     optarg);
                exit(2);
            }
            break;
        case 'V':
            opt_Volts = atof(optarg);
            break;
        case 'v':
            opt_verbose = true;
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

    if ( !opt_t ) {
        fprintf(stderr,"No trace name given: Supply -t\n");
        opt_errs = true;
    }

    if ( opt_errs ) {
        usage(argv[0]);
        exit(2);
    }

    filter(opt_t);

    return 0;
}

static void
filter(const char *trace) {
    char buf[2048], one[64], sym[64], tname[64], units[64];
    int n, nw, symch = 0;
    double tscale = 80.5 / 10E9; // Default
    std::unordered_map<std::string,double> divs({
        {"s",1}, {"ms",10E3}, {"us",10E6},
        {"ns",10E9}, {"ps",10E12}, {"fs",10E15}});

    //  $timescale 80.5 ns $end
    //  $var wire 1 M gpio12 $end

    while ( fgets(buf,sizeof buf,stdin) ) {
        if ( sscanf(buf," %s",one) == 1 ) {
            if ( !strcmp(one,"$var") ) {
                n = sscanf(buf," %s %*s %d %s %s\n",
                    one,&nw,sym,tname);
                if ( n == 4 && !strcmp(one,"$var") && nw == 1 ) {
                    if ( !strcmp(trace,tname) )
                        symch = sym[0];
                }
            } else if ( !strcmp(one,"$timescale") ) {
                n = sscanf(buf," %s %lf %s\n",
                    one,&tscale,units);
                if ( n == 3 ) {
                    for ( auto it=divs.cbegin();
                      it != divs.cend(); ++it ) {
                        const std::string& u = it->first;
                        const double div = it->second;

                        if ( !strcasecmp(units,u.c_str()) )
                            tscale /= div;
                    }
                }
            }
            if ( one[0] != '$' )
                break;
        }
    }

    if ( feof(stdin) ) {
        fprintf(stderr,"No data/invalid format.\n");
        return;
    }

    double t = 0.0, v = 0.0;
    double dt = opt_Volts / slew_rate / 10E6; // In seconds

    if ( opt_verbose ) {
        fprintf(stderr,"Trace:      '%s' is wire %c\n",trace,symch);
        fprintf(stderr,"Time scale: %g seconds\n",tscale);
        fprintf(stderr,"Slew Rate:  %.3lf V/usec\n",slew_rate);
    }	
	
    do  {
        n = sscanf(buf," %s",one);
        if ( n < 1 )
            continue;
        if ( one[0] == '#' ) {
            double f;
            n = sscanf(buf," #%lg",&f);
            if ( n == 1 )
                t = f * tscale;
        } else  {
            double f;
            char label;

            n = sscanf(buf," %lg%c\n",&f,&label);
            if ( n == 2 && label == char(symch) ) {
                printf("%.12lf %g\n",t,v * opt_Volts);
                t += dt;
                v = f;
                printf("%.12lf %g\n",t,v * opt_Volts);
            }
        }
    } while ( fgets(buf,sizeof buf,stdin) );
}

// End vcd2pwl.cpp
