/**
	\file NuXThreads.h
	
	NuXThreads is a library for:

	1) Creating asynchronous concurrent threads of execution.
	2) Synchronizing shared thread data and operation sequences using basic "synchronization primitives".
	3) Operating concurrently on integers and pointers with "atomic operations".
	
	NuXThreads is part of the NuEdge X-Platform Library / NuX.
	Written by Magnus Lidstroem
	(C) NuEdge Development 2005
	All Rights Reserved

	NuX design goals:
	
	1) Cross platform with effective OS-specific implementations for Windows XP and Mac OS X Carbon.
	2) Emphasis on native platform approaches and solutions. Follows rules and conventions of the supported platforms to maximum extent.
	3) Light-weight with small header files that do not depend on heavy platform-specific headers.
	4) Self-contained components with few dependencies. Few source files. Easily integrated into existing projects in whole or in part.
	5) Minimalistic but flexible approach, providing as few necessary building blocks as possible without sacrificing versatility.
	6) Easily understood standard C++ code, avoiding complex templates and using only a small set of the Standard C++ Library and STL.
	7) Self-explanatory code with inline documentation. Clear and consistent naming conventions.
*/

#ifndef NuXThreads_h
#define NuXThreads_h

#include <string>
#include <vector>
#include <exception>
#include <algorithm>
#include "assert.h"

