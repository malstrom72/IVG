/*
 *  NuXDebugCarbon.cpp
 *  Cockatoo
 *
 *  Created by Magnus Lidström on 5/24/07.
 *  Copyright 2007 NuEdge Development. All rights reserved.
 *
 */

#include <Carbon/Carbon.h>
#include <exception>
#include <iostream>
#include "NuXDebug.h"
#include "assert_h_replacement.h"

namespace NuXDebug {

class Hooks::Impl {
	public:		Impl(Logger* logger) {
					assertMessage(!isRegistered, "Only a single instance of NuXDebug::Hooks is allowed");
					isRegistered = true;
					Impl::logger = logger;
					previousTerminateHandler = std::set_terminate(myTerminateHandler);
					previousUnexpectedHandler = std::set_unexpected(myUnexpectedHandler);
				}
				
	public:		~Impl() {
					std::set_unexpected(previousUnexpectedHandler);
					std::set_terminate(previousTerminateHandler);
					Impl::logger = 0;
					isRegistered = false;
				}
				
	protected:	static void myTerminateHandler() { assertMessageNoThrow(0, "Unhandled exception"); }
	protected:	static void myUnexpectedHandler() { assertMessageNoThrow(0, "Unexpected exception thrown"); }

	public:		static void log(Logger::Level level, const std::string& text) throw() {
					std::cerr << text << std::endl << std::flush;
					if (logger != 0) {
						logger->log(level, text);
					}
				}
				
	public:		static void assertFailure(const char* assertionString, const char* fileName, int lineNumber) {
					std::string logMessage = "Assertion failure: ";
					logMessage += assertionString;
					std::string alertMessage = logMessage + "\n\n";
					if (fileName != 0 && fileName[0] != '\0') {
						logMessage += std::string(", file: ") + fileName;
						alertMessage += std::string("File: ") + fileName + "\n\n";
					}
					if (lineNumber != 0) {
						char buf[256];
						sprintf(buf, ", line: %d", lineNumber);
						logMessage += buf;
						sprintf(buf, "Line: %d\n\n", lineNumber);
						alertMessage += buf;
					}
					std::cerr << logMessage << std::endl;
					if (logger != 0) {
						logger->log(Logger::ASSERT_LEVEL, logMessage);
					}
					alertMessage += "Please, report this error to the developer.";
		
					if (getenv("NUX_NO_ASSERT_WINDOWS") != 0) {
						abort();
					} else if (!isShowingAlert) {
						try {
							isShowingAlert = true;
							
							::CFOptionFlags response = kCFUserNotificationDefaultResponse;
							::CFStringRef alertMessageCFString
									= ::CFStringCreateWithCString(kCFAllocatorDefault, alertMessage.c_str()
									, kCFStringEncodingISOLatin1);
							::CFUserNotificationDisplayAlert(2.0 * 60.0, kCFUserNotificationStopAlertLevel
									, NULL, NULL, NULL, CFSTR("Assertion failure"), alertMessageCFString
									, CFSTR("Ignore"), CFSTR("Terminate"), NULL, &response);
							if (alertMessageCFString != NULL) {
								::CFRelease(alertMessageCFString);
								alertMessageCFString = NULL;
							}

							switch (response) {
								case kCFUserNotificationDefaultResponse: break;
								case kCFUserNotificationCancelResponse:
								case kCFUserNotificationAlternateResponse: abort(); break;
							}
						}
						catch (...) {
							abort();
						}
					}
					isShowingAlert = false;
				}
				
	protected:	static bool isShowingAlert;
	protected:	static bool isRegistered;
	protected:	static Logger* logger;
	protected:	std::terminate_handler previousTerminateHandler;
	protected:	std::unexpected_handler previousUnexpectedHandler;
};

bool Hooks::Impl::isShowingAlert = false;
bool Hooks::Impl::isRegistered = false;
Logger* Hooks::Impl::logger = 0;

Hooks::Hooks(Logger* logger) : impl(new Impl(logger)) { }
Hooks::~Hooks() { delete impl; }

void assertFailure(const char* s, const char* f, int l) {
	assertFailureNoThrow(s, f, l);
	if (!std::uncaught_exception()) {
		throw Assert(std::string("Assertion failure: ") += s);
	}
}

void trace(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::TRACE_LEVEL, text); }
void warning(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::WARNING_LEVEL, text); }
void error(const std::string& text) throw() { NuXDebug::Hooks::Impl::log(Logger::ERROR_LEVEL, text); }

} /* namespace NuXDebug */

extern "C" void assertFailureNoThrow(const char* s, const char* f, int l) throw() {
	NuXDebug::Hooks::Impl::assertFailure(s, f, l);
}
