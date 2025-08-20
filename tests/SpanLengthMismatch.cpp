#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <sstream>

using namespace NuXPixels;

static void renderRect(const PolygonMask& mask, const IntRect& rect, int spanLength, SelfContainedRaster<Mask8>& dest)
{
	Mask8::Pixel* pixels = dest.getPixelPointer();
	int stride = dest.getStride();
	int right = rect.calcRight();
	for (int y = rect.top; y < rect.calcBottom(); ++y) {
		for (int x = rect.left; x < right; x += spanLength) {
			int length = std::min(right - x, spanLength);
			NUXPIXELS_SPAN_ARRAY(Mask8, spanArray);
			SpanBuffer<Mask8> output(spanArray, pixels + y * stride + x);
			mask.render(x, y, length, output);
			Mask8::Pixel* target = pixels + y * stride + x;
			SpanBuffer<Mask8>::iterator it = output.begin();
			while (it != output.end()) {
				int count = it->getLength();
				if (it->isSolid()) {
					fillPixels<Mask8>(count, target, it->getSolidPixel());
				} else {
					copyPixels<Mask8>(count, target, it->getVariablePixels());
				}
				target += count;
				++it;
			}
		}
	}
}

static bool equals(const SelfContainedRaster<Mask8>& a, const SelfContainedRaster<Mask8>& b, const IntRect& rect)
{
	bool equal = true;
	const int strideA = a.getStride();
	const int strideB = b.getStride();
	const Mask8::Pixel* pixelsA = a.getPixelPointer();
	const Mask8::Pixel* pixelsB = b.getPixelPointer();
	for (int y = rect.top; y < rect.calcBottom(); ++y) {
		const Mask8::Pixel* rowA = pixelsA + y * strideA + rect.left;
		const Mask8::Pixel* rowB = pixelsB + y * strideB + rect.left;
		for (int x = 0; x < rect.width; ++x) {
			if (rowA[x] != rowB[x]) {
				std::cerr << "mismatch at (" << rect.left + x << "," << y << ") baseline="
					<< int(rowA[x]) << " test=" << int(rowB[x]) << "\n";
				equal = false;
			}
		}
	}
	return equal;
}

static double randomDouble(double min, double max)
{
	return min + (max - min) * (double(std::rand()) / double(RAND_MAX));
}

static int randomInt(int min, int max)
{
	return min + std::rand() % (max - min + 1);
}

static void addRandomShape(Path& path, std::vector<std::string>* log)
{
	int shape = randomInt(0, 3);
	double posX = randomDouble(0.0, 700.0);
	double posY = randomDouble(0.0, 500.0);
	double size = randomDouble(5.0, 150.0);
	switch (shape) {
	case 0:
	{
		double x = posX;
		double y = posY;
		double w = size;
		double h = randomDouble(5.0, 150.0);
		path.addRect(x, y, w, h);
		if (log) {
			std::ostringstream ss;
			ss << "path.addRect(" << x << ',' << y << ',' << w << ',' << h << ");";
			log->push_back(ss.str());
		}
		break;
	}
	case 1:
	{
		double x = posX;
		double y = posY;
		double w = size;
		double h = randomDouble(5.0, 150.0);
		double rx = randomDouble(5.0, 150.0) * 0.5;
		double ry = randomDouble(5.0, 150.0) * 0.5;
		path.addRoundedRect(x, y, w, h, rx, ry);
		if (log) {
			std::ostringstream ss;
			ss << "path.addRoundedRect(" << x << ',' << y << ',' << w << ',' << h << ',' << rx << ',' << ry << ");";
			log->push_back(ss.str());
		}
		break;
	}
	case 2:
	{
		int points = 3 + randomInt(0, 7);
		double x = posX;
		double y = posY;
		double outer = size;
		double inner = randomDouble(5.0, 150.0);
		path.addStar(x, y, points, outer, inner, 0);
		if (log) {
			std::ostringstream ss;
			ss << "path.addStar(" << x << ',' << y << ',' << points << ',' << outer << ',' << inner << ",0);";
			log->push_back(ss.str());
		}
		break;
	}
	case 3:
	{
		double x = posX;
		double y = posY;
		double r = size;
		path.addCircle(x, y, r);
		if (log) {
			std::ostringstream ss;
			ss << "path.addCircle(" << x << ',' << y << ',' << r << ");";
			log->push_back(ss.str());
		}
		break;
	}
	}
}

static void buildRandomPath(Path& path, std::vector<std::string>* log)
{
	int count = randomInt(1, 10);
	for (int i = 0; i < count; ++i) {
		addRandomShape(path, log);
	}
	path.closeAll();
}

static void buildSeed1Iter3Path(Path& path)
{
path.addStar(97.493440-43,403.695645+82,3,62.663141,28.976358,0);
path.addCircle(487.772714-43,33.000087+82,114.542143);
path.closeAll();
}

int main(int argc, char** argv)
{
	int bigSpan = 128;
	int smallSpan = 64;
	if (argc > 1) bigSpan = std::atoi(argv[1]);
	if (argc > 2) smallSpan = std::atoi(argv[2]);
		int iterations = 1;
		if (argc > 3) iterations = std::atoi(argv[3]);
		unsigned seed = unsigned(std::time(0));
		if (argc > 4) seed = unsigned(std::atoi(argv[4]));
		std::srand(seed);
		bool dump = false;
		bool repro = false;
		for (int i = 5; i < argc; ++i) {
				if (!std::strcmp(argv[i], "dump")) dump = true;
				if (!std::strcmp(argv[i], "repro")) repro = true;
		}
		for (int i = 0; iterations == 0 || i < iterations; ++i) {
				Path path;
				std::vector<std::string> pathLog;
				if (repro) {
						buildSeed1Iter3Path(path);
				} else if (iterations == 1 && argc <= 3) {
						path.addRoundedRect(0, 0, 700, 500, 80, 80);
						path.addStar(350, 350, 7, 300, 150, 0);
						path.addCircle(350, 350, 200);
						path.closeAll();
				} else {
						buildRandomPath(path, dump ? &pathLog : 0);
				}
				PolygonMask mask(path);
				IntRect bounds = mask.calcBounds();
				if (iterations == 1 && argc <= 3) {
						std::cout << "mask bounds: " << bounds.left << "," << bounds.top << " - "
								<< bounds.calcRight() << "," << bounds.calcBottom() << "\n";
				}
				SelfContainedRaster<Mask8> big(bounds);
				renderRect(mask, bounds, bigSpan, big);
				PolygonMask mask2(path);
				SelfContainedRaster<Mask8> small(bounds);
				renderRect(mask2, bounds, smallSpan, small);
				if (!equals(big, small, bounds)) {
						std::cerr << "span length mismatch (seed=" << seed << ", iter=" << i << ")\n";
						for (std::vector<std::string>::const_iterator it = pathLog.begin(); it != pathLog.end(); ++it)
								std::cerr << *it << '\n';
						return 1;
				}
				if (dump) {
						for (std::vector<std::string>::const_iterator it = pathLog.begin(); it != pathLog.end(); ++it)
								std::cout << *it << '\n';
				}
		}
		return 0;
}
