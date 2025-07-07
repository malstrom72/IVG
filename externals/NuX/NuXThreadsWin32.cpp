#include "NuXThreadsWin32.h"
#include <sstream>
#include <process.h>

#if defined(_UINTPTR_T_DEFINED)
	typedef uintptr_t beginthreadexReturnType;
#else
	typedef unsigned long beginthreadexReturnType;
#endif

namespace NuXThreads {

template<typename T, typename U> T lossless_cast(U x) { assert(static_cast<T>(x) == x); return static_cast<T>(x); }

void ThreadMemoryFence() {
	MemoryBarrier();
}

static std::string convertToUTF8String(const std::wstring& w) {
	const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.data(), lossless_cast<int>(w.size()), NULL, 0, NULL, NULL);
	assert(sizeNeeded > 0);
	std::string utf8(sizeNeeded, 0);
	const int result = WideCharToMultiByte(CP_UTF8, 0, w.data(), lossless_cast<int>(w.size()), &utf8[0], sizeNeeded, NULL, NULL);
	assert(result == sizeNeeded);
	return utf8;
}

static void throwWin32Exception(const std::string& errorStringUTF8, ::DWORD errorCode) {
	std::wostringstream message;
	if (errorCode != 0) {
		std::vector<wchar_t> messageBuffer(4096);
		::DWORD formatMessageReturn = ::FormatMessageW
		( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode
			, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), messageBuffer.data()
			, sizeof (messageBuffer) - 1, NULL);
		if (formatMessageReturn != 0) {
			size_t length = wcslen(messageBuffer.data());
			while (length > 0 && (messageBuffer[length - 1] == '\r' || messageBuffer[length - 1] == '\n')) {
				--length;
			}
			messageBuffer[length] = '\0';
			message << " : " << messageBuffer.data();
		}
		message << " [" << errorCode << ']';
	}
	throw Exception(errorStringUTF8 + convertToUTF8String(message.str()), errorCode);
}

/* --- Mutex --- */

Mutex::Impl::Impl() { }
CRITICAL_SECTION* Mutex::Impl::getWin32CriticalSection() { return &cs; }

Mutex::Mutex() : impl(new Impl()) // Ok to allocate single initializer
{
	::InitializeCriticalSection(&impl->cs);
}
void Mutex::lock() const throw() { ::EnterCriticalSection(&impl->cs); }
bool Mutex::tryToLock() const throw() { return (::TryEnterCriticalSection(&impl->cs) == TRUE); }
void Mutex::unlock() const throw() { ::LeaveCriticalSection(&impl->cs); }

Mutex::~Mutex() {
	::DeleteCriticalSection(&impl->cs);
	delete impl;
}

/* --- Event --- */

Event::Impl::Impl() : handle(INVALID_HANDLE_VALUE) { }
::HANDLE Event::Impl::getWin32Handle() const { return handle; }

Event::Event() : impl(new Impl()) // Ok to allocate single initializer
{
	try {
		impl->handle = ::CreateEvent(0, FALSE, FALSE, 0);
		if (impl->handle == 0 || impl->handle == INVALID_HANDLE_VALUE) {
			throwWin32Exception("Error creating waitable event", ::GetLastError());
		}
	}
	catch (...) {
		delete impl;
		throw;
	}
}

void Event::signal() {
	::BOOL setEventReturn = ::SetEvent(impl->handle);
	if (!setEventReturn) {
		throwWin32Exception("Error signaling waitable event", ::GetLastError());
	}
}

void Event::reset() {
	::BOOL resetEventReturn = ::ResetEvent(impl->handle);
	if (!resetEventReturn) {
		throwWin32Exception("Error resetting waitable event", ::GetLastError());
	}
}

bool Event::timedWait(int ms) {
	::DWORD waitForSingleObjectExReturn = ::WaitForSingleObjectEx(impl->handle, ms, FALSE);
	switch (waitForSingleObjectExReturn) {
		case WAIT_OBJECT_0: return true;
		case WAIT_FAILED: throwWin32Exception("Error waiting on an event", ::GetLastError());
	}
	return false;
}

void Event::wait() { timedWait(INFINITE); }

Event::~Event() {
	::BOOL closeHandleReturn = ::CloseHandle(impl->handle);
	assert(closeHandleReturn);
	delete impl;
}

/* --- AtomicInt --- */

