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

#include <vector>
#include <iostream>
#include <stdexcept>

#include <agg_rendering_buffer.h>
#include <agg_rasterizer_scanline_aa.h>
#include <agg_scanline_p.h>
#include <agg_renderer_scanline.h>
#include <agg_conv_stroke.h>
#include <agg_path_storage.h>
#include <agg_pixfmt_rgba.h>

#include "png.h"
#include "zlib.h"

using namespace std;

static void PNGAPI myPNGErrorFunction(png_struct* png_ptr, png_const_charp error_msg) {
	throw runtime_error(string("Error writing PNG image : ") + string(static_cast<const char*>(error_msg)));
}

int main(int argc, const char* argv[]) {
	try {
		if (argc != 2) {
			cerr << "AGGMiterBug <output.png>\n\n";
			return 1;
		}

		const int width = 800;
		const int height = 800;
		vector<unsigned char> buffer(width * height * 4, 0xFF);

		agg::rendering_buffer rbuf(&buffer[0], width, height, width * 4);
		typedef agg::pixfmt_rgba32 pixfmt;
		pixfmt pixf(rbuf);
		agg::renderer_base<pixfmt> renBase(pixf);
		agg::renderer_scanline_aa_solid<agg::renderer_base<pixfmt> > ren(renBase);
		agg::rasterizer_scanline_aa<> ras;
		agg::scanline_p8 sl;

		agg::path_storage path;
		path.move_to(100.0, 203.29150156);
		path.line_to(100.32042778048, 94.17314925764);
		path.line_to(104.0, 98.64659562576);
		path.line_to(100.320428, 94.17314940388);

		agg::conv_stroke<agg::path_storage> stroke(path);
		stroke.width(8.0);
		stroke.line_cap(agg::butt_cap);
		stroke.line_join(agg::miter_join);
		stroke.miter_limit(100.0);

		ren.color(agg::rgba8(0, 0, 0, 255));
		ras.add_path(stroke);
		agg::render_scanlines(ras, sl, ren);

		FILE* fp = fopen(argv[1], "wb");
		if (!fp) throw runtime_error("Could not open output file");

		png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, myPNGErrorFunction, 0);
		if (!png_ptr) throw runtime_error("Could not create png_struct");

		png_info* info_ptr = png_create_info_struct(png_ptr);
		if (!info_ptr) {
			png_destroy_write_struct(&png_ptr, 0);
			throw runtime_error("Could not create png_info");
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			throw runtime_error("Error writing PNG image");
		}

		png_init_io(png_ptr, fp);
		png_set_IHDR(png_ptr, info_ptr, width, height, 8
			, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE
			, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

		vector<png_bytep> rowPointers(height);
		for (int y = 0; y < height; ++y) rowPointers[y] = &buffer[y * width * 4];
		png_set_rows(png_ptr, info_ptr, &rowPointers[0]);
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);

		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return 0;
	}
	catch (exception& e) {
		cerr << e.what() << endl;
		return 1;
	}
}
