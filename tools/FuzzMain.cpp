#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

/*
	Runs a libFuzzer target over files instead of under libFuzzer: every file named on the command line. This lets the
	build scripts compile the fuzz target with any compiler and replay the saved inputs through it.
	tools/buildIVGFuzz.sh and tools/buildIVGFuzz.cmd build the real fuzzer.
*/

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		std::ifstream stream(argv[i], std::ios::binary);
		if (!stream) {
			fprintf(stderr, "Could not open %s\n", argv[i]);
			return 1;
		}
		std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		const size_t size = bytes.size();
		bytes.push_back(0);																								// So that &bytes[0] is valid for an empty file.
		LLVMFuzzerTestOneInput(&bytes[0], size);
	}
	printf("Fuzz target ran on %d files\n", argc - 1);
	return 0;
}
