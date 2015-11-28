//////////////////////////////////////////////////////////////////////
// piweb.cpp -- Pi libevent Web Server
// Date: Sat Jul  4 07:56:19 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gpio.hpp"
#include "webmain.hpp"

static WebMain webmain;
static GPIO gpio;

static void
http_callback(evhttp_request *req,const char *uri,const char *path,Worker& worker) {
	bool do_shutdown = false;

	printf("Got uri='%s', path='%s', thread # %d\n",
		uri,path,worker.get_threadx());

	if ( !strcmp(path,"/cpuinfo") ) {
		worker.add("<html>\r\n<head>\r\n<title>cpuinfo</title></head>\r\n");
		worker.add("<body><pre>\r\n");
		FILE *f = fopen("/proc/cpuinfo","r");
		if ( f ) {
			char inbuf[1024];

			while ( fgets(inbuf,sizeof inbuf,f) != nullptr ) {
				char *cp = strrchr(inbuf,'\n');
				if ( cp != 0 && size_t(cp - inbuf) < sizeof inbuf - 3 )
					strcpy(cp,"\r\n");
				worker.add(inbuf,strlen(inbuf));
			}

			fclose(f);
		}
		worker.add("</pre></body>\r\n");
	} else if ( !strcmp(path,"/gpio") ) {
		worker.add("<html>\r\n<head>\r\n<title>cpuinfo</title></head>\r\n");
		worker.add("<body><table>\r\n");
		worker.add("<tr><td>GPIO</td><td>ALTFUN</td><td>LEV</td>"
			"<td>SLEW</td><td>HYST</td><td>DRIVE</td>"
			"<td>DESCRIPTION</td></tr>\r\n");

		if ( gpio.get_error() != 0 ) {
			worker.add_printf("%s: obtaining gpio info\r\n",
				strerror(gpio.get_error()));
		} else	{
			for (int gpno=0; gpno < 32; ++gpno ) {
				GPIO::IO io;
	                        bool slew_rate_limited, hysteresis;
	                        int drive, mA;

				gpio.alt_function(gpno,io);
	                        gpio.get_drive_strength(
	                        	gpno,
	                                slew_rate_limited,
	                                hysteresis,
	                                drive);

			        mA = 2 + drive * 2;

			        worker.add_printf(
					"<tr>"
					"<td>%2d</td>"
					"<td>%s</td>"
					"<td>%d</td>"
					"<td>%c</td>"
					"<td>%c</td>"
					"<td>%2d mA</td>"
					"<td>%s</td></tr>\r\n",
			            	gpno,
			            	GPIO::alt_name(io),
			            	gpio.read(gpno),
			            	slew_rate_limited ? 'Y' : 'N',
			            	hysteresis ? 'Y' : 'N',
			            	mA,
			            	GPIO::gpio_alt_func(gpno,io));
			}
			worker.add("</table></body>\r\n");
		}

	} else if ( !strcmp(path,"/shutdown") ) {
		do_shutdown = true;
		worker.add_printf(
			"<html>\n <head>\n"
			"  <title>%s</title>\n"
			"  <base href='.../%s'>\n"
			" </head>\n"
			" <body>\n"
			"  <h1>%s : thread %d, shutting down</h1>\n"
			"  <ul>\n",
			path,
			path,
			path,
			worker.get_threadx());
	} else	{
		// This holds the content we're sending.
		worker.add_printf(
			"<html><head><title>%s</title></head>\r\n"
			"<body><h1>Response</h1>\r\n"
			"<ul><li>Path: %s</li>"
			"<li>URI: %s</li>"
			"<li>Thread: %d</li>"
			"</ul></body>\r\n",
			path,	// title
			path,	// Path
			uri,	// URI
			worker.get_threadx()); // Thread
		evhttp_add_header(req->output_headers,"Connection","Close");
	}
	worker.send_reply(200,"OK");

	if ( do_shutdown )
		webmain.shutdown();
}

static void
usage(const char *cmd) {
	const char *cp = strrchr(cmd,'/');

	if ( cp )
		cmd = cp + 1;
		
	printf(	"Usage: %s [-options]\n"
		"where:\n"
		"\t-a address\tListening address (0.0.0.0)\n"
		"\t-p port\t\tListening port (80)\n"
		"\t-b backlog\tBacklog to use for listening ports\n"
		"\t-t threads\tNumber of threaded servers to use\n"
		"\t-v\t\tVerbose messages\n",
		cmd);
}

int
main(int argc, char **argv) {
	bool opt_errs = false;
	int optch, rc;

	while ( (optch = getopt(argc,argv,"a:p:b:t:h")) != -1) {
		switch ( optch ) {
		case 'a':
			webmain.set_address(optarg);
			break;				
		case 'p':
			webmain.set_port(atoi(optarg));
			break;
		case 'b':
			webmain.set_backlog(atoi(optarg));
			break;
		case 't':
			webmain.set_threads(atoi(optarg));
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

	

	if ( opt_errs ) {
		usage(argv[0]);
		exit(1);
	}

	webmain.set_callback(http_callback);
	rc = webmain.start();
	if ( rc != 0 ) {
		fprintf(stderr,"%s: Starting webmain\n",
			strerror(-rc));
		exit(2);
	}

	webmain.join();
	return 0;
}

// End piweb.cpp

