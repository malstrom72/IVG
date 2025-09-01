#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "src/IVG.h"

using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;
namespace fs = std::filesystem;

static std::string readFile(const fs::path& p) {
	std::ifstream in(p, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string trim(const std::string& s) {
	std::string::size_type end = s.find_last_not_of("\r\n");
	return end == std::string::npos ? std::string() : s.substr(0, end + 1);
}

int main() {
	bool anyFail = false;
	for (const fs::directory_entry& entry : fs::directory_iterator("ivg/invalid")) {
		if (entry.path().extension() != ".ivg") continue;
		fs::path errPath = entry.path();
		errPath.replace_extension(".err");
		const std::string source = readFile(entry.path());
		const std::string expected = trim(readFile(errPath));
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
			std::cout << entry.path().stem().string() << ": did not throw" << std::endl;
			anyFail = true;
		} else if (message != expected) {
			std::cout << entry.path().stem().string() << ": expected \"" << expected
				<< "\" but got \"" << message << "\"" << std::endl;
			anyFail = true;
		} else {
			std::cout << entry.path().stem().string() << ": ok" << std::endl;
		}
	}
	return anyFail ? 1 : 0;
}
