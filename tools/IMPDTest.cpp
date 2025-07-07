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
						interpreter.parseList(interpreter.expand(indexedArguments[0]), list, true, true);
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

int main(int argc, const char* argv[]) {
	MyExecutor myExecutor;
	STLMapVariables topVars;
	Interpreter imp(myExecutor, topVars);
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
