/**
	IMPD is released under the BSD 2-Clause License.

	Copyright (c) 2013-2025, Magnus Lidström

	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
	following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
	disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
	disclaimer in the documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include "assert.h"
#include <iostream>
#include <fstream>
#include "src/IMPD.h"

using namespace IMPD;

class MyExecutor : public Executor {
	public:		virtual bool format(Interpreter& interpreter, const String& identifier, const std::vector<String>& uses
						, const std::vector<String>& requires) {
					for (std::vector<String>::const_iterator it = requires.begin(); it != requires.end(); ++it) {
						std::cout << *it << std::endl;
					}
					return requires.empty();
				}
	public:		virtual bool execute(Interpreter& interpreter, const String& instruction, const String& arguments) {
					if (instruction == "test") {
						std::vector<Argument> allArguments;
						std::map<String, String> labeledArguments;
						std::vector<String> indexedArguments;
						interpreter.parseArguments(arguments, allArguments);
						if (interpreter.mapArguments(allArguments, labeledArguments, indexedArguments) < 1) {
							interpreter.throwBadSyntax("Missing argument for 'test' instruction");
						}
						std::cout << "Test instruction" << std::endl;
						std::vector<String> list;
						interpreter.parseList(interpreter.expand(indexedArguments[0]), list, true, true, 0, 100000);
						for (std::vector<String>::const_iterator it = list.begin(); it != list.end(); ++it) {
							std::cout << *it << std::endl;
						}
						return true;
					}
					return false;
				}
	public:		virtual void trace(Interpreter& interpreter, const WideString& s) {
					String byteString(s.begin(), s.end());
					std::cout << byteString << std::endl;
				}
	public:		virtual bool load(Interpreter& interpreter, const WideString& filename, String& contents) {
					std::string filename8Bit(filename.begin(), filename.end());
					std::ifstream inStream(filename8Bit.c_str());
					if (!inStream.good()) return false;
					contents.assign(std::istreambuf_iterator<char>(inStream), std::istreambuf_iterator<char>());
					return inStream.good();
				}
	public:		virtual bool progress(Interpreter& interpreter, int maxStatementsLeft) {
					assert(maxStatementsLeft > 0);
					return true;
				}
};

static bool testUniStringConversions() {
	UniString sample;
	sample.push_back(static_cast<UniChar>('A'));
	sample.push_back(static_cast<UniChar>(0x20AC));
	sample.push_back(static_cast<UniChar>(0x1F600));
	WideString wide = convertUniToWideString(sample);
	UniString uni = convertWideToUniString(wide);
	if (uni != sample) return false;
	WideString wide2 = convertUniToWideString(uni);
	if (wide != wide2) return false;
	
	WideString wideSample = L"A\u20AC\U0001F600";
	UniString uni2 = convertWideToUniString(wideSample);
	WideString wide3 = convertUniToWideString(uni2);
	if (wideSample != wide3) return false;
	UniString uni3 = convertWideToUniString(wide3);
	if (uni2 != uni3) return false;
	return true;
}

int main(int argc, const char* argv[]) {
	MyExecutor myExecutor;
	STLMapVariables topVars;
	Interpreter imp(myExecutor, topVars);

	assert(testUniStringConversions());

	String s;
	String code;
	while (!std::cin.eof()) {
		getline(std::cin, s);
		if (s.empty()) {
			try {
				imp.run(code);
			}
			catch (const Exception& x) {
				std::cout << "Exception: " << x.what() << std::endl;
				if (x.hasStatement()) std::cout << "in statement: " << x.getStatement() << std::endl;
			}
			catch (const std::exception& x) {
				std::cout << "Exception: " << x.what() << std::endl;
			}
			catch (...) {
				std::cout << "General exception" << std::endl;
			}
			code.clear();
		} else {
			code += s;
			code += '\n';
		}
	}
	return 0;
}
