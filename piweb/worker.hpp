//////////////////////////////////////////////////////////////////////
// worker.hpp -- Worker Thread Class for Pi Web Server
// Date: Sat Jul  4 21:18:00 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef WORKER_HPP
#define WORKER_HPP

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <evhttp.h>

#include <thread>

class Worker {
public:
	typedef void (*http_callback_t)(evhttp_request *req,const char *uri,const char *path,Worker& worker);

protected:

	int			thx;
	std::thread 		*thread;
	event_base 		*thread_base;
	evhttp 			*thread_http;
	event 			qev;
	int			nfd[2];
	evhttp_request		*req;
	evbuffer		*evb;
	http_callback_t		http_cb;
	bool			shutdownf;

	inline bool is_shutdown() { return shutdownf; }

public:
	Worker(int thx,int lsock,http_callback_t cb);
	~Worker();
	void start();				// Start thread

	void dispatch();			// Thread executes here
	void callback(evhttp_request *req);	// Internal

	void http_request(evhttp_request *req,const char *uri,const char *path);
	int  add_printf(const char *fmt,...);
	int  add(const char *data,size_t data_len);
	int  add(const char *sdata);
	void send_reply(unsigned code,const char *message);
	void send_error(unsigned code,const char *message);

	void request_shutdown();		// Initiate shutdown
	void join();				// Wait for shutdown

	inline int get_threadx()    { return thx; }
	inline void mark_shutdown() { shutdownf = true; }
};

#endif // WORKER_HPP

// End worker.hpp
