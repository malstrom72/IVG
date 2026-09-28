#ifndef PNGReading_h
#define PNGReading_h

/**
	PNG reading shared by IVG2PNG, IVGSnapshot and IVGFiddle, so an image renders the same in all three.
**/

#include <limits>
#include <stdexcept>
#include "png.h"
#include "../externals/NuX/NuXPixels.h"

/**
	Makes libpng hand back 8-bit BGRA rows for any PNG, which is what copyPNGRow() reads. Call png_read_png() with
	`PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16`, since those alone leave grayscale narrow and 16-bit wide.
**/
inline void setPNGReadTransforms(png_structp png) {
	png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
	png_set_gray_to_rgb(png);
	png_set_bgr(png);
}

/**
	Throws unless a `width` by `height` raster fits NuXPixels, which counts pixels in an int.
**/
inline void checkPNGSize(png_uint_32 width, png_uint_32 height) {
	if (width == 0 || height == 0 || width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) / height) {
		throw std::runtime_error("PNG dimensions exceed supported range");
	}
}

/**
	Converts one BGRA row to premultiplied ARGB32, rounding to nearest. IVGSnapshot writes its goldens with the exact
	inverse, so a golden reloads unchanged.
**/
inline void copyPNGRow(const png_byte* source, NuXPixels::ARGB32::Pixel* destination, png_uint_32 width) {
	for (png_uint_32 x = 0; x < width; ++x) {
		unsigned int b = source[x * 4 + 0];
		unsigned int g = source[x * 4 + 1];
		unsigned int r = source[x * 4 + 2];
		const unsigned int a = source[x * 4 + 3];
		if (a != 0xFF) {
			b = (b * a + 127) / 255;
			g = (g * a + 127) / 255;
			r = (r * a + 127) / 255;
		}
		destination[x] = (a << 24) | (r << 16) | (g << 8) | b;
	}
}

#endif
