/*
	Renders IVG documents and prints one line per document: its path, then either the error it stopped with or its
	bounds and a hash of its pixels. tools/replaySIMDDiff.sh and .cmd build it twice, with NUXPIXELS_SIMD set to 0 and
	to 1, run both over the same inputs and compare the output, since the two rasterizers must render alike. Documents
	are named on the command line, and for a `-` one per line of standard input, as for tools/FuzzMain.cpp.
*/
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include "src/IVG.h"

using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

struct DigestExecutor : public IVGExecutor {
	DigestExecutor(Canvas& canvas, IVGBudget& budget) : IVGExecutor(canvas, budget) { }
	virtual void trace(IMPD::Interpreter& interpreter, const IMPD::WideString& s) { (void)interpreter; (void)s; }
};

// 64-bit FNV-1a, enough to tell two renderings apart.
static void hashBytes(const void* data, size_t size, unsigned long long& hash) {
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i) {
		hash = (hash ^ bytes[i]) * 1099511628211ULL;
	}
}

static std::string digest(const std::string& source) {
	SelfContainedARGB32Canvas canvas;
	SelfContainedRaster<ARGB32>* raster = 0;
	try {
		{
			STLMapVariables topVars;
			IVGBudget budget;
			DigestExecutor executor(canvas, budget);
			FormatInfo formatInfo;
			Interpreter impd(executor, topVars, formatInfo, budget);
			impd.run(source);
		}
		raster = canvas.accessRaster();																					// Throws if the document declared no bounds.
	}
	catch (const IMPD::Exception& x) {
		return "error " + x.getError();
	}
	const IntRect bounds = raster->calcBounds();
	unsigned long long hash = 14695981039346656037ULL;
	const int stride = raster->getStride();
	const ARGB32::Pixel* pixels = raster->getPixelPointer() + bounds.top * stride + bounds.left;
	for (int y = 0; y < bounds.height; ++y) {
		hashBytes(pixels + y * stride, sizeof (ARGB32::Pixel) * bounds.width, hash);
	}
	char line[128];
	snprintf(line, sizeof (line), "%d,%d,%d,%d %016llx", bounds.left, bounds.top, bounds.width, bounds.height, hash);
	return line;
}

static bool printDigest(const std::string& path) {
	std::ifstream stream(path.c_str(), std::ios::binary);
	if (!stream) {
		fprintf(stderr, "Could not open %s\n", path.c_str());
		return false;
	}
	const std::string source((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	printf("%s\t%s\n", path.c_str(), digest(source).c_str());
	return true;
}

int main(int argc, char** argv) {
	for (int i = 1; i < argc; ++i) {
		const std::string argument(argv[i]);
		if (argument == "-") {
			std::string line;
			while (std::getline(std::cin, line)) {
				if (!line.empty() && line[line.size() - 1] == '\r') {
					line.erase(line.size() - 1);
				}
				if (!line.empty() && !printDigest(line)) {
					return 1;
				}
			}
		} else if (!printDigest(argument)) {
			return 1;
		}
	}
	return 0;
}