int AtomicInt::assign(volatile int* x, int y) {
	InterlockedExchange((LPLONG)(x), y);
	return y;
}

int AtomicInt::increment(volatile int* x) { return InterlockedIncrement((LPLONG)(x)); }
int AtomicInt::decrement(volatile int* x) { return InterlockedDecrement((LPLONG)(x)); }
int AtomicInt::add(volatile int* x, int y) { return InterlockedExchangeAdd((LPLONG)(x), y) + y; }
int AtomicInt::swap(volatile int* x, int y) { return InterlockedExchange((LPLONG)(x), y); }
bool AtomicInt::swapIfEqual(volatile int* x, int equalTo, int y) {
	return (InterlockedCompareExchange((LPLONG)(x), y, equalTo) == equalTo);
}

/* --- AtomicFloat --- */

float AtomicFloat::assign(volatile float* x, float y) {
	InterlockedExchange((LPLONG)(x), *(LPLONG)(&y));
	return y;
}

float AtomicFloat::swap(volatile float* x, float y) {
	int z = InterlockedExchange((LPLONG)(x), *(LPLONG)(&y));
	return *reinterpret_cast<float*>(&z);
}

bool AtomicFloat::swapIfEqual(volatile float* x, float equalTo, float y) {
	return (InterlockedCompareExchange((LPLONG)(x), *(LPLONG)(&y), *(LPLONG)(&equalTo)) == equalTo);
}

/* --- AtomicPointerBaseClass --- */

const void* AtomicPointerBaseClass::swap(const void* volatile* p, const void* q) {
	#if (_MSC_VER >= 1300) // Version 7 and 8 of the MSVC compiler issues warnings on the 32-bit version of 'InterlockedExchangePointer' that are of no importance.
		#pragma warning(push)
		#pragma warning(disable: 4311)
		#pragma warning(disable: 4312)
	#endif
	#if (__INTEL_COMPILER) // Intel-compiler has another warning.
		#pragma warning(disable: 1684)
	#endif
	return InterlockedExchangePointer(const_cast<void* volatile*>(p), const_cast<void*>(q));
	#if (_MSC_VER >= 1300)
		#pragma warning(pop)
	#endif
}

const void* AtomicPointerBaseClass::assign(const void* volatile* p, const void* q) {
	swap(p, q);
	return q;
}

bool AtomicPointerBaseClass::swapIfEqual(const void* volatile* p, const void* equalTo, const void* q) {
	return (InterlockedCompareExchangePointer(const_cast<void* volatile*>(p)
			, const_cast<void*>(q), const_cast<void*>(equalTo)) == equalTo);
}

/* --- Thread --- */

unsigned int __stdcall Thread::Impl::win32ThreadFunction(void* param) {
	try {
		Impl* impl = reinterpret_cast<Impl*>(param);
		if (impl->started != 0) {
			impl->runner->run();
		}
		if (--(impl->keepCounter) == 0) {
			delete impl;
		}
	}
	catch (...) {
		assert(0);
	}
	return 0;
}

Thread::Impl::Impl(Runnable* runner) : runner(runner), handle(INVALID_HANDLE_VALUE), id(0), started(0), keepCounter(1) {
	unsigned int uintId;
	beginthreadexReturnType beginthreadexReturn = ::_beginthreadex(0, 0, win32ThreadFunction, this
			, CREATE_SUSPENDED, &uintId);
	id = reinterpret_cast<ThreadId>(static_cast<intptr_t>(uintId));
	if (beginthreadexReturn == 0) {
		throw Exception("Error creating thread", errno);
	}
	++keepCounter;
	handle = reinterpret_cast< ::HANDLE >(beginthreadexReturn);
}

::HANDLE Thread::Impl::getWin32Handle() const { return handle; }

Thread::Impl::~Impl() {
	assert(handle != INVALID_HANDLE_VALUE);
	::BOOL closeHandleReturn = ::CloseHandle(handle);
	assert(closeHandleReturn);
}

int Thread::readMsTimer()
{
#ifndef NDEBUG // If we are in debug configuration, "start" the ms-timer close to 0xFFFFFFFF to force it to wrap in a short time. This is meant to provoke bugs that otherwise are hard to produce.
	static ::DWORD firstTickCount = ::GetTickCount() + (100U << ((::GetTickCount() * 0x569CF9F3U + 0xF923C1D1U) & 15U));
	return wrapToInt32(::GetTickCount() - firstTickCount);
#else
	return wrapToInt32(::GetTickCount());
#endif
}

