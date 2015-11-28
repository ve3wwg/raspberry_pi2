//////////////////////////////////////////////////////////////////////
// webmain.hpp -- Main Web Class
// Date: Mon Jul  6 21:49:42 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef WEBMAIN_HPP
#define WEBMAIN_HPP

#include "worker.hpp"

#include <vector>

class WebMain {
	int			backlog;	// Listen backlog
	std::string		address;	// Listen IPv4 Address
	int			port;		// Listen port
	int			lsock;		// Listen socket / -1
	int			threads;	// # of worker threads
	std::vector<Worker*>	workers;	// Ptrs to worker threads
	Worker::http_callback_t	callback;	// HTTP Callback

protected:
	int listen_socket(const char *addr,int port,int backlog);
public:

	WebMain();
	~WebMain();
	void set_address(const char *address);
	void set_port(int port);
	void set_backlog(int backlog);
	void set_threads(int threads);
	void set_callback(Worker::http_callback_t cb);
	int start();
	void join();
	void shutdown();
};

#endif // WEBMAIN_HPP

// End webmain.hpp
