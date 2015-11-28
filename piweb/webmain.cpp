//////////////////////////////////////////////////////////////////////
// webmain.cpp -- WebMain Class Implementation
// Date: Mon Jul  6 22:00:29 2015  (C) Warren W. Gay VE3WWG 
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <assert.h>

#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
// #include <event2/keyvalq_struct.h>
#include <event2/listener.h>
#include <evhttp.h>

#include "webmain.hpp"

//////////////////////////////////////////////////////////////////////
// WebMain Constructor
//////////////////////////////////////////////////////////////////////

WebMain::WebMain() {
	backlog = 500;		// Default: -b 500
	address = "0.0.0.0";	// Default: -a 0.0.0.0
	port = 80;		// Default: -p 80
	lsock = -1;		// Initialize as no socket
	threads = 4;		// Assume 4 threads
	callback = 0;		// None by default
}

//////////////////////////////////////////////////////////////////////
// WebMain Destructor
//////////////////////////////////////////////////////////////////////

WebMain::~WebMain() {
	if ( lsock >= 0 ) {
		::close(lsock);
		lsock = -1;
	}
	for ( size_t x=0; x<workers.size(); ++x ) {
		if ( workers[x] ) {
			delete workers[x];
			workers[x] = 0;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Set the listen IPv4 address
//////////////////////////////////////////////////////////////////////

void
WebMain::set_address(const char *address) {
	this->address = address;
}

//////////////////////////////////////////////////////////////////////
// Set the listen port
//////////////////////////////////////////////////////////////////////

void
WebMain::set_port(int port) {
	this->port = port;
}

//////////////////////////////////////////////////////////////////////
// Set the listen backlog
//////////////////////////////////////////////////////////////////////

void
WebMain::set_backlog(int backlog) {
	this->backlog = backlog;
}

//////////////////////////////////////////////////////////////////////
// Set the number of worker threads
//////////////////////////////////////////////////////////////////////

void
WebMain::set_threads(int threads) {
	assert(workers.size() == 0);
	this->threads = threads <=0 ? 1 : threads;
}

//////////////////////////////////////////////////////////////////////
// Configure the HTTP request callback
//////////////////////////////////////////////////////////////////////

void
WebMain::set_callback(Worker::http_callback_t cb) {
	callback = cb;
}

//////////////////////////////////////////////////////////////////////
// Create a listening socket
//////////////////////////////////////////////////////////////////////

int
WebMain::listen_socket(const char *arg_address,int arg_port,int arg_backlog) {
	const int one = 1;
	struct sockaddr_in addr;
	int s;
	int rc, flags;

	s = socket(AF_INET,SOCK_STREAM,0);
	if ( s < 0 )
		return -errno;

	rc = setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char *)&one,sizeof one);
	if ( rc < 0 )
		return -errno;

	memset(&addr,0,sizeof addr);
	addr.sin_family = AF_INET;
 	addr.sin_addr.s_addr = address.size() > 0 ? inet_addr(arg_address) : INADDR_ANY;
	addr.sin_port = htons(arg_port);

	rc = bind(s,(struct sockaddr *)&addr,sizeof addr);
	if ( rc < 0 ) {
		rc = errno;
		::close(s);
		return -rc;
	}

	rc = ::listen(s,arg_backlog);
	if ( rc < 0 ) {
		rc = errno;
		::close(s);
		return -rc;
	}

	flags = fcntl(s,F_GETFL,0);
	rc = fcntl(s,F_SETFL,flags|O_NONBLOCK);
	if ( rc < 0 )
		return -errno;
	return s;
}

//////////////////////////////////////////////////////////////////////
// Start the worker threads
//////////////////////////////////////////////////////////////////////

int
WebMain::start() {

	int lsock = listen_socket(address.c_str(),port,backlog);
	if ( lsock < 0 )
		return lsock;	// Negative error code

	signal(SIGPIPE,SIG_IGN);

	for ( int thx=0; thx < threads; ++thx ) {
		workers.push_back(new Worker(thx,lsock,callback));
		workers[thx]->start();
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////
// Join with the worker threads
//////////////////////////////////////////////////////////////////////

void
WebMain::join() {

	for ( int thx=0; thx < threads; ++thx )
		workers[thx]->join();
}

//////////////////////////////////////////////////////////////////////
// Shutdown all worker threads
//////////////////////////////////////////////////////////////////////

void
WebMain::shutdown() {

	for ( size_t x=0; x<workers.size(); ++x ) {
		Worker& worker = *workers[x];

		worker.request_shutdown();
	}
}

// End webmain.cpp