void Thread::sleep(int ms) { ::Sleep(ms); }
void Thread::yield() { ::SwitchToThread(); }
ThreadId Thread::getCurrentId() { return reinterpret_cast<ThreadId>(static_cast<intptr_t>(::GetCurrentThreadId())); }

Thread::Thread() : impl(0) { impl = new Impl(this); }

Thread::Thread(Runnable& runner) : impl(new Impl(&runner)) // Ok to allocate single initializer
{
}

/* FIX stupid range? */
/*
	Priority Value (priority) | Win32 Priority Level         
	---------------------------------------------------
	-10                        | THREAD_PRIORITY_IDLE               
	-9                         | THREAD_PRIORITY_IDLE             
	-8                         | THREAD_PRIORITY_IDLE             
	-7                         | THREAD_PRIORITY_IDLE             
	-6                         | THREAD_PRIORITY_LOWEST       
	-5                         | THREAD_PRIORITY_LOWEST       
	-4                         | THREAD_PRIORITY_LOWEST       
	-3                         | THREAD_PRIORITY_BELOW_NORMAL             
	-2                         | THREAD_PRIORITY_BELOW_NORMAL             
	-1                         | THREAD_PRIORITY_BELOW_NORMAL             
	0                          | THREAD_PRIORITY_NORMAL       
	1                          | THREAD_PRIORITY_NORMAL       
	2                          | THREAD_PRIORITY_NORMAL       
	3                          | THREAD_PRIORITY_NORMAL            
	4                          | THREAD_PRIORITY_ABOVE_NORMAL            
	5                          | THREAD_PRIORITY_ABOVE_NORMAL            
	6                          | THREAD_PRIORITY_ABOVE_NORMAL      
	7                          | THREAD_PRIORITY_HIGHEST      
	8                          | THREAD_PRIORITY_HIGHEST      
	9                          | THREAD_PRIORITY_HIGHEST      
	10                         | THREAD_PRIORITY_TIME_CRITICAL
*/
void Thread::setPriority(int priority)
{
	assert(priority >= -10 && priority <= 10);
	int win32Priority;
	switch ((priority + 10) * 3 / 10) {
		default: assert(0);
		case 0: win32Priority = THREAD_PRIORITY_IDLE; break;
		case 1: win32Priority = THREAD_PRIORITY_LOWEST; break;
		case 2: win32Priority = THREAD_PRIORITY_BELOW_NORMAL; break;
		case 3: win32Priority = THREAD_PRIORITY_NORMAL; break;
		case 4: win32Priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
		case 5: win32Priority = THREAD_PRIORITY_HIGHEST; break;
		case 6: win32Priority = THREAD_PRIORITY_TIME_CRITICAL; break;
	}
	::BOOL setThreadPriorityReturn = ::SetThreadPriority(impl->handle, win32Priority);
	if (!setThreadPriorityReturn) {
		throwWin32Exception("Error setting thread priority", ::GetLastError());
	}
}

void Thread::start() {
	if (impl->started.swap(1) == 0) {
		::DWORD resumeThreadReturn = ::ResumeThread(impl->handle);
		if (resumeThreadReturn != 1) {
			throwWin32Exception("Error starting thread", (resumeThreadReturn == 0xFFFFFFFF) ? (::GetLastError()) : 0);
		}
	}
}

bool Thread::timedJoin(int ms) const {
	assert(impl->started != 0);
	switch (::WaitForSingleObject(impl->handle, ms)) {
		case WAIT_OBJECT_0: return true;
		case WAIT_FAILED: throwWin32Exception("Error waiting on thread to finish", ::GetLastError());
	}
	return false;
}

void Thread::join() const { assert(impl->started != 0); timedJoin(INFINITE); }
ThreadId Thread::getId() const { return impl->id; }
bool Thread::isRunning() const { return (impl->started != 0 && !timedJoin(0)); }

Thread::~Thread()
{
	if (impl->started == 0) { // abandoned before started, let it run through without executing the runner
		::DWORD resumeThreadReturn = ::ResumeThread(impl->handle);
		assert(resumeThreadReturn == 1);
	}
	if (--impl->keepCounter == 0) {
		delete impl;
	}
}

}