#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <sstream>

using namespace NuXPixels;

class XorshiftRandom2x32 {
	public:		static XorshiftRandom2x32 randomSeeded();
	public:		XorshiftRandom2x32(unsigned int seed0 = 123456789, unsigned int seed1 = 362436069);
	public:		void randomSeed();
	public:		unsigned int nextUnsignedInt() throw();
	public:		unsigned int nextUnsignedInt(unsigned int maxx) throw(); // Range [0,maxx]
	public:		double nextDouble() throw();
	public:		double operator()() throw();
	public:		float nextFloat() throw();
	public:		void setState(unsigned int x, unsigned int y) throw();
	public:		void getState(unsigned int& x, unsigned int& y) throw();
	protected:	unsigned int px;
	protected:	unsigned int py;
};
inline XorshiftRandom2x32::XorshiftRandom2x32(unsigned int seed0, unsigned int seed1) : px(seed0), py(seed1) { }
inline unsigned int XorshiftRandom2x32::nextUnsignedInt() throw() {
	unsigned int t = px ^ (px << 10);
	px = py;
	py = py ^ (py >> 13) ^ t ^ (t >> 10);
	return py;
}

// From MersenneTwister by by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus, Richard J. Wagner, Magnus Jonsson
inline unsigned int XorshiftRandom2x32::nextUnsignedInt(unsigned int maxx) throw() {
	unsigned int used = maxx;
	used |= used >> 1;
	used |= used >> 2;
	used |= used >> 4;
	used |= used >> 8;
	used |= used >> 16;
	
	unsigned int i;
	do {
		i = nextUnsignedInt() & used;
	} while (i > maxx);
	return i;
}

inline double XorshiftRandom2x32::nextDouble() throw() {
	nextUnsignedInt();
	return py * 2.3283064365386962890625e-10 + px * 5.42101086242752217003726400434970855712890625e-20;
}

inline double XorshiftRandom2x32::operator()() throw() {
	return nextDouble();
}

inline float XorshiftRandom2x32::nextFloat() throw() {
	return static_cast<float>(nextUnsignedInt() * 2.3283064365386962890625e-10);
}

inline void XorshiftRandom2x32::setState(unsigned int x, unsigned int y) throw() { px = x; py = y; }
inline void XorshiftRandom2x32::getState(unsigned int& x, unsigned int& y) throw() { x = px; y = py; }



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

static double randomDouble(XorshiftRandom2x32& prng, double min, double max)
{
	return min + (max - min) * prng.nextDouble();
}

static int randomInt(XorshiftRandom2x32& prng, int min, int max)
{
	return min + prng.nextUnsignedInt(max - min);
}

static void addRandomShape(XorshiftRandom2x32& prng, Path& path, std::vector<std::string>* log)
{
	int shape = randomInt(prng, 0, 3);
	double posX = randomDouble(prng, 0.0, 700.0);
	double posY = randomDouble(prng, 0.0, 500.0);
	double size = randomDouble(prng, 5.0, 150.0);
	switch (shape) {
	case 0:
	{
		double x = posX;
		double y = posY;
		double w = size;
		double h = randomDouble(prng, 5.0, 150.0);
		path.addRect(x, y, w, h);
		if (log) {
			std::ostringstream ss;
			ss << std::setprecision(20) << "path.addRect(" << x << ',' << y << ',' << w << ',' << h << ");";
			log->push_back(ss.str());
		}
		break;
	}
	case 1:
	{
		double x = posX;
		double y = posY;
		double w = size;
		double h = randomDouble(prng, 5.0, 150.0);
		double rx = randomDouble(prng, 5.0, 150.0) * 0.5;
		double ry = randomDouble(prng, 5.0, 150.0) * 0.5;
		path.addRoundedRect(x, y, w, h, rx, ry);
		if (log) {
			std::ostringstream ss;
			ss << std::setprecision(20) << "path.addRoundedRect(" << x << ',' << y << ',' << w << ',' << h << ',' << rx << ',' << ry << ");";
			log->push_back(ss.str());
		}
		break;
	}
	case 2:
	{
		int points = 3 + randomInt(prng, 0, 7);
		double x = posX;
		double y = posY;
		double outer = size;
		double inner = randomDouble(prng, 5.0, 150.0);
		path.addStar(x, y, points, outer, inner, 0);
		if (log) {
			std::ostringstream ss;
			ss << std::setprecision(20) << "path.addStar(" << x << ',' << y << ',' << points << ',' << outer << ',' << inner << ",0);";
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
			ss << std::setprecision(20) << "path.addCircle(" << x << ',' << y << ',' << r << ");";
			log->push_back(ss.str());
		}
		break;
	}
	}
}

