#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

/*
	Runs a libFuzzer target over files instead of under libFuzzer: every file named on the command line, and for a `-`
	every file named on a line of standard input (for corpora too large for a command line). This lets the build
	scripts compile the fuzz target with any compiler and replay the saved inputs through it.
	tools/buildIVGFuzz.sh and tools/buildIVGFuzz.cmd build the real fuzzer.
*/

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

static bool runFile(const std::string& path) {
	std::ifstream stream(path.c_str(), std::ios::binary);
	if (!stream) {
		fprintf(stderr, "Could not open %s\n", path.c_str());
		return false;
	}
	std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	const size_t size = bytes.size();
	bytes.push_back(0);																									// So that &bytes[0] is valid for an empty file.
	LLVMFuzzerTestOneInput(&bytes[0], size);
	return true;
}

int main(int argc, char** argv) {
	int count = 0;
	for (int i = 1; i < argc; ++i) {
		const std::string argument(argv[i]);
		if (argument == "-") {
			std::string line;
			while (std::getline(std::cin, line)) {
				if (!line.empty() && line[line.size() - 1] == '\r') {
					line.erase(line.size() - 1);
				}
				if (!line.empty()) {
					if (!runFile(line)) {
						return 1;
					}
					++count;
				}
			}
		} else {
			if (!runFile(argument)) {
				return 1;
			}
			++count;
		}
	}
	printf("Fuzz target ran on %d files\n", count);
	return 0;
}
