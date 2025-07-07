#include <vector>
#include "png.h"
#include "NuXPNG.h"
#include "assert.h"

namespace NuXPNG {

template<typename T, typename U> T lossless_cast(U x) {
	assert(static_cast<U>(static_cast<T>(x)) == x);
	return static_cast<T>(x);
}

class PNGReader::Impl {
	public:		Impl();
	public:		~Impl();
	public:		png_struct* png;
	public:		png_info* info;
	public:		bool doGamma;
	public:		double targetGamma;
};

PNGReader::Impl::Impl()
	: png(0)
	, info(0)
	, doGamma(false)
	, targetGamma(2.2)
{
}

PNGReader::Impl::~Impl()
{
	png_destroy_read_struct(&png, &info, NULL);
	png = 0;
	info = 0;
}

static void PNGAPI myPNGErrorFunction(png_struct* /*png_ptr*/, png_const_charp error_msg)
{
	throw Exception(std::string("Error reading PNG image : ") + error_msg);
}

static void PNGAPI myPNGReadFunction(png_struct* png_ptr, png_byte* data, png_size_t length)
{
	ByteInput* input = reinterpret_cast<ByteInput*>(png_get_io_ptr(png_ptr));
	input->readBytes(lossless_cast<int>(length), data);
}

static bool isLittleEndian()
{
	assert(sizeof (unsigned int) == 4);
	static const unsigned char bytes[4] = { 0x4A, 0x3B, 0x2C, 0x1D };
	if (*reinterpret_cast<const unsigned int*>(bytes) == 0x1D2C3B4A) {
		return true;
	} else {
		assert(*reinterpret_cast<const unsigned int*>(bytes) == 0x4A3B2C1D);
		return false;
	}
}
	
PNGReader::PNGReader(ByteInput& input)
	: impl(new PNGReader::Impl())
{
	unsigned char header[8];
	input.readBytes(8, header);

	if (png_sig_cmp(reinterpret_cast<png_bytep>(header), 0, 8)){
		throw Exception("Error reading PNG image : invalid format");
	}

	impl->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, myPNGErrorFunction, 0);
	if (impl->png == 0){
		throw Exception("Error reading PNG image : could not initialize");
	}

	impl->info = png_create_info_struct(impl->png);
	if (impl->info == 0){
		throw Exception("Error reading PNG image : could not initialize");
	}
	
	png_set_read_fn(impl->png, &input, myPNGReadFunction);
	png_set_sig_bytes(impl->png, 8);

	png_read_info(impl->png, impl->info);
}

int PNGReader::getChannels() const
{
	assert(impl != 0);
	int channels = png_get_channels(impl->png, impl->info);
	if (png_get_valid(impl->png, impl->info, PNG_INFO_tRNS)) {
		assert(channels == 1 || channels == 3);
		++channels;
	}
	return channels;
}

int PNGReader::getWidth() const
{
	assert(impl != 0);
	return lossless_cast<int>(png_get_image_width(impl->png, impl->info));
}

int PNGReader::getHeight() const
{
	assert(impl != 0);
	return lossless_cast<int>(png_get_image_height(impl->png, impl->info));
}

void PNGReader::assignTargetGamma(double gamma)
{
	assert(impl != 0);
	impl->doGamma = true;
	impl->targetGamma = gamma;
}

