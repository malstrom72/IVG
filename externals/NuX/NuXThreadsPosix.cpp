/*
BSD 2-Clause License

Copyright (c) 2005-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Posix threads do not include functions for atomic swaps so we need platform specific solutions there.

#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "NuXThreadsPosix.h"
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

namespace NuXThreads {

static void throwPThreadException(const std::string& errorString, int error)
{
	char buffer[1024];
	sprintf(buffer, "%s [%d]", errorString.c_str(), error);
	throw Exception(buffer, error);
}

void threadMemoryFence() {
	__sync_synchronize();
}

/*
	Time-outs and readMsTimer() run on a monotonic clock, so a change of the wall clock (NTP, a VM resume, the user
	setting the time) neither stretches nor shortens them. Nanoseconds from an arbitrary start.
*/
static uint64_t readMonotonicNanoseconds() {
#ifdef __APPLE__
	::mach_timebase_info_data_t timebase;
	::mach_timebase_info(&timebase);
	const uint64_t ticks = ::mach_absolute_time();
	// Split so that multiplying by `numer` cannot overflow.
	return ticks / timebase.denom * timebase.numer + ticks % timebase.denom * timebase.numer / timebase.denom;
#else
	::timespec now;
	int err = ::clock_gettime(CLOCK_MONOTONIC, &now);
	(void)err;
	assert(err == 0);
	return static_cast<uint64_t>(now.tv_sec) * 1000000000U + static_cast<uint64_t>(now.tv_nsec);
#endif
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
		::pthread_mutexattr_destroy(&attr); // Doesn't affect the mutex created from it.
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
#ifdef __APPLE__
	int err = ::pthread_cond_init(&condition, 0);
#else
	// Its time-outs are deadlines on the monotonic clock (see timedWait()).
	::pthread_condattr_t attributes;
	int err = ::pthread_condattr_init(&attributes);
	if (err == 0) {
		err = ::pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
		if (err == 0) {
			err = ::pthread_cond_init(&condition, &attributes);
		}
		::pthread_condattr_destroy(&attributes);
	}
#endif
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
	if (ms <= 0) {
		int err = ::pthread_mutex_lock(&impl->mutex);
		assert(err == 0);
		bool ok = impl->signaled;
		impl->signaled = false;
		err = pthread_mutex_unlock(&impl->mutex);
		assert(err == 0);
		return ok;
	} else {
		const uint64_t deadline = readMonotonicNanoseconds() + static_cast<uint64_t>(ms) * 1000000U;
	#ifndef __APPLE__
		::timespec absTS;
		absTS.tv_sec = static_cast<time_t>(deadline / 1000000000U);
		absTS.tv_nsec = static_cast<long>(deadline % 1000000000U);
	#endif

		int err = ::pthread_mutex_lock(&impl->mutex);
		assert(err == 0);

		while (!impl->signaled && err == 0) {
		#ifdef __APPLE__
			// The Mac has no pthread_condattr_setclock(), so it waits for what is left until the deadline instead.
			const uint64_t now = readMonotonicNanoseconds();
			if (now >= deadline) {
				err = ETIMEDOUT;
			} else {
				::timespec leftTS;
				leftTS.tv_sec = static_cast<time_t>((deadline - now) / 1000000000U);
				leftTS.tv_nsec = static_cast<long>((deadline - now) % 1000000000U);
				err = ::pthread_cond_timedwait_relative_np(&impl->condition, &impl->mutex, &leftTS);
			}
		#else
			err = ::pthread_cond_timedwait(&impl->condition, &impl->mutex, &absTS);
		#endif
			assert(err == 0 || err == ETIMEDOUT);
		}
		// Decided by the flag, not by `err`: POSIX lets the wait report ETIMEDOUT although the signal came in time.
		const bool ok = impl->signaled;
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

/*
	The writes are full barriers, so a sequentially consistent read only has to be an acquire. Compilers without the
	`__atomic` builtins (GCC before 4.7) get a full barrier after the read instead.
*/
template<typename T> static T loadSeqCst(const volatile T* x)
{
#ifdef __ATOMIC_SEQ_CST
	return __atomic_load_n(x, __ATOMIC_SEQ_CST);
#else
	const T y = *x;
	__sync_synchronize();
	return y;
#endif
}

// __sync_lock_test_and_set() is only an acquire barrier, so the barrier in front makes the exchange a full one.
template<typename T> static T exchangeSeqCst(volatile T* x, T y)
{
	__sync_synchronize();
	return __sync_lock_test_and_set(const_cast<T*>(x), y);
}

int AtomicInt::load(const volatile int* x)
{
	return loadSeqCst(x);
}

// An unconditional store. (A single compare-and-swap would silently skip it if another thread changed `x` first.)
int AtomicInt::assign(volatile int* x, int y)
{
	(void)exchangeSeqCst(x, y);
	__sync_synchronize();
	return y;
}

int AtomicInt::increment(volatile int* x)
{
	return __sync_add_and_fetch(const_cast<int*>(x), 1);
}

int AtomicInt::decrement(volatile int* x)
{
	return __sync_sub_and_fetch(const_cast<int*>(x), 1);
}

int AtomicInt::add(volatile int* x, int y)
{
	return __sync_add_and_fetch(const_cast<int*>(x), y);
}

int AtomicInt::swap(volatile int* x, int y)
{
	// The exchange returns the previous value in the same step; a compare-and-swap would have to be retried until success
	// or we wouldn't know what value to return.
	return exchangeSeqCst(x, y);
}

bool AtomicInt::swapIfEqual(volatile int* x, int equalTo, int y)
{
	return __sync_bool_compare_and_swap(const_cast<int*>(x), equalTo, y);
}

/* --- AtomicFloat --- */

// Floats go through the integer atomics as their bit patterns, copied with memcpy, the conforming way to reinterpret.
typedef char FloatIsIntSizedAssertion[sizeof (float) == sizeof (int) ? 1 : -1];

static int floatToBits(float f)
{
	int bits;
	memcpy(&bits, &f, sizeof (bits));
	return bits;
}

static float bitsToFloat(int bits)
{
	float f;
	memcpy(&f, &bits, sizeof (f));
	return f;
}

float AtomicFloat::load(const volatile float* x)
{
	return bitsToFloat(loadSeqCst(reinterpret_cast<const volatile int*>(x)));
}

// An unconditional store, through swap(). (A single compare-and-swap would silently skip it if another thread changed
// `x` first.)
float AtomicFloat::assign(volatile float* x, float y)
{
	(void)swap(x, y);
	__sync_synchronize();
	return y;
}

float AtomicFloat::swap(volatile float* x, float y)
{
	return bitsToFloat(exchangeSeqCst(reinterpret_cast<volatile int*>(x), floatToBits(y)));
}

bool AtomicFloat::swapIfEqual(volatile float* x, float equalTo, float y)
{
	return __sync_bool_compare_and_swap(reinterpret_cast<volatile int*>(x), floatToBits(equalTo), floatToBits(y));
}

/* --- AtomicPointerBaseClass --- */

const void* AtomicPointerBaseClass::load(const void* const volatile* p)
{
	return loadSeqCst(p);
}

// An unconditional store. (A single compare-and-swap would silently skip it if another thread changed `p` first.)
const void* AtomicPointerBaseClass::assign(const void* volatile* p, const void* q)
{
	(void)exchangeSeqCst(p, q);
	__sync_synchronize();
	return q;
}

const void* AtomicPointerBaseClass::swap(const void* volatile* p, const void* q)
{
	// The exchange returns the previous value in the same step; a compare-and-swap would have to be retried until success
	// or we wouldn't know what value to return.
	return exchangeSeqCst(p, q);
}

bool AtomicPointerBaseClass::swapIfEqual(const void* volatile* p, const void* equalTo, const void* q)
{
	return __sync_bool_compare_and_swap(const_cast<const void**>(p), equalTo, q);
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
	}
	catch (...) {
		assert(0);
	}
	// Outside the try, so joiners are released even if run() threw.
	impl->stage = STOPPED;
	impl->stoppedEvent.signal();
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

// Calls pthread_join exactly once, whichever joiner gets here first. Only called once the thread has stopped.
void Thread::Impl::reap() {
	MutexLock lock(joinMutex);
	if (stage != JOINED) {
		assert(stage == STOPPED);
		int err = ::pthread_join(thread, 0);
		(void)err;
		assert(err == 0);
		stage = JOINED;
	}
}

Thread::Impl::~Impl()
{
}

int Thread::readMsTimer()
{
	return wrapToInt32(static_cast<unsigned int>(readMonotonicNanoseconds() / 1000000U));
}

void Thread::sleep(int ms) {
	// nanosleep() takes any length (1000 * ms overflows for usleep(), which POSIX only specifies up to a second) and is
	// resumed after a signal.
	::timespec remaining;
	remaining.tv_sec = (ms > 0 ? ms / 1000 : 0);
	remaining.tv_nsec = (ms > 0 ? (ms % 1000) * 1000000L : 0);
	while (::nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
	}
}

void Thread::yield()
{
	::sched_yield();
}

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

/*
	stoppedEvent releases one waiter at a time, so every joiner that gets it signals it again. That releases the next
	joiner and leaves it signaled for any later one, like a Win32 thread handle.
*/
void Thread::join() const {
	impl->stoppedEvent.wait();
	impl->stoppedEvent.signal();
	impl->reap();
}

bool Thread::timedJoin(int ms) const {
	if (!impl->stoppedEvent.timedWait(ms)) {
		return false;
	}
	impl->stoppedEvent.signal();
	impl->reap();
	return true;
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
	} else {
		impl->reap();
	}
	if (--impl->keepCounter == 0) {
		delete impl;
	}
}

int queryCPUCount()
{
	long cpuCount = ::sysconf(_SC_NPROCESSORS_ONLN);
	return (cpuCount > 0 ? static_cast<int>(cpuCount) : 0);
}

} /* namespace NuX */
