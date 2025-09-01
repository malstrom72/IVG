#include <fstream>
#include <iostream>
#include <string>
#include "src/IVG.h"

using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

static std::string readFile(const std::string& p) {
	std::ifstream in(p.c_str(), std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string trim(const std::string& s) {
	std::string::size_type end = s.find_last_not_of("\r\n");
	return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

static std::string stem(const std::string& path) {
	std::string::size_type slash = path.find_last_of("/\\");
	std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
	if (name.size() > 4 && name.substr(name.size() - 4) == ".ivg") name.resize(name.size() - 4);
	return name;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "usage: InvalidIVGTest <file>\n";
		return 1;
	}
	const std::string path = argv[1];
	std::string errPath = path;
	if (errPath.size() > 4) errPath.replace(errPath.size() - 4, 4, ".err");
	const std::string source = readFile(path);
	const std::string expected = trim(readFile(errPath));
	std::cout << "Testing " << stem(path) << ": expecting \"" << expected << "\" ... ";
	std::string message;
	bool threw = false;
	try {
		SelfContainedARGB32Canvas canvas;
		STLMapVariables vars;
		IVGExecutor exec(canvas);
		Interpreter imp(exec, vars);
		imp.run(source);
	}
	catch (const SyntaxException& x) { message = x.what(); threw = true; }
	catch (const FormatException& x) { message = x.what(); threw = true; }
	catch (const Exception& x) { message = x.what(); threw = true; }
	catch (const std::exception& x) { message = x.what(); threw = true; }
	if (!threw) {
		std::cout << "FAIL (did not throw)" << std::endl;
		return 1;
	} else if (message != expected) {
		std::cout << "FAIL (got \"" << message << "\")" << std::endl;
		return 1;
	} else {
		std::cout << "PASS" << std::endl;
		return 0;
	}
}