void PNGReader::readImageScanlines32Bit(unsigned int** scanlinePointers, bool premultiplyAlpha)
{
	assert(impl != 0);
	assert(sizeof (unsigned int) == 4);
	
	png_byte colorType = png_get_color_type(impl->png, impl->info);

	png_byte bitDepth = png_get_bit_depth(impl->png, impl->info);
	if (bitDepth < 8) {
		if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_expand_gray_1_2_4_to_8(impl->png);
		} else {
			png_set_packing(impl->png);
		}
    } else if (bitDepth == 16) {
        png_set_strip_16(impl->png);
	}
	
	bool gotAlpha = false;
	if (png_get_valid(impl->png, impl->info, PNG_INFO_tRNS)) {
		gotAlpha = true;
	}
	if (isLittleEndian()) {
		switch (colorType) {
			default: throw Exception("Error reading PNG image : unsupported pixel model");
			
			case PNG_COLOR_TYPE_PALETTE: png_set_palette_to_rgb(impl->png); png_set_bgr(impl->png); if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_AFTER); break;
			case PNG_COLOR_TYPE_RGB: png_set_bgr(impl->png); if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_AFTER); break;
			case PNG_COLOR_TYPE_RGB_ALPHA: png_set_bgr(impl->png); gotAlpha = true; break;
			case PNG_COLOR_TYPE_GRAY: png_set_gray_to_rgb(impl->png); png_set_bgr(impl->png); if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_AFTER); break;
			case PNG_COLOR_TYPE_GRAY_ALPHA: png_set_gray_to_rgb(impl->png); png_set_bgr(impl->png); gotAlpha = true; break;
		}
	} else {
		switch (colorType) {
			default: throw Exception("Error reading PNG image : unsupported pixel model");
			
			case PNG_COLOR_TYPE_PALETTE: png_set_palette_to_rgb(impl->png); if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_BEFORE); break;
			case PNG_COLOR_TYPE_RGB: if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_BEFORE); break;
			case PNG_COLOR_TYPE_RGB_ALPHA: png_set_swap_alpha(impl->png); gotAlpha = true; break;
			case PNG_COLOR_TYPE_GRAY: png_set_gray_to_rgb(impl->png); if (!gotAlpha) png_set_filler(impl->png, 0xFF, PNG_FILLER_BEFORE); break;
			case PNG_COLOR_TYPE_GRAY_ALPHA: png_set_gray_to_rgb(impl->png); png_set_swap_alpha(impl->png); gotAlpha = true; break;
		}
	}
	
	if (impl->doGamma) {
		double fileGamma;
		if (!png_get_gAMA(impl->png, impl->info, &fileGamma)) fileGamma = 1.0 / 2.2;
		png_set_gamma(impl->png, impl->targetGamma, fileGamma);
	}
	
	png_set_interlace_handling(impl->png);
	png_read_update_info(impl->png, impl->info);
	png_read_image(impl->png, reinterpret_cast<png_byte**>(scanlinePointers));
	png_read_end(impl->png, impl->info);

	if (premultiplyAlpha && gotAlpha) {
		int h = lossless_cast<int>(png_get_image_height(impl->png, impl->info));
		int w = lossless_cast<int>(png_get_image_width(impl->png, impl->info));
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				unsigned int argb = scanlinePointers[y][x];
				unsigned int alpha = argb >> 24;
				unsigned int a = alpha + ((alpha != 0) ? 1 : 0);
				argb = (((((argb & 0xFF00FF) * a) & 0xFF00FF00) | (((argb & 0x00FF00) * a) & 0x00FF0000)) >> 8) | (alpha << 24);
				scanlinePointers[y][x] = argb;
			}
		}
	}
}

void PNGReader::readImage32Bit(unsigned int* image, bool premultiplyAlpha)
{
	int height = getHeight();
	int width = getWidth();
	if (height > 0) {
		std::vector<unsigned int*> scanlinePointers(height);
		for (int y = 0; y < height; ++y) {
			scanlinePointers[y] = image + width * y;
		}
		readImageScanlines32Bit(&scanlinePointers[0], premultiplyAlpha);
	}
}

void PNGReader::readImageScanlines8Bit(unsigned char** scanlinePointers)
{
	assert(impl != 0);
	assert(sizeof (unsigned int) == 4);
	
	png_byte colorType = png_get_color_type(impl->png, impl->info);

	png_byte bitDepth = png_get_bit_depth(impl->png, impl->info);
	if (bitDepth < 8) {
		if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
			png_set_expand_gray_1_2_4_to_8(impl->png);
		} else {
			png_set_packing(impl->png);
		}
    } else if (bitDepth == 16) {
        png_set_strip_16(impl->png);
	}
		
	switch (colorType) {
		case PNG_COLOR_TYPE_PALETTE:
		case PNG_COLOR_TYPE_GRAY_ALPHA: /* TODO : support, as it is now you need to 'flatten' in photoshop for the alpha channel to go away */
		default: throw Exception("Error reading PNG image : unsupported pixel model");
		
		case PNG_COLOR_TYPE_RGB:
		case PNG_COLOR_TYPE_RGB_ALPHA: png_set_rgb_to_gray_fixed(impl->png, 1, -1, -1); break;
		case PNG_COLOR_TYPE_GRAY: break;
	}
	
	if (impl->doGamma) {
		double fileGamma;
		if (!png_get_gAMA(impl->png, impl->info, &fileGamma)) fileGamma = 1.0 / 2.2;
		png_set_gamma(impl->png, impl->targetGamma, fileGamma);
	}

	png_set_interlace_handling(impl->png);
	png_read_update_info(impl->png, impl->info);
	png_read_image(impl->png, reinterpret_cast<png_byte**>(scanlinePointers));
	png_read_end(impl->png, impl->info);
}

// TODO : very similar to 32-bit, templatize?
void PNGReader::readImage8Bit(unsigned char* image)
{
	int height = getHeight();
	int width = getWidth();
	if (height > 0) {
		std::vector<unsigned char*> scanlinePointers(height);
		for (int y = 0; y < height; ++y) {
			scanlinePointers[y] = image + width * y;
		}
		readImageScanlines8Bit(&scanlinePointers[0]);
	}
}

void PNGReader::getPNGTexts(std::vector<PNGTextKVPair>& texts) {
	png_text* textsPointer;
	int numberOfTexts = 0;
	const int count = png_get_text(impl->png, impl->info, &textsPointer, &numberOfTexts);
	(void)count;
	for (int i = 0; i < numberOfTexts; ++i) {
		texts.push_back(std::make_pair(std::string(textsPointer[i].key), std::string(textsPointer[i].text)));
	}
}

PNGReader::~PNGReader()
{
	delete impl;
}

} /* namespace NuXPNG */
