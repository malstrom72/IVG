/*
 *  NuXThreadsCarbon.cpp
 *  NuXTest
 *
 *  Created by Magnus Lidström on 1/9/06.
 *  Copyright 2006 NuEdge Development. All rights reserved.
 *
 */

#include "NuXThreadsCarbon.h"
#include <libkern/OSAtomic.h>

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

namespace NuXThreads {

static void throwCarbonException(const std::string& errorString, int error)
{
	char buffer[1024];
	if (error != 0) {
		sprintf(buffer, "%s [%d]", errorString.c_str(), error);
	} else {
		sprintf(buffer, "%s", errorString.c_str());
	}
	throw Exception(buffer, error);
}

/* --- Mutex --- */

Mutex::Impl::Impl()
	: criticalRegionId(0)
{
}

::MPCriticalRegionID Mutex::Impl::getCarbonCriticalRegionId() const
{
	return criticalRegionId;
}

Mutex::Mutex()
	: impl(new Impl()) // Ok to allocate single initializer
{
	try {
		::OSStatus mpCreateCriticalRegionReturn = ::MPCreateCriticalRegion(&impl->criticalRegionId);
		if (mpCreateCriticalRegionReturn != noErr) {
			throwCarbonException("Error creating critical region", mpCreateCriticalRegionReturn);
		}
	}
	catch (...) {
		delete impl;
		throw;
	}
}

void Mutex::lock() const throw()
{
	::OSStatus mpEnterCriticalRegionReturn = ::MPEnterCriticalRegion(impl->criticalRegionId, kDurationForever);
	(void)mpEnterCriticalRegionReturn;
	assert(mpEnterCriticalRegionReturn == noErr);
}

bool Mutex::tryToLock() const throw()
{
	::OSStatus mpEnterCriticalRegionReturn = ::MPEnterCriticalRegion(impl->criticalRegionId, kDurationImmediate);
	assert(mpEnterCriticalRegionReturn == noErr || mpEnterCriticalRegionReturn == kMPTimeoutErr);
	return (mpEnterCriticalRegionReturn == noErr);
}

void Mutex::unlock() const throw()
{
	::OSStatus mpExitCriticalRegion = ::MPExitCriticalRegion(impl->criticalRegionId);
	(void)mpExitCriticalRegion;
	assert(mpExitCriticalRegion == noErr);
}

Mutex::~Mutex()
{
	::OSStatus mpDeleteCriticalRegionReturn = ::MPDeleteCriticalRegion(impl->criticalRegionId);
	(void)mpDeleteCriticalRegionReturn;
	assert(mpDeleteCriticalRegionReturn == noErr);
	delete impl;
}

/* --- Event --- */

Event::Impl::Impl()
	: eventId(0)
{
}

::MPEventID Event::Impl::getCarbonEventId() const
{
	return eventId;
}

Event::Event()
	: impl(new Impl()) // Ok to allocate single initializer
{
	try {
		::OSStatus mpCreateEventReturn = ::MPCreateEvent(&impl->eventId);
		if (mpCreateEventReturn != noErr) {
			throwCarbonException("Error creating waitable event", mpCreateEventReturn);
		}
	}
	catch (...) {
		delete impl;
		throw;
	}
}

void Event::signal()
{
	::OSStatus mpSetEventReturn = ::MPSetEvent(impl->eventId, 1);
	if (mpSetEventReturn != noErr) {
		throwCarbonException("Error signaling waitable event", mpSetEventReturn);
	}
}

void Event::reset()
{
	::MPEventFlags flags;
	::OSStatus mpWaitForEventReturn = ::MPWaitForEvent(impl->eventId, &flags, kDurationImmediate);
	if (mpWaitForEventReturn != noErr && mpWaitForEventReturn != kMPTimeoutErr) {
		throwCarbonException("Error resetting waitable event", mpWaitForEventReturn);
	}
}

bool Event::timedWait(int ms)
{
	::MPEventFlags flags;
	::OSStatus mpWaitForEventReturn = ::MPWaitForEvent(impl->eventId, &flags, ms);
	if (mpWaitForEventReturn != noErr && mpWaitForEventReturn != kMPTimeoutErr) {
		throwCarbonException("Error waiting on an event", mpWaitForEventReturn);
	}
	return (mpWaitForEventReturn == noErr);
}

void Event::wait()
{
	timedWait(kDurationForever);
}

Event::~Event()
{
	::OSStatus mpDeleteEventReturn = ::MPDeleteEvent(impl->eventId);
	(void)mpDeleteEventReturn;
	assert(mpDeleteEventReturn == noErr);
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
	union u { ::UInt32 i; float f; };
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
	union u { ::UInt32 i; float f; };
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

::OSStatus Thread::Impl::carbonTaskEntryPoint(void* parameter)
{
	Thread::Impl* impl = reinterpret_cast<Thread::Impl*>(parameter);
	try {
		impl->startEvent.wait();
		if (impl->started) {
			impl->runner->run();
		}
	}
	catch (...) {
		assert(0);
	}
	if (--impl->keepCounter == 0) {
		delete impl;
	}
	return noErr;
}

Thread::Impl::Impl(Runnable* runner)
	: keepCounter(1)
	, runner(runner)
	, taskId(0)
	, queueId(0)
	, started(false)
	, killed(false)
{
	try {
		::OSStatus mpCreateQueueReturn = ::MPCreateQueue(&queueId);
		if (mpCreateQueueReturn != noErr) {
			throwCarbonException("Error creating thread message queue", mpCreateQueueReturn);
		}
		::OSStatus mpCreateTaskReturn = ::MPCreateTask
				( carbonTaskEntryPoint
				, this
				, 512 * 1024
				, queueId
				, reinterpret_cast<void*>('thrd')
				, reinterpret_cast<void*>('term')
				, 0
				, &taskId
				);
		if (mpCreateTaskReturn != noErr) {
			throwCarbonException("Error creating thread", mpCreateTaskReturn);
		}
		++keepCounter;
	}
	catch (...) {
		if (queueId != 0) {
			::OSStatus mpDeleteQueueReturn = ::MPDeleteQueue(queueId);
			(void)mpDeleteQueueReturn;
			assert(mpDeleteQueueReturn == noErr);
		}
		throw;
	}
}

::MPTaskID Thread::Impl::getCarbonTaskId() const
{
	return taskId;
}

Thread::Impl::~Impl()
{
	assert(queueId != 0);
	::OSStatus mpDeleteQueueReturn = ::MPDeleteQueue(queueId);
	(void)mpDeleteQueueReturn;
	assert(mpDeleteQueueReturn == noErr);
}

int Thread::readMsTimer()
{
	return wrapToInt32(static_cast<unsigned long long>(::GetCurrentEventTime() * 1000.0 + 0.5) & 0xFFFFFFFF);
}

void Thread::sleep(int ms)
{
	// FIX : better solution?
	::AbsoluteTime expireTime = ::AddDurationToAbsolute(durationMillisecond * ms, ::UpTime());
	::MPDelayUntil(&expireTime);
}

void Thread::yield()
{
	::MPYield(); // FIX : does it do any good?
}

ThreadId Thread::getCurrentId()
{
	return reinterpret_cast<ThreadId>(::MPCurrentTaskID());
}

Thread::Thread()
	: impl(new Impl(this))
{
}

Thread::Thread(Runnable& runner)
	: impl(new Impl(&runner)) // Ok to allocate single initializer
{
}

/* FIX : stupid range? */

void Thread::setPriority(int priority)
{
	assert(priority >= -10 && priority <= 10);
	const ::MPTaskWeight prioWeights[21] = { 1, 2, 3, 5, 7, 10, 15, 25, 50, 75, 100, 150, 250, 500, 750, 1000, 1500, 2500, 5000, 7500, 10000 };
	::OSStatus mpSetTaskWeightReturn = ::MPSetTaskWeight(impl->taskId, prioWeights[priority + 10]);
	if (mpSetTaskWeightReturn != noErr) {
		throwCarbonException("Error setting thread priority", mpSetTaskWeightReturn);
	}
}

void Thread::start()
{
	impl->started = true;
	impl->startEvent.signal();
}

bool Thread::timedJoin(int ms) const
{
	assert(impl->started);
	void* param1;
	void* param2;
	void* param3;
	::OSStatus mpWaitOnQueueReturn = ::MPWaitOnQueue(impl->queueId, &param1, &param2, &param3, ms);
	if (mpWaitOnQueueReturn == noErr) {
		assert(reinterpret_cast<size_t>(param1) == 'thrd');
		assert(reinterpret_cast<size_t>(param2) == 'term');
		// Bounce back the termination so that this or any other thread receives it next time.
		::OSStatus mpNotifyQueueReturn = ::MPNotifyQueue(impl->queueId, param1, param2, param3);
		(void)mpNotifyQueueReturn;
		assert(mpNotifyQueueReturn == noErr);
		return true;
	} else if (mpWaitOnQueueReturn != kMPTimeoutErr) {
		throwCarbonException("Error waiting on thread to finish", mpWaitOnQueueReturn);
	}
	return false;
}

void Thread::join() const
{
	assert(impl->started);
	timedJoin(kDurationForever);
}

ThreadId Thread::getId() const
{
	return reinterpret_cast<ThreadId>(impl->taskId);
}

bool Thread::isRunning() const
{
	return (impl->started && !timedJoin(kDurationImmediate));
}

Thread::~Thread()
{
	if (!impl->started) {
		impl->startEvent.signal();
		impl->killed = true;
	}
	if (impl->killed) {
		void* param1;
		void* param2;
		void* param3;
		::OSStatus mpWaitOnQueueReturn = ::MPWaitOnQueue(impl->queueId, &param1, &param2, &param3, kDurationForever);
		(void)mpWaitOnQueueReturn;
		assert(mpWaitOnQueueReturn == noErr);
		assert(reinterpret_cast<size_t>(param1) == 'thrd');
		assert(reinterpret_cast<size_t>(param2) == 'term');
		delete impl;
	} else if (--impl->keepCounter == 0) {
		delete impl;
	}
}

} /* namespace NuX */
