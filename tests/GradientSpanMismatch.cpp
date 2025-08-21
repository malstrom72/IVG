#include <cstdlib>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"

using namespace NuXPixels;

class XorshiftRandom2x32 {
	public:	static XorshiftRandom2x32 randomSeeded();
	public:	XorshiftRandom2x32(unsigned int seed0 = 123456789, unsigned int seed1 = 362436069);
	public:	void randomSeed();
	public:	unsigned int nextUnsignedInt() throw();
	public:	unsigned int nextUnsignedInt(unsigned int maxx) throw();
	public:	double nextDouble() throw();
	public:	double operator()() throw();
	public:	float nextFloat() throw();
	public:	void setState(unsigned int x, unsigned int y) throw();
	public:	void getState(unsigned int& x, unsigned int& y) throw();
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
inline double XorshiftRandom2x32::operator()() throw() { return nextDouble(); }
inline float XorshiftRandom2x32::nextFloat() throw() { return static_cast<float>(nextUnsignedInt() * 2.3283064365386962890625e-10); }
inline void XorshiftRandom2x32::setState(unsigned int x, unsigned int y) throw() { px = x; py = y; }
inline void XorshiftRandom2x32::getState(unsigned int& x, unsigned int& y) throw() { x = px; y = py; }

static double randomDouble(XorshiftRandom2x32& prng, double min, double max)
{
	return min + (max - min) * prng.nextDouble();
}
static int randomInt(XorshiftRandom2x32& prng, int min, int max)
{
	return min + int(prng.nextUnsignedInt(unsigned(max - min)));
}
static ARGB32::Pixel randomColor(XorshiftRandom2x32& prng)
{
	return 0xFF000000 | prng.nextUnsignedInt(0x00FFFFFF);
}

template<class T> static void renderRect(const Renderer<T>& renderer, const IntRect& rect, int spanLength, SelfContainedRaster<T>& dest)
{
	typename T::Pixel* pixels = dest.getPixelPointer();
	int stride = dest.getStride();
	int right = rect.calcRight();
	for (int y = rect.top; y < rect.calcBottom(); ++y) {
		for (int x = rect.left; x < right; x += spanLength) {
			int length = std::min(right - x, spanLength);
			NUXPIXELS_SPAN_ARRAY(T, spanArray);
			SpanBuffer<T> output(spanArray, pixels + y * stride + x);
			renderer.render(x, y, length, output);
			typename T::Pixel* target = pixels + y * stride + x;
			typename SpanBuffer<T>::iterator it = output.begin();
			while (it != output.end()) {
				int count = it->getLength();
				if (it->isSolid()) {
					fillPixels<T>(count, target, it->getSolidPixel());
				} else {
					copyPixels<T>(count, target, it->getVariablePixels());
				}
				target += count;
				++it;
			}
		}
	}
}

template<class T> static bool equals(const SelfContainedRaster<T>& a, const SelfContainedRaster<T>& b, const IntRect& rect)
{
	bool equal = true;
	int strideA = a.getStride();
	int strideB = b.getStride();
	const typename T::Pixel* pixelsA = a.getPixelPointer();
	const typename T::Pixel* pixelsB = b.getPixelPointer();
	for (int y = rect.top; y < rect.calcBottom(); ++y) {
		const typename T::Pixel* rowA = pixelsA + y * strideA + rect.left;
		const typename T::Pixel* rowB = pixelsB + y * strideB + rect.left;
		for (int x = 0; x < rect.width; ++x) {
			if (rowA[x] != rowB[x]) {
				std::cerr << "mismatch at (" << rect.left + x << "," << y << ") baseline=" << std::hex << rowA[x] << " test=" << rowB[x] << std::dec << "\n";
				equal = false;
			}
		}
	}
	return equal;
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
	for (int i = 0; iterations == 0 || i < iterations; ++i) {
		unsigned iterSeed = seed + unsigned(i);
		XorshiftRandom2x32 prng(iterSeed);
		Gradient<ARGB32>::Stop stops[2];
		stops[0].position = 0.0;
		stops[0].color = randomColor(prng);
		stops[1].position = 1.0;
		stops[1].color = randomColor(prng);
		Gradient<ARGB32> grad(2, stops);
		IntRect bounds(0, 0, 256, 256);
		if (randomInt(prng, 0, 1) == 0) {
			double x0 = randomDouble(prng, -50.0, 300.0);
			double y0 = randomDouble(prng, -50.0, 300.0);
			double x1 = randomDouble(prng, -50.0, 300.0);
			double y1 = randomDouble(prng, -50.0, 300.0);
			Lookup<ARGB32, LookupTable<ARGB32> > renderer = grad[LinearAscend(x0, y0, x1, y1)];
			SelfContainedRaster<ARGB32> big(bounds);
			SelfContainedRaster<ARGB32> small(bounds);
			renderRect(renderer, bounds, bigSpan, big);
			renderRect(renderer, bounds, smallSpan, small);
			if (!equals(big, small, bounds)) {
				std::cerr << "span length mismatch (seed=" << iterSeed << ", iter=" << i << ") linear gradient\n";
				return 1;
			}
		} else {
			double cx = randomDouble(prng, 0.0, 256.0);
			double cy = randomDouble(prng, 0.0, 256.0);
			double rx = randomDouble(prng, 5.0, 200.0);
			double ry = randomDouble(prng, 5.0, 200.0);
			Lookup<ARGB32, LookupTable<ARGB32> > renderer = grad[RadialAscend(cx, cy, rx, ry)];
			SelfContainedRaster<ARGB32> big(bounds);
			SelfContainedRaster<ARGB32> small(bounds);
			renderRect(renderer, bounds, bigSpan, big);
			renderRect(renderer, bounds, smallSpan, small);
			if (!equals(big, small, bounds)) {
				std::cerr << "span length mismatch (seed=" << iterSeed << ", iter=" << i << ") radial gradient\n";
				return 1;
			}
		}
	}
	return 0;
}
