/*
 *  NuXThreadsCarbon.h
 *  NuXTest
 *
 *  Created by Magnus Lidström on 1/9/06.
 *  Copyright 2006 NuEdge Development. All rights reserved.
 *
 */

#ifndef NuXThreadsCarbon_h
#define NuXThreadsCarbon_h

#include <Carbon/Carbon.h>

#include "NuXThreads.h"

namespace NuXThreads {

class Mutex::Impl {
	friend class Mutex;
	protected:	Impl();
	public:		::MPCriticalRegionID getCarbonCriticalRegionId() const;
	protected:	::MPCriticalRegionID criticalRegionId;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Event::Impl {
	friend class Event;
	protected:	Impl();
	public:		::MPEventID getCarbonEventId() const;
	protected:	::MPEventID eventId;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Thread::Impl {
	friend class Thread;
	protected:	Impl(Runnable* runner);
	public:		::MPTaskID getCarbonTaskId() const;
	public:		virtual ~Impl();
	protected:	static ::OSStatus carbonTaskEntryPoint(void* parameter);
	protected:	AtomicInt keepCounter;
	protected:	Runnable* volatile runner;
	protected:	::MPTaskID taskId;
	protected:	::MPQueueID queueId;
	protected:	Event startEvent;
	protected:	bool started;
	protected:	bool killed;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

} /* namespace NuX */

#endif