static void buildRandomPath(XorshiftRandom2x32& prng, Path& path, std::vector<std::string>* log)
{
	int count = randomInt(prng, 10, 100);
	for (int i = 0; i < count; ++i) {
		addRandomShape(prng, path, log);
	}
	path.closeAll();
}

static void buildSeed1Iter3Path(Path& path)
{
path.addStar(97.493440-43,403.695645+82,3,62.663141,28.976358,0);
path.addCircle(487.772714-43,33.000087+82,114.542143);
path.closeAll();
}

static void buildSeed20834(Path& path) {
	// mask bounds: 108,13 - 747,566
	// mismatch at (556,298) baseline=254 test=255
	// span length mismatch (seed=20834, iter=0)
	std::cout << "building seed 20834 path\n";
	path.addStar(196.897827925578639,258.42424657122433018,4,102.53051554436353854,23.492460827572486437,0);
	path.addCircle(322.92343998463985599,195.89701560134861325,132.93095144114036543);
	path.addCircle(232.41463351641530721,137.67536456588439364,123.85715505753510968);
	path.addRoundedRect(508.38750568609100355,192.00576152280240194,16.841834984646101958,90.720586947035315006,67.952409411292705954,18.644975596640712467);
	path.addRect(499.75181161414445796,20.498427804791568008,9.9520733882449903263,149.49743623356215494);
	path.addCircle(651.4204433427287313,302.42232945860467908,95.506451123164254113);
	path.addCircle(661.96899952458636562,437.83929265934938257,81.847600448805650331);
	path.addRect(274.34635836367795036,28.032155953362654088,44.569081368189806369,72.550555166113440464);
	path.addRect(316.86408953595162075,453.39487909963116863,65.242577975263159829,112.00803024787829543);
	path.transform(AffineTransformation().translate(-108, -13));
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
		if (argc > 4) seed = unsigned(std::atoi(argv[4]));		bool dump = false;
		bool repro = false;
		for (int i = 5; i < argc; ++i) {
				if (!std::strcmp(argv[i], "dump")) dump = true;
				if (!std::strcmp(argv[i], "repro")) repro = true;
		}
		for (int i = 0; iterations == 0 || i < iterations; ++i) {
				unsigned iterSeed = seed + unsigned(i);
				XorshiftRandom2x32 prng(iterSeed);
				if ((i % 10000) == 0) {
					std::cout << i << std::endl;
				}
				Path path;
				std::vector<std::string> pathLog;
				if (repro) {
					buildSeed20834(path);
				} else if (iterations == 1 && argc <= 3) {
						path.addRoundedRect(0, 0, 700, 500, 80, 80);
						path.addStar(350, 350, 7, 300, 150, 0);
						path.addCircle(350, 350, 200);
						path.closeAll();
				} else {
						buildRandomPath(prng, path, dump ? &pathLog : 0);
				}
				PolygonMask mask(path);
				IntRect bounds = mask.calcBounds();
				if (iterations == 1) {
						std::cout << "mask bounds: " << bounds.left << "," << bounds.top << " - "
								<< bounds.calcRight() << "," << bounds.calcBottom() << "\n";
				}
				SelfContainedRaster<Mask8> big(bounds);
				renderRect(mask, bounds, bigSpan, big);
				PolygonMask mask2(path);
				SelfContainedRaster<Mask8> small(bounds);
				renderRect(mask2, bounds, smallSpan, small);
				if (!equals(big, small, bounds)) {
						std::cerr << "span length mismatch (seed=" << iterSeed << ", iter=" << i << ")\n";
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
