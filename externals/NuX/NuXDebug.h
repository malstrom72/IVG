/**
	\file NuXDebug.h

	NuXDebug is a library for:
	
	** TODO **
	
	NuXFiles is part of the NuEdge X-Platform Library / NuX.
	Written by Magnus Lidstroem
	(C) NuEdge Development 2010
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

#ifndef NuXDebug_h
#define NuXDebug_h

#include <exception>
#include <string>

#ifndef NUX_DEBUG_INCLUDE_TESTS
#ifndef NDEBUG
#define NUX_DEBUG_INCLUDE_TESTS 1
#else
#define NUX_DEBUG_INCLUDE_TESTS 0
#endif
#endif

#if (NUX_DEBUG_INCLUDE_TESTS)
#define NUX_DEBUG_CAT(x, y) x ## y
#define NUX_DEBUG_CAT2(x, y) NUX_DEBUG_CAT(x, y)
#define REGISTER_UNIT_TEST(f) static bool NUX_DEBUG_CAT2(_registeredTest_, __LINE__) = NuXDebug::registerTest(#f, f);
#endif

#if defined (__GNUC__)
	#define NO_RETURN_ATTRIBUTE __attribute__((analyzer_noreturn))
#else
	#define NO_RETURN_ATTRIBUTE
#endif

namespace NuXDebug {

class Assert : public std::exception {
	public:		Assert(const std::string& assertion) : assertion(assertion) { };
	public:		virtual const char* what() const throw() { return assertion.c_str(); };
	public:		virtual ~Assert() throw() { };
	protected:	std::string assertion;
};

class Logger {
	public:		enum Level { TRACE_LEVEL = 0, WARNING_LEVEL = 1, ERROR_LEVEL = 2, ASSERT_LEVEL = 3 };
	public:		virtual void log(Level level, const std::string& text) throw() = 0;
};

class Hooks {
	public:		Hooks(Logger* logger);
	public:		virtual ~Hooks();
	public:		class Impl;
	protected:	Impl* impl;
};

#if (NUX_DEBUG_INCLUDE_TESTS)
bool registerTest(const char* name, bool (*function)());
bool runTests();
#endif

void assertFailure(const char* assertion, const char* file, int line) NO_RETURN_ATTRIBUTE;
void trace(const std::string& text) throw();
void warning(const std::string& text) throw();
void error(const std::string& text) throw();

}

extern "C" void assertFailureNoThrow(const char* assertion, const char* file, int line) throw() NO_RETURN_ATTRIBUTE;

#endif