namespace NuXThreads {

/*
	Useful utilities for deterministic wrapping behavior of signed integers, since this is undefined behavior in the C++
	standard and optimizer is free to interpret e.g. `(a - b) >= 0` as `(a > b)`. Notice that it is always safe to cast
	to an unsigned type.
*/
inline signed int wrapToInt32(unsigned int i) {
	return (i >= 0x80000000U ? static_cast<signed int>(i - 0x80000000U) - 0x7FFFFFFF - 1 : static_cast<signed int>(i));
}


// FIX : implement throw()?

// FIX doc
class Exception : public std::exception {
	public:		Exception(const std::string& errorString, int osErrorCode = 0)
						: errorString(errorString), osErrorCode(osErrorCode) { }
	public:		virtual const char *what() const throw() { return errorString.c_str(); }
	public:		virtual ~Exception() throw() { }
	public:		std::string errorString;
	public:		int osErrorCode;
};

/**
	The AtomicInt class encapsulates a 32-bit integer and the "atomic operations" you can perform on it. It uses
	operator overloading to emulate the native int type.
	
	Atomic operations are guaranteed to yield consistent results on values used by multiple threads in a multi-threaded
	environment. For such matters you cannot trust standard C++ types and operators since even basic operations such as
	addition or subtraction may require several steps to be carried out on microprocessor level. If multiple threads
	are accessing the same data these microprocessor operations needs to be serialized in order to prevent them from
	interfering with each other (a situation called a "race condition").
	
	Even simple assignments of native data types may turn out to be not entirely thread-safe, especially not on multi-
	processor systems. The reason is that the memory writes involved may need to be synchronized in the proper sequence
	across all cpu's. This is not guaranteed unless a so called "read/write barrier" is performed. This class ensures
	this as well.
*/
class AtomicInt {
	public:		static int assign(volatile int* x, int y); // FIX : comment
	public:		static int increment(volatile int* x);							///< Increments the 32-bit integer pointed to by \p x by one "atomically" and returns the resulting value. You can use this static method on your 32-bit ints as an alternative to using the entire class.
	public:		static int decrement(volatile int* x);							///< Decrements the 32-bit integer pointed to by \p x by one "atomically" and returns the resulting value. You can use this static method on your 32-bit ints as an alternative to using the entire class.
	public:		static int add(volatile int* x, int y);							///< Adds \p y to the 32-bit integer pointed to by \p x "atomically" and returns the resulting value. You can use this static method on your 32-bit ints as an alternative to using the entire class.
	public:		static int swap(volatile int* x, int y);						///< Sets the 32-bit integer pointed to by \p x to \p y "atomically" and returns the previous value. You can use this static method on your 32-bit ints as an alternative to using the entire class.
	public:		static bool swapIfEqual(volatile int* x, int equalTo, int y);	///< Like swap() but only sets the new value if the previous value equals to \p equalTo. Returns true if the previous value equalled \p equalTo. You can use this static method on your 32-bit ints as an alternative to using the entire class.
	public:		AtomicInt() : x(0) { }											///< (AtomicInts are always initialized to 0 by default.)
	public:		AtomicInt(int x) : x(x) { }
	public:		AtomicInt(const AtomicInt& copy) { assign(&x, copy.x); }
	public:		AtomicInt& operator=(const AtomicInt& copy) { assign(&x, copy.x); return (*this); }
	public:		AtomicInt& operator=(int y) { assign(&x, y); return (*this); }
	public:		operator int() const { return x; }
	public:		int operator++() { return increment(&x); }
	public:		int operator++(int) { return increment(&x) - 1; }
	public:		int operator--() { return decrement(&x); }
	public:		int operator--(int) { return decrement(&x) + 1; }
	public:		int operator+=(int y) { return add(&x, y); }
	public:		int operator-=(int y) { return add(&x, -y); }
	public:		int swap(int y) { return swap(&x, y); }							///< Sets the value to \p y "atomically" and returns the previous value.
	public:		bool swapIfEqual(int equalTo, int y) {							///< Like swap() but only sets the new value if the previous value equals to \p equalTo. Returns true if the previous value equalled \p equalTo.
					return swapIfEqual(&x, equalTo, y);
				}
	protected:	volatile int x;
};

/**
	AtomicFloat works like AtomicInt but is meant for 32-bit floats.
	
	You cannot increment or decrement floats atomically nor add to or subtract from them.
*/
class AtomicFloat {
	public:		static float assign(volatile float* x, float y);
	public:		static float swap(volatile float* x, float y);						///< Sets the 32-bit float pointed to by \p x to \p y "atomically" and returns the previous value. You can use this static method on your 32-bit floats as an alternative to using the entire class.
	public:		static bool swapIfEqual(volatile float* x, float equalTo, float y); ///< Like swap() but only sets the new value if the previous value equals to \p equalTo. Returns true if the previous value equalled \p equalTo. You can use this static method on your 32-bit floats as an alternative to using the entire class.
	public:		AtomicFloat() : x(0.0f) { }											///< (AtomicFloats are always initialized to 0 by default.)
	public:		AtomicFloat(float x) : x(x) { }
	public:		AtomicFloat(const AtomicFloat& copy) { assign(&x, copy.x); }
	public:		AtomicFloat& operator=(const AtomicFloat& copy) { assign(&x, copy.x); return (*this); }
	public:		AtomicFloat& operator=(float y) { assign(&x, y); return (*this); }
	public:		operator float() const { return x; }
	public:		float swap(float y) { return swap(&x, y); }							///< Sets the value to \p y "atomically" and returns the previous value.
	public:		bool swapIfEqual(float equalTo, float y) {							///< Like swap() but only sets the new value if the previous value equals to \p equalTo. Returns true if the previous value equalled \p equalTo.
					return swapIfEqual(&x, equalTo, y);
				}
	protected:	volatile float x;
};

class AtomicPointerBaseClass { // FIX : comments
	public:		static const void* assign(const void* volatile* p, const void* q);
	public:		static const void* swap(const void* volatile* p, const void* q);
	public:		static bool swapIfEqual(const void* volatile* p, const void* equalTo, const void* q);
};

/**
	AtomicPointer works like AtomicInt but should be used for pointers instead of integers. This is a template so that you
	can easily create atomic pointers of different types. Besides the convenience of having a templatized class for this
	purpose, we also need to handle pointers differently from integers to achieve compatibility with systems that use
	64-bit pointers.
	
	You cannot increment or decrement pointers atomically nor add to or subtract from them.
*/
template<typename T> class AtomicPointer : public AtomicPointerBaseClass { // FIX : comments
	public:		AtomicPointer() : p(0) { }
	public:		AtomicPointer(T* p) : p(p) { }
	public:		AtomicPointer(const AtomicPointer<T>& copy) { assign(&p, copy.p); }
	public:		AtomicPointer& operator=(const AtomicPointer<T>& copy) { assign(&p, copy.p); return (*this); }
	public:		AtomicPointer& operator=(T* q) { AtomicPointerBaseClass::assign((const void* volatile*)(&p), q); return (*this); }
	public:		operator T*() const { return p; }
	public:		T* operator->() const { assert(p != 0); return p; }
	public:		T* swap(T* q) { return reinterpret_cast<T*>(const_cast<void*>(AtomicPointerBaseClass::swap(reinterpret_cast<const void* volatile*>(const_cast<const T* volatile*>(&p)), q))); }
	public:		bool swapIfEqual(T* equalTo, T* q) { return AtomicPointerBaseClass::swapIfEqual(reinterpret_cast<const void* volatile*>(const_cast<const T* volatile*>(&p)), equalTo, q); }
	protected:	T* volatile p;
};

void ThreadMemoryFence(); ///< Provides a full memory barrier to enforce ordering of reads and writes across threads. Guarantees both compiler and CPU reordering protection.

// FIX : use InitializeCriticalSectionAndSpinCount or SetCriticalSectionSpinCount under Windows?

/**
	Mutexes are usually used to coordinate mutually-exclusive access to a shared resource. Only one thread at a time can
	hold the lock to a mutex. If another thread tries to claim the lock it will be blocked until the first thread releases
	it.	This implemention allows recursive locking, i.e. once a thread has acquired the lock of a mutex it can make
	additional lock attempts without blocking execution. Internally, mutexes are implemented using "critical sections"
	under Windows ??? mac
*/
class Mutex {
	public:		Mutex();
	public:		void lock() const throw();			///< Tries to claim the lock on the mutex. Blocks execution if the lock is held by another thread.
	public:		bool tryToLock() const throw();		///< Tries to claim the lock on the mutex. Returns true on success and false if the lock is held by another thread.
	public:		void unlock() const throw();		///< Unlocks the mutex. Every call to lock() must be matched with a call to unlock().
	public:		virtual ~Mutex();

