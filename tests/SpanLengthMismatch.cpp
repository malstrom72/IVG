#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cstring>

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

static void addRandomShape(Path& path, std::mt19937& rng, std::vector<std::string>* log)
{
	std::uniform_int_distribution<int> shapeDist(0, 3);
	std::uniform_real_distribution<double> posX(0.0, 700.0);
	std::uniform_real_distribution<double> posY(0.0, 500.0);
	std::uniform_real_distribution<double> size(5.0, 150.0);
        switch (shapeDist(rng)) {
        case 0:
        {
                double x = posX(rng);
                double y = posY(rng);
                double w = size(rng);
                double h = size(rng);
                path.addRect(x, y, w, h);
                if (log) log->push_back("path.addRect(" + std::to_string(x) + "," + std::to_string(y) + "," +
                                std::to_string(w) + "," + std::to_string(h) + ");");
                break;
        }
        case 1:
        {
                double x = posX(rng);
                double y = posY(rng);
                double w = size(rng);
                double h = size(rng);
                double rx = size(rng) * 0.5;
                double ry = size(rng) * 0.5;
                path.addRoundedRect(x, y, w, h, rx, ry);
                if (log) log->push_back("path.addRoundedRect(" + std::to_string(x) + "," + std::to_string(y) + "," +
                                std::to_string(w) + "," + std::to_string(h) + "," + std::to_string(rx) + "," +
                                std::to_string(ry) + ");");
                break;
        }
        case 2:
        {
                int points = 3 + rng() % 8;
                double x = posX(rng);
                double y = posY(rng);
                double outer = size(rng);
                double inner = size(rng);
                path.addStar(x, y, points, outer, inner, 0);
                if (log) log->push_back("path.addStar(" + std::to_string(x) + "," + std::to_string(y) + "," +
                                std::to_string(points) + "," + std::to_string(outer) + "," +
                                std::to_string(inner) + ",0);");
                break;
        }
        case 3:
        {
                double x = posX(rng);
                double y = posY(rng);
                double r = size(rng);
                path.addCircle(x, y, r);
                if (log) log->push_back("path.addCircle(" + std::to_string(x) + "," + std::to_string(y) + "," +
                                std::to_string(r) + ");");
                break;
        }
        }
}

static void buildRandomPath(Path& path, std::mt19937& rng, std::vector<std::string>* log)
{
        std::uniform_int_distribution<int> countDist(1, 10);
        int count = countDist(rng);
        for (int i = 0; i < count; ++i) {
                addRandomShape(path, rng, log);
        }
        path.closeAll();
}

static void buildSeed1Iter3Path(Path& path)
{
path.addStar(97.493440,403.695645,3,62.663141,28.976358,0);
path.addCircle(487.772714,33.000087,114.542143);
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
        unsigned seed = unsigned(std::random_device{}());
        if (argc > 4) seed = unsigned(std::atoi(argv[4]));
        bool dump = false;
        bool repro = false;
        for (int i = 5; i < argc; ++i) {
                if (!std::strcmp(argv[i], "dump")) dump = true;
                if (!std::strcmp(argv[i], "repro")) repro = true;
        }
        std::mt19937 rng(seed);
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
                        buildRandomPath(path, rng, dump ? &pathLog : nullptr);
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
                        for (const auto& s : pathLog) std::cerr << s << '\n';
                        return 1;
                }
                if (dump) {
                        for (const auto& s : pathLog) std::cout << s << '\n';
                }
        }
        return 0;
}
