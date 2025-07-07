/*
 *  NuXThreadsPosix.h
 *
 *  Created by Magnus Lidström on 1/9/06.
 *  Copyright 2006 NuEdge Development. All rights reserved.
 *
 */

#ifndef NuXThreadsPosix_h
#define NuXThreadsPosix_h

#include <pthread.h>
#include "NuXThreads.h"

namespace NuXThreads {

class Mutex::Impl {
	friend class Mutex;
	protected:	Impl();
	public:		::pthread_mutex_t& getPThreadMutex();
	protected:	::pthread_mutex_t mutex;
	protected:	~Impl();
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Event::Impl : public Mutex::Impl {
	friend class Event;
	protected:	Impl();
	public:		::pthread_cond_t& getPThreadCondition();
	protected:	::pthread_cond_t condition;
	protected:	bool signaled;
	protected:	~Impl();
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Thread::Impl {
	friend class Thread;
	protected:	Impl(Runnable* runner);
	public:		::pthread_t getPThreadId() const;
	public:		virtual ~Impl();

	protected:	enum Stage { SUSPENDED, RUNNING, STOPPED, JOINED };
	protected:	static void* startRoutine(void*);
	protected:	Runnable* const runner;
	protected:	::pthread_t thread;
	protected:	Event startEvent;
	protected:	Event stoppedEvent;
	protected:	AtomicInt stage;
	protected:	AtomicInt keepCounter;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

} /* namespace NuX */

#endif
