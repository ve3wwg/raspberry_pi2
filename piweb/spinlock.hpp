//////////////////////////////////////////////////////////////////////
// spinlock.hpp -- Spin Lock Class
// Date: Sat Jul  4 08:31:03 2015   (C) Warren Gay ve3wwg
//
// Exploring the Raspberry Pi 2 with C++ (ISBN 978-1-4842-1738-2)
// by Warren Gay VE3WWG
// LGPL2 V2.1
///////////////////////////////////////////////////////////////////////

#ifndef SPINLOCK_HPP
#define SPINLOCK_HPP

#include <atomic>

class SpinLock {
	std::atomic_flag atomic_mutex;

public:	SpinLock() : atomic_mutex(ATOMIC_FLAG_INIT) { }

	inline void lock() {
		while ( atomic_mutex.test_and_set() )
			;
	}

	inline void unlock() {
		atomic_mutex.clear();
	}
};

#endif // SPINLOCK_HPP

// End spinlock.hpp
