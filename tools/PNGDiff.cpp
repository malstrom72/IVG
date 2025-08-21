/**
	PNGDiff outputs a diff image highlighting differing pixels.
	Pixels that differ are marked in bright magenta; others are black.
**/
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <string>
#include "png.h"

using namespace std;

static void readPNG(const char *path, vector<png_byte> &out, int &width, int &height) {
	FILE *fp = fopen(path, "rb");
	if (!fp) throw runtime_error(string("Failed to open ") + path);
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, nullptr);
		fclose(fp);
		throw runtime_error("Error reading PNG");
	}
	png_init_io(png, fp);
	png_read_info(png, info);
	width = png_get_image_width(png, info);
	height = png_get_image_height(png, info);
	png_set_expand(png);
	png_set_strip_16(png);
	png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
	png_set_gray_to_rgb(png);
	png_read_update_info(png, info);
	out.resize(width * height * 4);
	vector<png_bytep> rows(height);
	for (int y = 0; y < height; y++) rows[y] = &out[y * width * 4];
	png_read_image(png, rows.data());
	png_destroy_read_struct(&png, &info, nullptr);
	fclose(fp);
}

static void writePNG(const char *path, const vector<png_byte> &data, int width, int height) {
	FILE *fp = fopen(path, "wb");
	if (!fp) throw runtime_error(string("Failed to open ") + path);
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &info);
		fclose(fp);
		throw runtime_error("Error writing PNG");
	}
	png_init_io(png, fp);
	png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png, info);
	vector<png_bytep> rows(height);
	for (int y = 0; y < height; y++) rows[y] = (png_bytep)&data[y * width * 4];
	png_write_image(png, rows.data());
	png_write_end(png, nullptr);
	png_destroy_write_struct(&png, &info);
	fclose(fp);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <a.png> <b.png> <diff.png>\n", argv[0]);
		return 1;
	}
	int w1, h1, w2, h2;
	vector<png_byte> a, b;
	readPNG(argv[1], a, w1, h1);
	readPNG(argv[2], b, w2, h2);
	if (w1 != w2 || h1 != h2) {
		fprintf(stderr, "Image dimensions must match\n");
		return 1;
	}
	vector<png_byte> out(w1 * h1 * 4);
	for (size_t i = 0; i < out.size(); i += 4) {
		if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2] || a[i + 3] != b[i + 3]) {
			out[i] = 255;
			out[i + 1] = 0;
			out[i + 2] = 255;
			out[i + 3] = 255;
		} else {
			out[i] = 0;
			out[i + 1] = 0;
			out[i + 2] = 0;
			out[i + 3] = 255;
		}
	}
	writePNG(argv[3], out, w1, h1);
	return 0;
}
