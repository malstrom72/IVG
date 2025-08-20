#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"
#include <algorithm>
#include <iostream>

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

int main()
{
	Path path;
	path.addRoundedRect(0, 0, 700, 500, 80, 80);
	path.addStar(350, 350, 7, 300, 150, 0);
	path.addCircle(350, 350, 200);
	path.closeAll();

	PolygonMask mask(path);
	IntRect bounds = mask.calcBounds();
	std::cout << "mask bounds: " << bounds.left << "," << bounds.top << " - "
		<< bounds.calcRight() << "," << bounds.calcBottom() << "\n";

	SelfContainedRaster<Mask8> big(bounds);
	renderRect(mask, bounds, 128, big);

	PolygonMask mask2(path);
	SelfContainedRaster<Mask8> small(bounds);
	renderRect(mask2, bounds, 64, small);

	if (!equals(big, small, bounds)) {
		std::cerr << "span length mismatch\n";
		return 1;
	}
	return 0;
}
