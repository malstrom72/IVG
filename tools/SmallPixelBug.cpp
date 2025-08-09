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

#include <iostream>
#include <vector>
#include <stdexcept>
#include "src/IVG.h"
#include "externals/NuX/NuXPixels.h"
#include "png.h"
#include "zlib.h"

using namespace NuXPixels;
using namespace IVG;
using namespace IMPD;

static void PNGAPI myPNGErrorFunction(png_structp png_ptr, png_const_charp error_msg) {
	throw std::runtime_error(std::string("Error writing PNG image : ") + std::string(static_cast<const char*>(error_msg)));
}

static bool isLittleEndian() {
	assert(sizeof (unsigned int) == 4);
	static const unsigned char bytes[4] = { 0x4A, 0x3B, 0x2C, 0x1D };
	if (*reinterpret_cast<const unsigned int*>(bytes) == 0x1D2C3B4A) {
		return true;
	} else {
		assert(*reinterpret_cast<const unsigned int*>(bytes) == 0x4A3B2C1D);
		return false;
	}
}

int main(int argc, const char* argv[]) {
	try {
		if (argc != 2) {
			std::cerr << "SmallPixelBug <output.png>\n";
			return 1;
		}

		const int SCREEN_WIDTH = 800;
		const int SCREEN_HEIGHT = 250;
		IntRect bounds(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		SelfContainedRaster<ARGB32> raster(bounds);
		static GammaTable myGamma(1.41);
		const double curveQuality = 1.0;

		const char* iPathData =
		"M2752.875,216.875c0,0-23.057,0.7-23.875,0.5c-3.875-1.25-18.625-15-22.75-20.125"
		"c8.25-1.625,28.234-3.402,28.234-3.402l6.391,1.402L2752.875,216.875z"
		"M2779.875,306.125c-1.75,3.625-2.5,6.5-5.625,11.375c-3.311-1.872-31.078-17.036-34.25-18.875"
		"c13.25-31.625,1.625-62.375-1.5-70.125c7.865-0.78,15.861-1.458,23.625-0.625"
		"C2778.75,245.5,2783.25,286.5,2779.875,306.125z";
		Path iPath;
		const char* errorString = 0;
		if (!buildPathFromSVG(iPathData, curveQuality, iPath, errorString)) {
			std::cerr << "Failed to parse path: " << (errorString ? errorString : "") << std::endl;
			return 1;
		}

		AffineTransformation xlate(AffineTransformation().translate(-2200, -150));
		Vertex sp = xlate.transform(Vertex(2742.1494, 196.7764));
		Vertex ep = xlate.transform(Vertex(2743.7817, 316.4407));
		LinearAscend myRamp(sp.x, sp.y, ep.x, ep.y);
		Gradient<ARGB32>::Stop myGradientStops[5] = {
			{0.0, 0xFF78CCCB},
			{0.1012, 0xFF74C1C8},
			{0.2942, 0xFF6AA8C2},
			{0.5562, 0xFF5C82B5},
			{0.8182, 0xFF4F5DAA}
		};
		Gradient<ARGB32> myGradient(5, myGradientStops);

		raster = Solid<ARGB32>(0xFFFFFFFF)
		| myGradient[myRamp] * myGamma[PolygonMask(Path(iPath).transform(xlate).closeAll(), bounds)]
		| Solid<ARGB32>(0xFFEEEDE3) * myGamma[PolygonMask(Path(iPath).stroke(4, Path::BUTT, Path::MITER, 10.0).transform(xlate), bounds)];

		std::vector<png_bytep> rowPointers(bounds.height);
		int imageStride = raster.getStride();
		ARGB32::Pixel* pixels = raster.getPixelPointer();
		for (int y = 0; y < bounds.height; ++y) {
			ARGB32::Pixel* p = pixels + y * imageStride;
			rowPointers[y] = reinterpret_cast<png_bytep>(p);
			for (int x = 0; x < bounds.width; ++x) {
				int a = (*p >> 24) & 0xFF;
				if (a != 0xFF && a != 0x00) {
					int m = 0xFFFF / a;
					int r = (((*p >> 16) & 0xFF) * m) >> 8;
					int g = (((*p >> 8) & 0xFF) * m) >> 8;
					int b = (((*p >> 0) & 0xFF) * m) >> 8;
					*p = (a << 24) | (r << 16) | (g << 8) | (b << 0);
				}
				++p;
			}
		}

		FILE* f = 0;
		png_structp png_ptr = 0;
		png_infop info_ptr = 0;
		try {
			f = fopen(argv[1], "wb");
			if (f == 0) throw std::runtime_error("Could not open output PNG file");

			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, myPNGErrorFunction, 0);
			if (png_ptr == 0) throw std::runtime_error("Error writing PNG image : could not initialize");

			info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr == 0) throw std::runtime_error("Error writing PNG image : could not initialize");

			png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
			png_init_io(png_ptr, f);
			png_set_IHDR(png_ptr, info_ptr, bounds.width, bounds.height, 8, PNG_COLOR_TYPE_RGB_ALPHA,
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_ABSOLUTE);
			png_set_oFFs(png_ptr, info_ptr, bounds.left, bounds.top, PNG_OFFSET_PIXEL);
			png_set_rows(png_ptr, info_ptr, &rowPointers[0]);
			png_write_png(png_ptr, info_ptr,
			(isLittleEndian() ? PNG_TRANSFORM_BGR : PNG_TRANSFORM_SWAP_ALPHA), 0);
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(f);
			f = 0;
		}
		catch (...) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			if (f != 0) fclose(f);
			throw;
		}
	}
	catch (const std::exception& x) {
		std::cerr << "Exception: " << x.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cerr << "General exception" << std::endl;
		return 1;
	}
	return 0;
}