	// Implementation
	public:		class Impl;
	public:		Impl* getImpl() const { return impl; }
	protected:	Impl* impl;
	private:	Mutex(const Mutex& copy); // N/A
	private:	Mutex& operator=(const Mutex& copy); // N/A
};

// FIX : inner-class in Mutex class?
/**
	The MutexLock is a utility class for "scoped" locking and unlocking of a mutex. By using this class instead of
	manually calling lock() and unlock() you are guaranteed never to leave a mutex locked by mistake.
	
	Use like this: { Mutex myMutex; { MutexLock lock(myMutex); ... some serious business ... } }
*/
class MutexLock {
	public:		MutexLock(const Mutex& mutex) : mutex(mutex) { mutex.lock(); }
	public:		MutexLock(const MutexLock& other) : mutex(other.mutex) { mutex.lock(); }
	public:		virtual ~MutexLock() { mutex.unlock(); }
	protected:	const Mutex& mutex;
	private:	MutexLock& operator=(const MutexLock& copy); // N/A
};

template<typename T> class Lockable {
	public:		class Lock {
					public:		Lock(const Lockable& l) : m(l.mutex), r(l.resource) { }
					public:		T& access() const { return r; }
					public:		T& operator=(const T& copy) const { if (&r != &copy) { r = copy; }; return r; }
					public:		T& operator=(T&& move) const { if (&r != &move) { r = std::move(move); } return r; }
					public:		T* operator->() const { return &r; }
					public:		operator T&() const { return r; }
					protected:	MutexLock m;
					protected:	T& r;
				};
	public:		Lockable() { }
	public:		Lockable(const T& copy) : resource(copy) { }
    public:		Lockable(Lockable&& other) : resource(moveResource(std::move(other))) { }
	public:		Lockable& operator=(const T& copy) {
					if (&resource != &copy) {
						MutexLock lock(mutex);
						resource = copy;
					}
					return *this;
				}
	public:		Lockable& operator=(T&& move) {
					if (&resource != &move) {
						MutexLock lock(mutex);
						resource = std::move(move);
					}
					return *this;
				}
	public:		Lockable& operator=(const Lockable& copy) {
					if (this != &copy) {
						MutexLock lockThis(mutex);
						MutexLock lockCopy(copy.mutex);
						resource = copy.resource;
					}
					return *this;
				}
	public:		Lockable& operator=(Lockable&& other) {
					if (this != &other) {
						MutexLock lockThis(mutex);
						MutexLock lockOther(other.mutex);
						resource = std::move(other.resource);
					}
					return *this;
				}
	public:		Lock lock() const { return Lock(*this); }
	public:		Lock operator->() const { return Lock(*this); }
	public:		T get() const { return Lock(*this).access(); }
	private:	static T moveResource(Lockable&& other) {
					MutexLock lock(other.mutex);
					return std::move(other.resource);
				}
	private:	mutable Mutex mutex;
	private:	mutable T resource;
};

/**
	Events are used to synchronize and coordinate operation sequences among multiple threads.
	
	For example this class can be used if one thread needs to wait for an operation to complete on another thread. (The
	bad alternative would be to set a flag when the operation was complete and to wait on this flag with a "busy loop".
	This drains unnecessary CPU.)
	
	If several threads wait on the same event, only one of them will be released when the event is being signaled. Which
	thread that will be is totally random.
*/
class Event {
	public:		Event();
	public:		void signal();				///< Sets the event state to "signaled". If a thread is blocked waiting for this signal it will be released. If several threads are waiting only one will be released. If no thread is waiting the state will remain "signaled" and the next thread that waits will be released immediately (and only that thread).
	public:		void reset();				///< Resets a "signaled" event to unsignaled. The next thread that waits for this signal will be blocked until signal() is called.
	public:		void wait();				///< Waits on the event to be signaled (by a call to signal() by another thread). This method will wait infinitely on the signal to be raised. The signal is automatically reset when the wait completes.
	public:		bool timedWait(int ms);		///< Like wait() but with a time-out (in milliseconds). Returns true if the signal was raised within the time-out period.
	public:		virtual ~Event();
	
