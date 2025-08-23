#include <cstdio>
#include <cmath>
#include "externals/NuX/NuXPixels.h"
using namespace NuXPixels;

int main() {
	SelfContainedRaster<ARGB32> canvas(IntRect(0, 0, 64, 64));
	Path rect;
	rect.addRect(IntRect(8, 8, 48, 48));
	PolygonMask mask(rect, canvas.calcBounds());
	canvas |= Solid<ARGB32>(0xFFFF0000) * mask; // fill red square

	FILE* f = fopen("out.ppm", "wb");
	fprintf(f, "P6\n64 64\n255\n");
	const ARGB32::Pixel* p = canvas.getPixelPointer();
	for(int i = 0; i < 64 * 64; ++i) {
		unsigned char rgb[3] = {
			static_cast<unsigned char>(p[i] >> 16),
			static_cast<unsigned char>(p[i] >> 8),
			static_cast<unsigned char>(p[i])
		};
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	return 0;
}
