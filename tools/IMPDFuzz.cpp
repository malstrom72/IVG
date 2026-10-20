/*
	Fuzz target for the IMPD interpreter alone: parsing, control flow, variables, expressions and argument expansion.
	The executor accepts every instruction and expands every argument, so inputs reach as much of IMPD as possible
	instead of stopping at an unknown instruction, while nothing is drawn. Built by tools/buildIMPDFuzz.sh and .cmd.
*/
#include <cstddef>
#include <string>
#include <vector>
#include "src/IMPD.h"

using namespace IMPD;

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <crtdbg.h>

/*
	On Windows a stack overflow leaves no stack for AddressSanitizer and libFuzzer to report it on, so the input that
	caused it is never saved. Reserving some stack for the overflow handler lets libFuzzer write the crash file.
	Failed asserts, abort() and crashes report to stderr instead of opening dialogs that would stall an unattended run.
*/
extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
	ULONG guarantee = 256 * 1024;
	SetThreadStackGuarantee(&guarantee);
	_set_error_mode(_OUT_TO_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	return 0;
}
#endif

class AcceptingExecutor : public Executor {
	public:		virtual bool format(Interpreter& interpreter, const FormatInfo& formatInfo) {
					(void)interpreter;
					(void)formatInfo;
					return true;
				}
	public:		virtual bool execute(Interpreter& interpreter, const String& instruction, const String& arguments) {
					(void)instruction;
					std::vector<Argument> allArguments;
					std::map<String, String> labeledArguments;
					std::vector<String> indexedArguments;
					interpreter.parseArguments(arguments, allArguments);
					interpreter.mapArguments(allArguments, labeledArguments, indexedArguments);
					for (size_t i = 0; i < indexedArguments.size(); ++i) {
						std::vector<String> list;
						interpreter.parseList(interpreter.expand(indexedArguments[i]), list, true, true);
					}
					for (std::map<String, String>::const_iterator it = labeledArguments.begin()
							; it != labeledArguments.end(); ++it) {
						interpreter.expand(it->second);
					}
					return true;
				}
	public:		virtual void trace(Interpreter& interpreter, const WideString& s) {
					(void)interpreter;
					(void)s;
				}
	public:		virtual bool meta(Interpreter& interpreter, const String& key, const String& arguments) {
					(void)interpreter;
					(void)key;
					(void)arguments;
					return true;
				}
	public:		virtual bool load(Interpreter& interpreter, const WideString& filename, String& contents) {
					(void)interpreter;
					(void)filename;
					(void)contents;
					return false;
				}
	public:		virtual bool progress(Interpreter& interpreter, int maxStatementsLeft) {
					(void)interpreter;
					(void)maxStatementsLeft;
					return true;
				}
};

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
	const std::string source(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size);
	try {
		AcceptingExecutor executor;
		STLMapVariables topVars;
		FormatInfo formatInfo;
		IMPDBudget budget;
		Interpreter impd(executor, topVars, formatInfo, budget);
		impd.run(source);
	}
	catch (const IMPD::Exception&) {
	}
	return 0;
}
