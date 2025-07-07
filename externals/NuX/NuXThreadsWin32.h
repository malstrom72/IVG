#ifndef NuXThreadsWin32_h
#define NuXThreadsWin32_h

#if (_WIN32_WINNT < 0x0500)
	#undef _WIN32_WINNT
	#define _WIN32_WINNT 0x0500
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
	#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include "NuXThreads.h"

namespace NuXThreads {

class Mutex::Impl {
	friend class Mutex;
	protected:	Impl();
	public:		CRITICAL_SECTION* getWin32CriticalSection();
	protected:	CRITICAL_SECTION cs;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Event::Impl {
	friend class Event;
	protected:	Impl();
	public:		::HANDLE getWin32Handle() const;
	protected:	::HANDLE handle;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

class Thread::Impl {
	friend class Thread;
	protected:	Impl(Runnable* runner);
	public:		::HANDLE getWin32Handle() const;
	public:		virtual ~Impl();
	protected:	static unsigned int __stdcall win32ThreadFunction(void* param);
	protected:	Runnable* const runner;
	protected:	::HANDLE handle;
	protected:	ThreadId id;
	protected:	NuXThreads::AtomicInt started;
	protected:	NuXThreads::AtomicInt keepCounter;
	private:	Impl(const Impl& copy); // N/A
	private:	Impl& operator=(const Impl& copy); // N/A
};

}

#endif
