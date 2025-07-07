/*
 *  NuXThreadsPosix.cpp
 *  Copyright 2019 NuEdge Development. All rights reserved.
 */

// Posix threads do not include functions for atomic swaps so we need platform specific solutions there.

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include "NuXThreadsPosix.h"
#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#else
#error No implementation of atomic compare (yet)
#endif

#ifdef __APPLE__
#if (__MAC_OS_X_VERSION_MAX_ALLOWED < 1050)
#if (__LP64__)
inline bool OSAtomicCompareAndSwapPtrBarrier( void *oldValue, void *newValue, void * volatile *theValue )
{
	return OSAtomicCompareAndSwap64Barrier(reinterpret_cast<int64_t>(oldValue), reinterpret_cast<int64_t>(newValue)
			, (int64_t*)(theValue));
}
#else
inline bool OSAtomicCompareAndSwapPtrBarrier( void *oldValue, void *newValue, void * volatile *theValue )
{
	return OSAtomicCompareAndSwap32Barrier(reinterpret_cast<int32_t>(oldValue), reinterpret_cast<int32_t>(newValue)
			, (int32_t*)(theValue));
}
#endif
#endif
#endif

namespace NuXThreads {

static void throwPThreadException(const std::string& errorString, int error)
{
	char buffer[1024];
	sprintf(buffer, "%s [%d]", errorString.c_str(), error);
	throw Exception(buffer, error);
}

void ThreadMemoryFence() {
	__sync_synchronize();
}

/* --- Mutex --- */

Mutex::Impl::Impl() {
	::pthread_mutexattr_t attr;
	int err = ::pthread_mutexattr_init(&attr);
	if (err == 0) {
		err = ::pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		if (err == 0) {
			err = ::pthread_mutex_init(&mutex, &attr);
		}
	}
	if (err != 0) {
		throwPThreadException("Error initializing pthread mutex", err);
	}
}

Mutex::Impl::~Impl() {
	const int err = ::pthread_mutex_destroy(&mutex);
	(void)err;
	assert(err == 0);
}

::pthread_mutex_t& Mutex::Impl::getPThreadMutex() { return mutex; }

Mutex::Mutex() : impl(new Impl()) // Ok to allocate single initializer
{
}

void Mutex::lock() const throw()
{
	int err = ::pthread_mutex_lock(&impl->mutex);
	(void)err;
	assert(err == 0);
}

bool Mutex::tryToLock() const throw()
{
	int err  = ::pthread_mutex_trylock(&impl->mutex);
	assert(err == EBUSY || err == 0);
	return (err == 0);
}

void Mutex::unlock() const throw()
{
	int err = ::pthread_mutex_unlock(&impl->mutex);
	(void)err;
	assert(err == 0);
}

Mutex::~Mutex() { delete impl; }

/* --- Event --- */

Event::Impl::Impl() : signaled(false) {
	int err = ::pthread_cond_init(&condition, 0);
	if (err != 0) {
		throwPThreadException("Error initializing pthread condition", err);
	}
}

pthread_cond_t& Event::Impl::getPThreadCondition() { return condition; }

Event::Impl::~Impl() {
	int err = ::pthread_cond_destroy(&condition);
	(void)err;
	assert(err == 0);
}

Event::Event() : impl(new Impl()) // Ok to allocate single initializer
{
}

void Event::signal() {
    int err = ::pthread_mutex_lock(&impl->mutex);
    assert(err == 0);
    impl->signaled = true;
    err = ::pthread_cond_signal(&impl->condition);
    assert(err == 0);
    err = ::pthread_mutex_unlock(&impl->mutex);
    assert(err == 0);
}

void Event::reset() {
    int err = ::pthread_mutex_lock(&impl->mutex);
    assert(err == 0);
    impl->signaled = false;
    err = ::pthread_mutex_unlock(&impl->mutex);
    assert(err == 0);
}

bool Event::timedWait(int ms) {
	if (ms == 0) {
		int err = ::pthread_mutex_lock(&impl->mutex);
		assert(err == 0);
		bool ok = impl->signaled;
		impl->signaled = false;
		err = pthread_mutex_unlock(&impl->mutex);
		assert(err == 0);
		return ok;
	} else {
		timespec waitTS;
		waitTS.tv_sec = ms / 1000;
		waitTS.tv_nsec = (ms - waitTS.tv_sec * 1000) * 1000000;
		assert(waitTS.tv_nsec < 1000000000L);

		::timespec nowTS;
	#if 0 // older OS X doesn't have clock_gettime
		int err = ::clock_gettime(CLOCK_REALTIME, &nowTS);
		assert(err == 0);
		assert(nowTS.tv_nsec < 1000000000L);
	#else
		::timeval nowTV;
		int err = ::gettimeofday(&nowTV, 0);
		assert(err == 0);
		nowTS.tv_sec = nowTV.tv_sec;
		nowTS.tv_nsec = nowTV.tv_usec * 1000;
	#endif

		timespec absTS;
		absTS.tv_sec = nowTS.tv_sec + waitTS.tv_sec;
		absTS.tv_nsec = nowTS.tv_nsec + waitTS.tv_nsec;
		if (absTS.tv_nsec >= 1000000000L) {		/* Carry? */
			++absTS.tv_sec;
			absTS.tv_nsec -= 1000000000L;
		}
		assert(absTS.tv_nsec < 1000000000L);

		err = ::pthread_mutex_lock(&impl->mutex);
		assert(err == 0);

		bool ok = true;
		while (!impl->signaled && err == 0) {
			err = ::pthread_cond_timedwait(&impl->condition, &impl->mutex, &absTS);
			assert(err == 0 || err == ETIMEDOUT);
			ok = (err == 0);
		}
		if (ok) {
			impl->signaled = false;
		}
		err = pthread_mutex_unlock(&impl->mutex);
		assert(err == 0);
		return ok;
	}
}

void Event::wait() {
     int err = ::pthread_mutex_lock(&impl->mutex);
     assert(err == 0);
     while (!impl->signaled && err == 0) {
     	err = ::pthread_cond_wait(&impl->condition, &impl->mutex);
     	assert(err == 0);
	 }
	 impl->signaled = false;
     err = pthread_mutex_unlock(&impl->mutex);
     assert(err == 0);
}

Event::~Event() {
	delete impl;
}

/* --- AtomicInt --- */

int AtomicInt::assign(volatile int* x, int y)
{
	::OSAtomicCompareAndSwap32Barrier(*x, y, const_cast<int32_t*>(x));
	return y;
}

int AtomicInt::increment(volatile int* x)
{
	return ::OSAtomicIncrement32Barrier(const_cast<int32_t*>(x));
}		

int AtomicInt::decrement(volatile int* x)
{
	return ::OSAtomicDecrement32Barrier(const_cast<int32_t*>(x));
}

int AtomicInt::add(volatile int* x, int y)
{
	return ::OSAtomicAdd32Barrier(y, const_cast<int32_t*>(x));
}

int AtomicInt::swap(volatile int* x, int y)
{
	// We must retry the swap until success or we don't know what value to return. We cannot simply return z if the swap
	// goes through and (*x) if not, since the sequence of returned values between concurrent threads / CPU may be important,
	// i.e. the previous value (z) must be returned by this thread / CPU only and the next time, the new value (y) must be returned.
	// Consider the following code: { x = new ...; delete p.swap(x); }
	volatile int z;
	do {
		z = *x;
	} while (!OSAtomicCompareAndSwap32Barrier(z, y, const_cast<int32_t*>(x)));
	return z;
}

bool AtomicInt::swapIfEqual(volatile int* x, int equalTo, int y)
{
	return ::OSAtomicCompareAndSwap32Barrier(equalTo, y, const_cast<int32_t*>(x));
}

/* --- AtomicFloat --- */

float AtomicFloat::assign(volatile float* x, float y)
{
	union u { uint32_t i; float f; };
	u a, b;
	a.f = *x;
	b.f = y;
	::OSAtomicCompareAndSwap32Barrier(a.i, b.i, (int32_t*)(x));
	return y;
}

float AtomicFloat::swap(volatile float* x, float y)
{
	union u { int i; float f; };
	u z;
	z.f = y;
	z.i = AtomicInt::swap(reinterpret_cast<volatile int*>(x), z.i);
	return z.f;
}

bool AtomicFloat::swapIfEqual(volatile float* x, float equalTo, float y)
{
	union u { uint32_t i; float f; };
	u a, b;
	a.f = equalTo;
	b.f = y;
	return ::OSAtomicCompareAndSwap32Barrier(a.i, b.i, (int32_t*)(x));
}

/* --- AtomicPointerBaseClass --- */

const void* AtomicPointerBaseClass::assign(const void* volatile* p, const void* q)
{
	::OSAtomicCompareAndSwapPtrBarrier(const_cast<void*>(*p), const_cast<void*>(q), const_cast<void**>(p));
	return q;
}

const void* AtomicPointerBaseClass::swap(const void* volatile* p, const void* q)
{
	// We must retry the swap until success or we don't know what value to return. We cannot simply return z if the swap
	// goes through and (*x) if not, since the sequence of returned values between concurrent threads / CPU may be important,
	// i.e. the previous value (z) must be returned by this thread / CPU only and the next time, the new value (y) must be returned.
	// Consider the following code: { x = new ...; delete p.swap(x); }
	volatile const void* r;
	do {
		r = *p;
	} while (!::OSAtomicCompareAndSwapPtrBarrier(const_cast<void*>(r), const_cast<void*>(q), const_cast<void**>(p)));
	return const_cast<const void*>(r);
}

bool AtomicPointerBaseClass::swapIfEqual(const void* volatile* p, const void* equalTo, const void* q)
{
	return ::OSAtomicCompareAndSwapPtrBarrier(const_cast<void*>(equalTo), const_cast<void*>(q), const_cast<void**>(p));
}

/* --- Thread --- */

void* Thread::Impl::startRoutine(void* arg) {
	Thread::Impl* impl = reinterpret_cast<Thread::Impl*>(arg);
	try {
/* FIX : drop, see cancel
		int oldType;
		::pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType);
*/
		impl->startEvent.wait();
		assert(impl->stage != SUSPENDED);
		if (impl->stage == RUNNING) {
			impl->runner->run();
		}
		impl->stage = STOPPED;
		impl->stoppedEvent.signal();
	}
	catch (...) {
		assert(0);
	}
	if (--impl->keepCounter == 0) {
		delete impl;
	}
	return 0;
}

Thread::Impl::Impl(Runnable* runner) : runner(runner), stage(SUSPENDED), keepCounter(1) {
	int err = ::pthread_create(&thread, 0, startRoutine, this);
	if (err != 0) {
		throwPThreadException("Error creating thread", err);
	}
	++keepCounter;
}

::pthread_t Thread::Impl::getPThreadId() const { return thread; }

Thread::Impl::~Impl()
{
}

int Thread::readMsTimer()
{
	::timeval nowTV;
	int err = ::gettimeofday(&nowTV, 0);
	(void)err;
	assert(err == 0);
	return wrapToInt32(static_cast<unsigned int>(nowTV.tv_sec * 1000) + nowTV.tv_usec / 1000);
}

void Thread::sleep(int ms) { ::usleep(1000 * ms); }

void Thread::yield() { ::pthread_yield_np(); }

ThreadId Thread::getCurrentId() { return reinterpret_cast<ThreadId>(::pthread_self()); }

Thread::Thread() : impl(new Impl(this)) { }

Thread::Thread(Runnable& runner) : impl(new Impl(&runner)) // Ok to allocate single initializer
{
}

/* FIX : stupid range? */

void Thread::setPriority(int priority) {
	::sched_param param;
	int policy;

	int err = ::pthread_getschedparam(impl->thread, &policy, &param);
	assert(err == 0);

	const int maxPrio = sched_get_priority_max(policy);
	const int minPrio = sched_get_priority_min(policy);
	
	param.sched_priority = std::min(std::max(minPrio + (maxPrio - minPrio) * (priority + 10) / 20, minPrio), maxPrio);
	err = ::pthread_setschedparam(impl->thread, policy, &param);
	assert(err == 0);
}

void Thread::start() {
	if (impl->stage == Impl::SUSPENDED) {
		impl->stage = Impl::RUNNING;
		impl->startEvent.signal();
	}
}

void Thread::join() const {
	if (impl->stage != Impl::JOINED) {
		int err = ::pthread_join(impl->thread, 0);
		(void)err;
		assert(err == 0);
		assert(impl->stage == Impl::STOPPED);
		impl->stage = Impl::JOINED;
	}
}

bool Thread::timedJoin(int ms) const {
	if (impl->stage == Impl::JOINED) {
		return true;
	} else if (!impl->stoppedEvent.timedWait(ms)) {
		return false;
	} else {
		assert(impl->stage == Impl::STOPPED);
		int err = ::pthread_join(impl->thread, 0);
		(void)err;
		assert(err == 0);
		impl->stage = Impl::JOINED;
		return true;
	}
}

ThreadId Thread::getId() const { return reinterpret_cast<ThreadId>(impl->thread); }
bool Thread::isRunning() const { return (impl->stage == Impl::RUNNING); }

Thread::~Thread()
{
	if (impl->stage == Impl::SUSPENDED) {
		impl->stage = Impl::STOPPED;
		impl->startEvent.signal();
	}
	if (impl->stage == Impl::RUNNING) {
		int err = ::pthread_detach(impl->thread);
		(void)err;
		assert(err == 0);
	} else if (impl->stage != Impl::JOINED) {
		int err = ::pthread_join(impl->thread, 0);
		(void)err;
		assert(err == 0);
	}
	if (--impl->keepCounter == 0) {
		delete impl;
	}
}

} /* namespace NuX */
