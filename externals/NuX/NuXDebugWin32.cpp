/*
 *  NuXDebugWin32.cpp
 *  Cockatoo
 *
 *  Created by Magnus Lidstr� on 5/24/07.
 *  Copyright 2007 NuEdge Development. All rights reserved.
 *
 */

#if !defined(NDEBUG)

#include <windows.h>
#include <crtdbg.h>
#include <exception>
#include <string>
#include "NuXDebug.h"
#include "assert_h_replacement.h"

namespace NuXDebug {

template<typename T, typename U> T lossless_cast(U x) { assert(static_cast<T>(x) == x); return static_cast<T>(x); }

static std::string convertWideToMultiByteString(const std::wstring& w, ::UINT codePage) {
	if (w.empty()) return std::string();
	else {
		assert(sizeof (wchar_t) == 2);
		int wideCharToMultiByteReturn = ::WideCharToMultiByte(codePage, WC_COMPOSITECHECK, w.data(), lossless_cast<int>(w.size())
				, 0, 0, 0, 0);
		assert(wideCharToMultiByteReturn != 0);
		std::string s;
		s.resize(wideCharToMultiByteReturn);
		int wideCharToMultiByteReturnAgain = ::WideCharToMultiByte(codePage, WC_COMPOSITECHECK, w.data(), lossless_cast<int>(w.size())
				, &s[0], lossless_cast<int>(s.size()), 0, 0);
		assert(wideCharToMultiByteReturnAgain == wideCharToMultiByteReturn);
		return s;
	}
}

static std::string convertWideToByteString(const std::wstring& w) { return convertWideToMultiByteString(w, 28591); }

class Hooks::Impl {
	public:		Impl(Logger* logger) {
					assertMessage(!isRegistered, "Only a single instance of NuXDebug::Hooks is allowed");
					isRegistered = true;
					Impl::logger = logger;
					if (logger != 0) {
						int count = _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, myReportHook);
						assert(count == 1);
					}
					previousTerminateHandler = std::set_terminate(myTerminateHandler);
					previousUnexpectedHandler = std::set_unexpected(myUnexpectedHandler);
					previousInvalidParameterHandler = _set_invalid_parameter_handler(myInvalidParameterHandler);
				}
				
	public:		~Impl() {
					_set_invalid_parameter_handler(previousInvalidParameterHandler);
					std::set_unexpected(previousUnexpectedHandler);
					std::set_terminate(previousTerminateHandler);
					if (logger != 0) {
						int count = _CrtSetReportHook2(_CRT_RPTHOOK_REMOVE, myReportHook);
						assert(count == 0);
						logger = 0;
					}
					isRegistered = false;
				}
				
	protected:	static void myTerminateHandler() { assertMessageNoThrow(0, "Unhandled exception"); }
	protected:	static void myUnexpectedHandler() { assertMessageNoThrow(0, "Unexpected exception thrown"); }
	
	protected:	static void myInvalidParameterHandler(const wchar_t * expression, const wchar_t * function
						, const wchar_t * file, unsigned int line, uintptr_t pReserved) {
					std::string expressionByteString(convertWideToByteString(expression));
					std::string fileByteString(convertWideToByteString(file));
					assertFailure(expressionByteString.c_str(), fileByteString.c_str(), line);
				}

	public:		static void log(Logger::Level level, const std::string& text) throw() {
					::OutputDebugStringA((text + "\n").c_str());
					if (logger != 0) {
						logger->log(level, text);
					}
				}
				
	protected:	static int myReportHook(int reportType, char* userMessage, int* returnValue) {
					assert(logger != 0);
					*returnValue = 0;
					std::string message = userMessage;
					while (!message.empty() && (message[message.size() - 1] == '\r'
							|| message[message.size() - 1] == '\n')) {
						message.resize(message.size() - 1);
					}
					Logger::Level level;
					switch (reportType) {
						case _CRT_WARN: level = Logger::WARNING_LEVEL; break;
						case _CRT_ERROR: level = Logger::ERROR_LEVEL; break;
						case _CRT_ASSERT: level = Logger::ASSERT_LEVEL; break;
						default: assert(0);
					}
					logger->log(level, message);
					return 0;
				}
				
	protected:	static bool isRegistered;
	protected:	static Logger* logger;
	protected:	std::terminate_handler previousTerminateHandler;
	protected:	std::unexpected_handler previousUnexpectedHandler;
	protected:	_invalid_parameter_handler previousInvalidParameterHandler;
};

bool Hooks::Impl::isRegistered = false;
Logger* Hooks::Impl::logger = 0;

Hooks::Hooks(Logger* logger) : impl(new Impl(logger)) { }
Hooks::~Hooks() { delete impl; }

void assertFailure(const char* s, const char* f, int l) {
	assertFailureNoThrow(s, f, l);
	if (!std::uncaught_exception()) throw Assert(std::string("Assertion failure: ") += s);
}

void trace(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::TRACE_LEVEL, text); }
void warning(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::WARNING_LEVEL, text); }
void error(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::ERROR_LEVEL, text); }

static void configureCrtReport(bool& windowless) {
	windowless = (getenv("NUX_NO_ASSERT_WINDOWS") != 0);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	const int newMode = (windowless ? _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE
			: _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW);
	_CrtSetReportMode(_CRT_ASSERT, newMode);
	_CrtSetReportMode(_CRT_ERROR, newMode);
}

struct CrtReportConfigurer {
	CrtReportConfigurer() { bool dummy; configureCrtReport(dummy); }
};

static CrtReportConfigurer crtReportConfigurer;

} /* namespace NuXDebug */

extern "C" void assertFailureNoThrow(const char* s, const char* f, int l) throw() {
	bool windowless;
	NuXDebug::configureCrtReport(windowless);
	if (_CrtDbgReport(_CRT_ASSERT, f, l, NULL, s) == 1) {
		_CrtDbgBreak();
	} else if (windowless) {
		abort();
	}
}

#endif
