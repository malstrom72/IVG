// Rename this file to assert.h and place it in one of your project's include paths.

#ifndef assert_h_replacement_h
#define assert_h_replacement_h

#ifdef __cplusplus
	#include <NuX/NuXDebug.h>
#else
	#if defined (__GNUC__)
		#define NO_RETURN_ATTRIBUTE __attribute__((analyzer_noreturn));
	#else
		#define NO_RETURN_ATTRIBUTE
	#endif
	void assertFailureNoThrow(const char* assertion, const char* file, int line) NO_RETURN_ATTRIBUTE;
#endif

#undef assert

#if defined(NDEBUG)
	#define assert(a)
	#define assertNoThrow(a)
	#define assertMessage(a, m)
	#define assertMessageNoThrow(a, m)
#else
	#ifdef __cplusplus
		#define assert(a) do { if (!(a)) { NuXDebug::assertFailure(#a, __FILE__, __LINE__); } } while (false)
		#define assertMessage(a, m) do { if (!(a)) { NuXDebug::assertFailure(m, __FILE__, __LINE__); } } while (false)
	#else
		#define assert(a) do { if (!(a)) { assertFailureNoThrow(#a, __FILE__, __LINE__); } } while (false)
		#define assertMessage(a, m) do { if (!(a)) { assertFailureNoThrow(m, __FILE__, __LINE__); } } while (false)
	#endif
	#define assertNoThrow(a) do { if (!(a)) { assertFailureNoThrow(#a, __FILE__, __LINE__); } } while (false)
	#define assertMessageNoThrow(a, m) do { if (!(a)) { assertFailureNoThrow(m, __FILE__, __LINE__); } } while (false)
#endif

#endif
