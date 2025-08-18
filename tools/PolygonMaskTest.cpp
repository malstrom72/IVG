/**
IVG is released under the BSD 2-Clause License.

Copyright (c) 2013-2025, Magnus Lidström

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <cmath>
#include "externals/NuX/NuXPixels.h"
#include "externals/NuX/NuXPixelsImpl.h"
#include <algorithm>
#include <iostream>

using namespace NuXPixels;

static void renderRect(const PolygonMask& mask, const IntRect& rect, SelfContainedRaster<Mask8>& dest)
{
Mask8::Pixel* pixels = dest.getPixelPointer();
	int stride = dest.getStride();
	int right = rect.calcRight();
	for (int y = rect.top; y < rect.calcBottom(); ++y) {
		for (int x = rect.left; x < right; x += MAX_RENDER_LENGTH) {
			int length = std::min(right - x, MAX_RENDER_LENGTH);
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

static bool equals(const SelfContainedRaster<Mask8>& a, const SelfContainedRaster<Mask8>& b, const IntRect& rect, const char* label)
{
int strideA = a.getStride();
int strideB = b.getStride();
const Mask8::Pixel* pixelsA = a.getPixelPointer();
const Mask8::Pixel* pixelsB = b.getPixelPointer();
for (int y = rect.top; y < rect.calcBottom(); ++y) {
const Mask8::Pixel* rowA = pixelsA + y * strideA + rect.left;
const Mask8::Pixel* rowB = pixelsB + y * strideB + rect.left;
for (int x = 0; x < rect.width; ++x) {
if (rowA[x] != rowB[x]) {
std::cerr << label << " mismatch at (" << rect.left + x << "," << y << ") baseline="
<< int(rowA[x]) << " test=" << int(rowB[x]) << "\n";
return false;
}
}
}
return true;
}

int main()
{
	Path path;
	path.addRoundedRect(50, 50, 700, 500, 80, 80);
	path.addStar(400, 300, 7, 300, 150, 0);
	path.addCircle(400, 300, 200);
	path.closeAll();

PolygonMask mask(path);
IntRect bounds = mask.calcBounds();
std::cerr << "bounds left=" << bounds.left << " top=" << bounds.top << " width=" << bounds.width
<< " height=" << bounds.height << "\n";

	SelfContainedRaster<Mask8> baseline(bounds);
	renderRect(mask, bounds, baseline);

	PolygonMask bounded(path, bounds);
	SelfContainedRaster<Mask8> boundedRaster(bounds);
	renderRect(bounded, bounds, boundedRaster);
if (!equals(baseline, boundedRaster, bounds, "calcBounds")) {
std::cerr << "calcBounds render mismatch\n";
return 1;
}

	IntRect clip(150, 75, 200, 100);
	PolygonMask clipped(path, clip);
	SelfContainedRaster<Mask8> clippedRaster(clip);
	renderRect(clipped, clip, clippedRaster);
if (!equals(baseline, clippedRaster, clip, "clip")) {
std::cerr << "clip render mismatch\n";
return 1;
}

SelfContainedRaster<Mask8> silly(bounds);
int midX = bounds.left + bounds.width / 2;
int midY = bounds.top + bounds.height / 2;
std::cerr << "midX=" << midX << " midY=" << midY << "\n";
	IntRect bottomRight(midX, midY, bounds.calcRight() - midX, bounds.calcBottom() - midY);
	IntRect topLeft(bounds.left, bounds.top, midX - bounds.left, midY - bounds.top);
	IntRect bottomLeft(bounds.left, midY, midX - bounds.left, bounds.calcBottom() - midY);
	IntRect topRight(midX, bounds.top, bounds.calcRight() - midX, midY - bounds.top);

	renderRect(mask, bottomRight, silly);
	renderRect(mask, topLeft, silly);
renderRect(mask, bottomLeft, silly);
renderRect(mask, topRight, silly);
if (!equals(baseline, silly, bounds, "random order")) {
std::cerr << "random order render mismatch\n";
return 1;
}

SelfContainedRaster<Mask8> separate(bounds);
{
PolygonMask m(path);
renderRect(m, bottomRight, separate);
}
{
PolygonMask m(path);
renderRect(m, topLeft, separate);
}
{
PolygonMask m(path);
renderRect(m, bottomLeft, separate);
}
{
PolygonMask m(path);
renderRect(m, topRight, separate);
}
if (!equals(baseline, separate, bounds, "multi rasterizer")) {
std::cerr << "multi rasterizer render mismatch\n";
return 1;
}

return 0;
}

