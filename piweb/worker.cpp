//////////////////////////////////////////////////////////////////////
// worker.cpp -- Worker Thread Class Implementation
// Date: Sat Jul  4 21:18:58 2015  (C) Warren W. Gay VE3WWG 
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

#include <sys/types.h>
#include <fcntl.h>

#include "worker.hpp"
#include "common.hpp"

#include <string>

static void notify_cb(int fd,short what,void *arg);
static void generic_cb(struct evhttp_request *req, void *arg);
static void thread_main(Worker *worker);

//////////////////////////////////////////////////////////////////////
// Worker Constructor
//////////////////////////////////////////////////////////////////////

Worker::Worker(int thx,int lsock,http_callback_t cb) {
	int rc;

	http_cb = cb;				// HTTP request callback

#ifdef __linux__
	rc = pipe2(nfd,O_NONBLOCK);
	assert(!rc);
#else
	rc = pipe(nfd);
	assert(!rc);
	{
		int flags = fcntl(nfd[0],F_GETFL,0);
		assert(flags != -1);
		rc = fcntl(nfd[0],flags|O_NONBLOCK);
		assert(!rc);
	}
#endif

	this->thx = thx;			// Thread index
	thread = 0;				// No thread yet
	thread_base = event_base_new();		// Event base
	thread_http = evhttp_new(thread_base);	// HTTP server
	shutdownf = false;			// No shutdown yet
	req = 0;
	evb = 0;

	// Let's be notified when a byte is sent to the pipe
	event_assign(&qev,thread_base,nfd[0],EV_READ|EV_PERSIST,notify_cb,this);
	event_add(&qev,0);

	// Invoke generic_cb when we receive an HTTP request
	evhttp_set_gencb(thread_http,generic_cb,this);

	// Listen for requests on lsock
	evhttp_accept_socket(thread_http,lsock);
};

//////////////////////////////////////////////////////////////////////
// Worker Destructor
//////////////////////////////////////////////////////////////////////

Worker::~Worker() {
	if ( thread ) {
		delete thread;
		thread = 0;
	}

	if ( evb ) {
		evbuffer_free(evb);
		evb = 0;
	}
}

//////////////////////////////////////////////////////////////////////
// Start the worker thread
//////////////////////////////////////////////////////////////////////

void
Worker::start() {

	assert(!thread);	// Allow only one call here
	thread = new std::thread(thread_main,this);
};

//////////////////////////////////////////////////////////////////////
// Dispatch on events for this thread
//////////////////////////////////////////////////////////////////////

void
Worker::dispatch() {

	while ( !shutdownf ) 
		event_base_loop(thread_base,EVLOOP_ONCE);
}

//////////////////////////////////////////////////////////////////////
// Request a thread shutdown: This is done by sending one byte ('X')
// through the pipe (nfd[0,1]). This causes an event that will cause
// the Worker::dispatch() to exit event_base_loop() eventually, so
// that it can see that shutdownf flag is set.
//////////////////////////////////////////////////////////////////////

void
Worker::request_shutdown() {

	while ( write(nfd[1],"X",1) == -1 && errno == EINTR )
		;
}

//////////////////////////////////////////////////////////////////////
// Caller is blocked until the Worker thread has exited.
//////////////////////////////////////////////////////////////////////

void
Worker::join() {

	thread->join();
}

//////////////////////////////////////////////////////////////////////
// Add formatted data to the request response
//////////////////////////////////////////////////////////////////////

int
Worker::add_printf(const char *fmt,...) {
	va_list ap;
	int n;

	va_start(ap,fmt);
	n = evbuffer_add_vprintf(evb,fmt,ap);
	va_end(ap);
	return n;
}

//////////////////////////////////////////////////////////////////////
// Add preformatted data to the request response
//////////////////////////////////////////////////////////////////////

int
Worker::add(const char *data,size_t data_len) {
	return evbuffer_add(evb,data,data_len);
}

int
Worker::add(const char *sdata) {
	return evbuffer_add(evb,sdata,strlen(sdata));
}

//////////////////////////////////////////////////////////////////////
// Send a reply with buffered data
//////////////////////////////////////////////////////////////////////

void
Worker::send_reply(unsigned code,const char *message) {
	evhttp_send_reply(req,code,message,evb);
}

//////////////////////////////////////////////////////////////////////
// Send an error reply with buffered data
//////////////////////////////////////////////////////////////////////

void
Worker::send_error(unsigned code,const char *message) {
	evhttp_send_error(req,code,message);
}

//////////////////////////////////////////////////////////////////////
// Internal callback
//////////////////////////////////////////////////////////////////////

void
Worker::callback(evhttp_request *req) {
	const char *uri = evhttp_request_get_uri(req);
	evhttp_uri *decoded = 0;
	const char *path;
	char *decoded_path = 0;

	this->req = req;			// For send_reply() calls
	evb = evbuffer_new();

	// Decode the URI
	decoded = evhttp_uri_parse(uri);
	if ( !decoded ) {
		// Bad URI : reject request
		evhttp_send_error(req,HTTP_BADREQUEST,0);
		evhttp_uri_free(decoded);
		return;
	}

	// Extract raw path
	path = evhttp_uri_get_path(decoded);
	if ( !path )
		path = "/";

	// Unescape path etc.
	decoded_path = evhttp_uridecode(path,0,0);

	//////////////////////////////////////////////////////////////
	// Issue request callback
	//////////////////////////////////////////////////////////////

	if ( decoded_path ) {
		if ( http_cb )
			http_cb(req,uri,decoded_path,*this);
	} else	{
		// Failed request
		evhttp_send_error(req,404,"Failed request\r\n");
	}

	if ( decoded )
		evhttp_uri_free(decoded);
	if ( decoded_path )
		free(decoded_path);
	if ( evb ) {
		evbuffer_free(evb);
		evb = 0;
	}
	this->req = 0;
}

//////////////////////////////////////////////////////////////////////
// This callback is invoked after Worker::shutdown() has written a
// byte to the pipe.
//////////////////////////////////////////////////////////////////////

static void
notify_cb(int fd,short what,void *arg) {
	Worker& worker = *(Worker *)arg;	// Access Worker class
	char bytes[32];

	assert(what & EV_READ);			// Only expecting read notifies

	::read(fd,bytes,sizeof bytes);		// Siphon off any bytes written
	worker.mark_shutdown();			// Now set shutdownf = true
}

//////////////////////////////////////////////////////////////////////
// This callback is invoked by libevent:
// 1) Access the Worker class object
// 2) Invoke the Worker callback() method
//////////////////////////////////////////////////////////////////////

static void
generic_cb(struct evhttp_request *req,void *arg) {
	Worker& worker = *(Worker *)arg;

	worker.callback(req);
}

//////////////////////////////////////////////////////////////////////
// Thread execution starts here. Transfer execution to C++ method
// Worker::dispatch() for duration of thread.
//////////////////////////////////////////////////////////////////////

static void
thread_main(Worker *worker) {
	worker->dispatch();
}

// End worker.cpp