	// Implementation
	public:		class Impl;
	public:		Impl* getImpl() const { return impl; }
	protected:	Impl* impl;
	private:	Event(const Event& copy); // N/A
	private:	Event& operator=(const Event& copy); // N/A
};

/**
	Runnable is an abstract base class (or interface) that Thread uses for running threads. See the Thread class for
	more info.
*/
class Runnable {
	public:		virtual void run() = 0;
	public:		virtual ~Runnable() = 0;
};

typedef struct ThreadIdVoid {} * ThreadId;

// FIX : implement functor and function pointer "as a thread" support.
/**
	Thread is a class that manages an asynchronous concurrent thread.
	
	There are two ways to create a thread and implement the code that it should run. Either create a subclass of Thread,
	override run() and use the default constructor, or create an object that inherits and implements the Runnable
	interface and pass this object to the Thread constructor.
*/
class Thread : public Runnable {
// FIX : should these really be statics or rather global functions in the namespace?
	public:		static int readMsTimer();		///< Reads the system millisecond timer. Use this function to measure time intervals. Remember that, at least in theory, 32-bit timer values may overflow and "wrap". Thus you should never compare two timer values with comparison operators (e.g. x >= y + 100), but instead compare the difference of the two timer values (e.g. wrapToInt32(x - y) >= 100).
	public:		static void sleep(int ms);		///< Suspends execution of the current thread for at least \p ms milliseconds and passes the cpu to any other thread that is not waiting.
	public:		static void yield();			///< Releases the remainder of the thread's time slice to any other thread that is running.
	public:		static ThreadId getCurrentId();	///< Returns the currently running thread's unique id.	
// FIX : add optional stack-size
	public:		Thread();						///< The default constructor of Thread will assign itself as the "runner", i.e. you should use this constructor only if you create a subclass of Thread and override run(). Once constructed, the thread is allocated but suspended. Use start() to run.
	public:		Thread(Runnable& runner);		///< This constructor assigns a Runnable object as the "runner", i.e. use this constructor if you have a subclass of Runnable that you wish to use to run the thread. Once constructed, the thread is allocated but suspended. Use start() to run.
	public:		void setPriority(int priority);	///< Sets the thread priority. \p priority is between -10 and 10, where 0 is normal, -10 is lowest priority and 10 is highest.
	public:		void start();					///< Starts running the thread. Calling this method more than once has no effect.
	public:		void join() const;				///< Blocks the current thread and waits infinitely for the thread represented by this object to exit "naturally". (The thread must have been started by start() before calling this method.)
	public:		bool timedJoin(int ms) const;	///< As join() but with a time-out of \p ms milliseconds. Returns false if the thread didn't exit.
	public:		ThreadId getId() const;			///< Gets the unique id of the thread represented by this object.
	public:		bool isRunning() const;			///< Returns true if the thread has started and not exited yet.
// FIX : drop, this is such a stupid routine, async cancelling even doesn't work right with pthreads on MacOS
//	public:		void kill();					///< Aborts the thread abruptly. Use this method only in extreme emergency since it will not allow the thread to cleanup and release allocated or locked resources before exiting.
	protected:	virtual void run();				///< Override this method with the code that should be run by the thread or use the alternative Runnable class. (The default implementation of run() does nothing and asserts.)
	public:		virtual ~Thread();				///< When Thread destructs it does not automatically terminate the thread it represents but leaves it running (if it has started). This means that you will lose control of the thread so normally you should call join() before destroying Thread.

