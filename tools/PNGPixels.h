#ifndef PNGPixels_h
#define PNGPixels_h

/**
	PNG pixel conversion shared by IVG2PNG, IVGSnapshot and IVGFiddle, so all three read and write the same pixels.
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
	Converts one BGRA row to premultiplied ARGB32, rounding to nearest. unpremultiplyPixel() is the exact inverse, so a
	PNG written by these tools reloads unchanged.
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

inline unsigned int unpremultiplyChannel(unsigned int value, unsigned int alpha) {
	const unsigned int straight = (value * 255 + alpha / 2) / alpha;
	return (straight > 255 ? 255 : straight);
}

/**
	Converts one premultiplied ARGB32 pixel to straight alpha for writing a PNG, rounding to nearest.
**/
inline NuXPixels::ARGB32::Pixel unpremultiplyPixel(NuXPixels::ARGB32::Pixel pixel) {
	const unsigned int a = (pixel >> 24) & 0xFF;
	if (a == 0x00 || a == 0xFF) {
		return pixel;
	}
	return (a << 24) | (unpremultiplyChannel((pixel >> 16) & 0xFF, a) << 16)
			| (unpremultiplyChannel((pixel >> 8) & 0xFF, a) << 8) | unpremultiplyChannel(pixel & 0xFF, a);
}

#endif
