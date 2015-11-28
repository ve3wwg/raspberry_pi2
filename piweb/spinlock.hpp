//////////////////////////////////////////////////////////////////////
// spinlock.hpp -- Spin Lock Class
// Date: Sat Jul  4 08:31:03 2015   (C) Warren Gay ve3wwg
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