	// Implementation
	public:		class Impl;
	public:		Impl* getImpl() const { return impl; }
	protected:	Impl* impl;
	private:	Thread(const Thread& copy); // N/A
	private:	Thread& operator=(const Thread& copy); // N/A
};

// FIX : if yield etc is not part of thread class we could declare snapshot above with the rest of the sync primitives.
/*
	NuXThreads::Snapshot is a lightweight snapshot-based concurrent container
	for a single object of type T, but with multiple "slots" to allow safe
	access from multiple threads without conventional locking mechanisms.

	Important Concurrency Detail
	----------------------------
	- When you acquire a Guard on the current active slot, you prevent other
	  threads from swapping in a new active slot while that Guard exists.
	  However, if multiple threads simultaneously hold Guards to the *same*
	  active slot, they can share data changes among themselves. In other
	  words, guarding doesn't serialize modifications within the same snapshot.
	  It only prevents other threads from making a *new* snapshot (via set,
	  swap, etc.) and making that new snapshot active.

	- This design means older snapshots can still be referenced by other
	  threads if they locked them earlier, so your writes won't affect those
	  older snapshots.

	Destruction Control with setWaitAndDestroy
	------------------------------------------
	- setWaitAndDestroy(...) replaces the active slot with new data and then
	  waits until no other thread references the old slot before destructing
	  it, ensuring the destructor (if T has one) is called in your current
	  thread.

	Small Example with a Struct
	---------------------------
	#include <iostream>

	struct MyData {
		int a;
		float b;
	};

	int main() {
		// Create a snapshot with initial data
		Snapshot<MyData> data({10, 3.14f});

		// Overwrite the entire struct "atomically"
		data = {20, 2.71f};

		// Acquire a Guard to modify the active slot
		{
			auto guard = data.guard();
			guard->a = 30;
			guard->b = 1.23f;
			// Another thread also guarding() at the same time would see and share these changes
			// within the same active snapshot.
		}

		// Read the entire struct "atomically"
		MyData copyOfActive = data;
		std::cout << copyOfActive.a << ", " << copyOfActive.b << std::endl;

		// If you want control over where MyData is destroyed, call setWaitAndDestroy
		data.setWaitAndDestroy({40, 9.99f});

		return 0;
	}
*/
template<typename T> class Snapshot {
	public:		class Guard {
					public:		Guard(const Snapshot& s) : s(s), x(s.lock()) { }
					public:		Guard(const Guard& g) : s(g.s), x(g.s.lock()) { }
					public:		T& get() const { return x; } // FIX : phase out
					public:		T& access() const { return x; }
					public:		T& operator=(const T& copy) const { if (&x != &copy) { x = copy; }; return x; }
					public:		T* operator->() const { return &x; }
					public:		operator T&() const { return x; }
					public:		~Guard() { s.unlock(x); }
					protected:	const Snapshot& s;
					protected:	T& x;
				};

	public:		typedef Guard Lock; // FIX : phase out

	public:		Snapshot(const T& copy = T(), int capacity = 2) // \p capacity is the number of "slots" for the "snapshot pool". At minimum, it should be one greater to the number of threads that may concurrently write to the snapshot. If \p capacity is too small, you may experience unwanted busy-waiting on both reading and writing. (Min \p capacity is 2.)
						: slots(0), locks(0) {
					construct(capacity, const_cast<T&>(copy)); // Trick to allow default construction of std::auto_ptr which needs a non-const reference since it is changed. GCC is picky about non-const temporaries, so Snapshot(T& copy = T()) is not an option.
				}

	public:		template<class U> Snapshot(U& copy, int capacity = 2) // \p capacity is the number of "slots" for the "snapshot pool". At minimum, it should be one greater to the number of threads that may concurrently write to the snapshot. If \p capacity is too small, you may experience unwanted busy-waiting on both reading and writing. (Min \p capacity is 2.)
						: slots(0), locks(0) {
					construct(capacity, copy);
				}
	
	public:		Snapshot(const Snapshot<T>& copy) : slots(0), locks(0) {
					construct(copy.capacity, Guard(copy).get());
				}
				
	public:		template<class U> Snapshot(const Snapshot<U>& copy) : slots(0), locks(0) {
					construct(copy.capacity, Snapshot<U>::Guard(copy).get());
				}
				
	public:		template<class U> Snapshot(const Snapshot<U>& copy, int capacity) : slots(0), locks(0) {
					construct(capacity, Snapshot<U>::Guard(copy).get());
				}
				
	public:		void rescale(int newCapacity) { // rescaling is not a thread-safe operation, i.e. you should not call this method while accessing the snapshot from other threads.
					assert(newCapacity >= 2);
					volatile T* xSlots = 0;
					AtomicInt* xLocks = 0;
					try {
						// Assertion failures here probably indicates the snapshot is used by another thread concurrently, which is not legal right now.
						assert(locks[active] == 2);
						for (int i = 0; i < capacity; ++i) {
							assert(i == active || locks[i] == 0);
						}

						xSlots = reinterpret_cast<volatile T*>(operator new(sizeof (T) * newCapacity));
						xLocks = new AtomicInt[newCapacity];
						new (const_cast<T*>(&xSlots[0])) T(const_cast<T&>(slots[active])); // FIX : not deleted on catch
						xLocks[0] = 2;
						std::swap(slots, xSlots);
						std::swap(locks, xLocks);
						capacity = newCapacity;
						
						delete [] xLocks;
						xLocks = 0;
						const_cast<T*>(xSlots)[active].~T();
						operator delete(const_cast<T*>(xSlots));
						xSlots = 0;
						active = 0;
						last = 0;
					}
					catch (...) {
						delete [] xLocks;
						operator delete(const_cast<T*>(xSlots));
						throw;
					}
				}
				
	public:		template<class U> T swap(const U& x) {
					const T& y = exchange(x);
					T z(y);
					unlock(y);
					return z;
				}

	public:		Guard guard() const { return Guard(*this); }
	public:		Guard operator->() const { return Guard(*this); }
	public:		T get() const { return Guard(*this).access(); }
	public:		void set(const T& x) { unlock(exchange(x)); }
	public:		void setWaitAndDestroy(const T& x) { waitAndDestroy(exchange(x)); }
	public:		void setWaitAndDestroy(const Snapshot& x) { waitAndDestroy(exchange(x.guard())); }
	public:		Snapshot& operator=(const T& x) { unlock(exchange(x)); return *this; }
	public:		Snapshot& operator=(const Snapshot& copy) { unlock(exchange(copy.guard())); return *this; }
	public:		operator T() const { return Guard(*this).access(); }

	public:		template<class U> void set(const U& x) { unlock(exchange(x)); }
	public:		template<class U> void setWaitAndDestroy(const U& x) { waitAndDestroy(exchange(x)); }
	public:		template<class U> void setWaitAndDestroy(const Snapshot<U>& x) { waitAndDestroy(exchange(x.guard())); }
	public:		template<class U> Snapshot& operator=(const U& x) { unlock(exchange(x)); return *this; }
	public:		template<class U> Snapshot& operator=(const Snapshot<U>& copy) { unlock(exchange(copy.guard())); return *this; }

	public:		~Snapshot() {
					// Assertion failures here probably indicates the snapshot is used by another thread concurrently, which is not legal right now.
					assert(locks[active] == 2);
					for (int i = 0; i < capacity; ++i) {
						assert(i == active || locks[i] == 0);
					}
					delete [] locks;
					const_cast<T*>(slots)[active].~T();
					operator delete(const_cast<T*>(slots));
				}
				
	protected:	template<class U> void construct(int n, U& copy) {
					assert(n > 0);
					capacity = n;
					try {
						slots = reinterpret_cast<T*>(operator new(sizeof (T) * static_cast<size_t>(capacity)));
						locks = new AtomicInt[static_cast<int>(capacity)];
						new (const_cast<T*>(&slots[0])) T(copy);
						locks[0] = 2;
					}
					catch (...) {
						delete [] locks;
						operator delete(const_cast<T*>(slots));
						throw;
					}
				}

	protected:	T& lock() const {
					int activeSlot = active;
					int lockCount = locks[activeSlot];
					while (lockCount < 2 || !locks[activeSlot].swapIfEqual(lockCount, lockCount + 1)) {
					//	Thread::yield(); FIX : removed 2015-03-02 by Dimdals suggestion. Really, the swapIfEqual above should always work in just a few rounds. 
						activeSlot = active;
						lockCount = locks[activeSlot];
					}
					assert(0 <= activeSlot && activeSlot < capacity);
					return const_cast<T&>(slots[activeSlot]);
				}
	
	protected:	int allocate() {
					int firstSlot = last;
					firstSlot = ((firstSlot + 1) < capacity ? (firstSlot + 1) : 0);
					int slotIndex = firstSlot;
					assert(0 <= slotIndex && slotIndex < capacity);
					while (!locks[slotIndex].swapIfEqual(0, 1)) {
						slotIndex = ((slotIndex + 1) < capacity ? (slotIndex + 1) : 0);
						if (slotIndex == firstSlot) {
							Thread::yield();
						}
					}
					last = slotIndex;
					assert(0 <= slotIndex && slotIndex < capacity);
					return slotIndex;
				}

	protected:	template<class U> T& exchange(const U& x) {
					int slotIndex = allocate();
					new (const_cast<T*>(&slots[slotIndex])) T(x);
					locks[slotIndex] = 2;
					return const_cast<T&>(slots[active.swap(slotIndex)]);
				}
	
	protected:	void unlock(const T& x) const {
					assert(slots <= &x && &x < slots + capacity);
					ptrdiff_t unlockSlot = &x - const_cast<T*>(slots);
					assert(locks[unlockSlot] >= 2);
					if (--locks[unlockSlot] == 1) {
						const_cast<T*>(slots)[unlockSlot].~T();
						locks[unlockSlot] = 0;
					}
				}
				
	protected:	void waitAndDestroy(const volatile T& x) const { // FIX : cooler method name?
					assert(slots <= &x && &x < slots + capacity);
					ptrdiff_t unlockSlot = &x - slots;
					assert(locks[unlockSlot] >= 2);
					while (!locks[unlockSlot].swapIfEqual(2, 1)) {
						Thread::yield();
					}
					// 20140521 : const_cast<T*> is required to work around a bug in MSVC 2010 C++ compiler.
					const_cast<T*>(slots)[unlockSlot].~T();
					locks[unlockSlot] = 0;
				}
				
	protected:	AtomicInt capacity;
	protected:	volatile T* slots;
	protected:	AtomicInt* locks; // 0: free, 1: currently constructing, >=2: locked counter
	protected:	AtomicInt active; // Active element should always be considered locked. When changing, lock count should decremented.
	protected:	AtomicInt last;
};

// TODO : it would be cool in a single writer / reader situation to be able to access queue elements without copying them (if they are big), would this be possible?
template<typename T> class Queue { // FIX : name ConcurrentQueue, LockFreeQueue?
	public:		void swap(Queue& other) { // Not thread-safe.
					other.capacity = capacity.swap(other.capacity);
					std::swap(other.elements, elements);
					other.readBegin = readBegin.swap(other.readBegin);
					other.readEnd = readEnd.swap(other.readEnd);
					other.writeBegin = writeBegin.swap(other.writeBegin);
					other.writeEnd = writeEnd.swap(other.writeEnd);
				}

	public:		Queue(int capacity) : capacity(capacity)
					, elements(reinterpret_cast<volatile T*>(operator new(sizeof (T) * capacity))) {
					assert(capacity > 0 && (capacity & (capacity - 1)) == 0); // Capacity must be power of 2.
				}

	public:		Queue(const Queue& copy) : elements(0) { assign(copy, copy.capacity); }

	public:		Queue(const Queue& copy, int newCapacity) : elements(0) { assign(copy, newCapacity); }

	public:		Queue& operator=(const Queue& copy) { // Not thread-safe.
					assign(copy, copy.capacity);
					return (*this);
				}
				
	public:		void setCapacity(int capacity) { // Not thread-safe.
					assert(capacity > 0 && (capacity & (capacity - 1)) == 0); // Capacity must be power of 2.
					assign(*this, capacity);
				}
				
	public:		int getSize() const {
					assert(wrapToInt32(static_cast<unsigned int>(readEnd) - static_cast<unsigned int>(readBegin)) >= 0);
					return (readEnd - readBegin);
				}
				
				// Note: if several threads are pushing simultaneously, we need to sync the order of the pushing, and this may cause a little wait.
	public:		int push(int count, const T* x) {
					int e;
					while (true) {
						e = writeEnd;
						int maxCount = capacity - e + writeBegin;
						int c = (count < maxCount ? count : maxCount);
						if (c <= 0) {
							return 0;
						}
						if (writeEnd.swapIfEqual(e, e + c)) {
							for (int i = 0; i < c; ++i) {
								new (const_cast<T*>(&elements[(e + i) & (capacity - 1)])) T(x[i]);
							}
							while (!readEnd.swapIfEqual(e, e + c)) {
								Thread::yield();
							}
							return c;
						}
						Thread::yield();
					}
				}
				
				// Note: if several threads are popping simultaneously, we need to sync the order of the popping, and this may cause a little wait.
	public:		int pop(int count, T* x) {
					int b;
					while (true) {
						b = readBegin;
						int maxCount = readEnd - b;
						int c = (count < maxCount ? count : maxCount);
						if (c <= 0) {
							return 0;
						}
						if (readBegin.swapIfEqual(b, b + c)) {
							if (x != 0) {
								for (int i = 0; i < c; ++i) {
									x[i] = const_cast<T&>(elements[(b + i) & (capacity - 1)]);
								}
							}
							for (int i = 0; i < c; ++i) {
								// 20140521 : const_cast<T*> is required to work around a bug in MSVC 2010 C++ compiler.
								const_cast<T*>(elements)[(b + i) & (capacity - 1)].~T();
							}
							while (!writeBegin.swapIfEqual(b, b + c)) {
								Thread::yield();
							}
							return c;
						}
						Thread::yield();
					}
				}
					
	public:		T pop()	{
					T x;
					bool b = pop(x);
					(void)b;
					assert(b);
					return x;
				}
				
	public:		int getCapacity() const { return capacity; }
	public:		bool isEmpty() const { return getSize() == 0; }
	public:		bool push(const T& x) { return (push(1, &x) == 1); }
	public:		bool pop(T& x) { return (pop(1, &x) == 1); }
	public:		int skip(int count)	{ return pop(count, 0); }
	public:		bool skip() { return (skip(1) == 1); }
	public:		void clear() { skip(readEnd - readBegin); }

	public:		~Queue() {
					assert(readBegin == writeBegin);
					assert(readEnd == writeEnd);
					while (writeEnd != writeBegin) {
						// 20140521 : const_cast<T*> is required to work around a bug in MSVC 2010 C++ compiler.
						--writeEnd;
						const_cast<T*>(elements)[writeEnd & (capacity - 1)].~T();
					}
					operator delete(const_cast<T*>(elements));
				}

	protected:	void assign(const Queue& copy, int newCapacity) { // reserving is not a thread-safe operation, i.e. you should not call this method while accessing the queue from other threads.
					assert((newCapacity & (newCapacity - 1)) == 0); // Capacity must be power of 2.
					assert(newCapacity >= copy.getSize());
					// FIX : optimize, don't push one at a time
					Queue xQueue(newCapacity);
					for (int i = copy.readBegin; i < copy.readEnd; ++i) {
						xQueue.push(const_cast<T&>(copy.elements[i & (copy.capacity - 1)]));
					}
					swap(xQueue);
				}

	protected:	AtomicInt capacity;
	protected:	volatile T* elements;
	protected:	AtomicInt readBegin;
	protected:	AtomicInt readEnd;
	protected:	AtomicInt writeBegin;
	protected:	AtomicInt writeEnd;
};

} /* namespace NuX */

#endif
