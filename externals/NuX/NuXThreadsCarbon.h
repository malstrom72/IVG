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
/*
 *  NuXThreadsCarbon.h
 *  NuXTest
 *
 *  Created by Magnus Lidström on 1/9/06.
 *  Copyright 2005-2025 NuEdge Development. All rights reserved.
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
