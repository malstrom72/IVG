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
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "externals/NuX/NuXFiles.h"
#include "src/IVG.h"
#include "png.h"
#include "zlib.h"

using namespace std;
using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

// Important! Make sure to compile with exceptions enabled for C code. E.g. in GCC -fexceptions (GCC_ENABLE_EXCEPTIONS)

static void PNGAPI myPNGErrorFunction(png_struct* png_ptr, png_const_charp error_msg) {
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

static std::wstring pathStringToWide(const std::string& path) {
	return std::wstring(path.begin(), path.end());
}

static bool isAbsolutePath(const WideString& path) {
	return !path.empty() && (path[0] == L'/' || path[0] == L'\\' || (path.size() >= 2 && path[1] == L':'));
}

static NuXFiles::Path pathFromUserArgument(const std::string& path) {
	const WideString wide = pathStringToWide(path);
	return isAbsolutePath(wide) ? NuXFiles::Path(wide) : NuXFiles::Path::getCurrentDirectoryPath().getRelative(wide);
}

static std::vector<unsigned char> readFileBytes(const NuXFiles::Path& path) {
	NuXFiles::ReadOnlyFile file(path);
	const NuXFiles::Int64 size = file.getSize();
	if (!size.is32Bit() || size.toInt32() < 0) {
		throw std::runtime_error("File is too large.");
	}
	const int byteCount = size.toInt32();
	std::vector<unsigned char> bytes(static_cast<size_t>(byteCount));
	if (byteCount > 0) {
		file.read(NuXFiles::Int64(0), byteCount, &bytes[0]);
	}
	return bytes;
}

static void readTextFile(const NuXFiles::Path& path, String& contents) {
	const std::vector<unsigned char> bytes = readFileBytes(path);
	contents.assign(bytes.begin(), bytes.end());
}

struct PNGReadContext {
	const unsigned char* bytes;
	size_t size;
	size_t offset;
};

static void PNGAPI myPNGReadFunction(png_structp png_ptr, png_bytep data, png_size_t length) {
	PNGReadContext* context = static_cast<PNGReadContext*>(png_get_io_ptr(png_ptr));
	if (context == 0 || length > context->size - context->offset) {
		png_error(png_ptr, "Read past end of PNG image");
	}
	std::memcpy(data, context->bytes + context->offset, length);
	context->offset += length;
}

struct PNGWriteContext {
	NuXFiles::ReadWriteFile* file;
	NuXFiles::Int64 offset;
};

static void PNGAPI myPNGWriteFunction(png_structp png_ptr, png_bytep data, png_size_t length) {
	PNGWriteContext* context = static_cast<PNGWriteContext*>(png_get_io_ptr(png_ptr));
	if (context == 0 || context->file == 0 || length > static_cast<png_size_t>(0x7FFFFFFF)) {
		png_error(png_ptr, "Error writing PNG image");
	}
	context->file->write(context->offset, static_cast<int>(length), data);
	context->offset += static_cast<int>(length);
}

static void PNGAPI myPNGFlushFunction(png_structp png_ptr) {
	PNGWriteContext* context = static_cast<PNGWriteContext*>(png_get_io_ptr(png_ptr));
	if (context != 0 && context->file != 0) {
		context->file->flush();
	}
}

class IVGExecutorWithExternalFiles : public IVGExecutor {
	public:
		IVGExecutorWithExternalFiles(Canvas& canvas, const NuXFiles::Path& fontPath
				, const NuXFiles::Path& imagePath, const NuXFiles::Path& includePath
				, const NuXFiles::Path& inputPath
				, const AffineTransformation& xform = AffineTransformation())
				: IVGExecutor(canvas, xform), fontPath(fontPath), imagePath(imagePath), includePath(includePath)
				, inputDirectory(inputPath.isRoot() ? NuXFiles::Path() : inputPath.getParent()) {
		}
		virtual std::vector<const Font*> lookupFonts(IMPD::Interpreter& interpreter, const IMPD::WideString& fontName
				, const IMPD::UniString& forString) {
			(void)interpreter;
			std::pair< FontMap::iterator, bool > insertResult = loadedFonts.insert(std::make_pair(fontName, Font()));
			if (insertResult.second) {
				const WideString fileName = fontName + L".ivgfont";
				String fontCode;
				if (!loadResource(fileName, fontPath, fontCode)) {
					return std::vector<const Font*>();
				}
				std::wcerr << "parsing external font " << fontName << std::endl;
				FontParser fontParser(this);
				STLMapVariables vars;
			    FormatInfo formatInfo;
				Interpreter impd(fontParser, vars, formatInfo);
				impd.run(fontCode);
				insertResult.first->second = fontParser.finalizeFont();
			}
			return std::vector<const Font*>(1, &insertResult.first->second);
		}
		virtual Image loadImage(IMPD::Interpreter& interpreter, const IMPD::WideString& imageSource
				, const IntRect* sourceRectangle, bool forStretching, double forXSize, bool xSizeIsRelative
				, double forYSize, bool ySizeIsRelative) {
			(void)interpreter;
			(void)sourceRectangle;
			(void)forStretching;
			(void)forXSize;
			(void)xSizeIsRelative;
			(void)forYSize;
			(void)ySizeIsRelative;
			NuXFiles::Path path;
			if (!resolveResourcePath(imageSource, imagePath, path)) {
				return Image();
			}
			const std::vector<unsigned char> imageBytes = readFileBytes(path);
			if (imageBytes.empty()) return Image();
			PNGReadContext readContext = { &imageBytes[0], imageBytes.size(), 0 };
			png_structp png_ptr = 0;
			png_infop info_ptr = 0;
			try {
				png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, myPNGErrorFunction, 0);
				if (png_ptr == 0) throw std::runtime_error("Error reading PNG image : could not initialize");
				info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr == 0) throw std::runtime_error("Error reading PNG image : could not initialize");
				png_set_read_fn(png_ptr, &readContext, myPNGReadFunction);
				png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
				if (isLittleEndian()) {
					png_set_bgr(png_ptr);
				} else {
					png_set_swap_alpha(png_ptr);
				}
				png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
				png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
				png_uint_32 height = png_get_image_height(png_ptr, info_ptr);
				png_bytep* rows = png_get_rows(png_ptr, info_ptr);
				loadedImage = SelfContainedRaster<ARGB32>(IntRect(0, 0, static_cast<int>(width), static_cast<int>(height)));
				for (png_uint_32 y = 0; y < height; ++y) {
					ARGB32::Pixel* dest = loadedImage.getPixelPointer() + y * loadedImage.getStride();
					png_bytep src = rows[y];
					for (png_uint_32 x = 0; x < width; ++x) {
						unsigned int b = src[x * 4 + 0];
						unsigned int g = src[x * 4 + 1];
						unsigned int r = src[x * 4 + 2];
						unsigned int a = src[x * 4 + 3];
						if (a != 0xFF) {
							r = (r * a + 0x7F) >> 8;
							g = (g * a + 0x7F) >> 8;
							b = (b * a + 0x7F) >> 8;
						}
						dest[x] = (a << 24) | (r << 16) | (g << 8) | b;
					}
				}
				png_destroy_read_struct(&png_ptr, &info_ptr, 0);
				Image img;
				img.raster = &loadedImage;
				img.xResolution = 1.0;
				img.yResolution = 1.0;
				return img;
			} catch (...) {
				png_destroy_read_struct(&png_ptr, &info_ptr, 0);
				return Image();
			}
		}
		virtual bool load(IMPD::Interpreter& interpreter, const IMPD::WideString& filename, String& contents) {
			(void)interpreter;
			return loadResource(filename, includePath, contents);
		}
	protected:
		bool resolveResourcePath(const WideString& filename, const NuXFiles::Path& assetPath
				, NuXFiles::Path& resolvedPath) const {
			if (isAbsolutePath(filename)) {
				const NuXFiles::Path candidate(filename);
				if (candidate.isFile()) {
					resolvedPath = candidate;
					return true;
				}
				return false;
			}
			if (!inputDirectory.isNull()) {
				const NuXFiles::Path candidate = inputDirectory.getRelative(filename);
				if (candidate.isFile()) {
					resolvedPath = candidate;
					return true;
				}
			}
			if (!assetPath.isNull()) {
				const NuXFiles::Path candidate = assetPath.getRelative(filename);
				if (candidate.isFile()) {
					resolvedPath = candidate;
					return true;
				}
			}
			const NuXFiles::Path candidate = NuXFiles::Path::getCurrentDirectoryPath().getRelative(filename);
			if (candidate.isFile()) {
				resolvedPath = candidate;
				return true;
			}
			return false;
		}
		bool loadResource(const WideString& filename, const NuXFiles::Path& assetPath, String& contents) const {
			NuXFiles::Path path;
			if (!resolveResourcePath(filename, assetPath, path)) {
				return false;
			}
			readTextFile(path, contents);
			return true;
		}
		FontMap loadedFonts;
		NuXFiles::Path fontPath;
		NuXFiles::Path imagePath;
		NuXFiles::Path includePath;
		NuXFiles::Path inputDirectory;
		SelfContainedRaster<ARGB32> loadedImage;
};


#ifdef LIBFUZZ
const int BOUNDS_PIXEL_LIMIT = 1 << 24; // 16M pixels

struct FuzzerCanvas : public SelfContainedARGB32Canvas {
	using SelfContainedARGB32Canvas::SelfContainedARGB32Canvas;
	virtual void defineBounds(const IntRect& newBounds) override {
		if (newBounds.width > 0 && newBounds.height > 0
				&& newBounds.width * newBounds.height > BOUNDS_PIXEL_LIMIT) {
			Interpreter::throwRunTimeError(String("bounds area out of range [0..")
				+ Interpreter::toString(BOUNDS_PIXEL_LIMIT)
				+ String("]: ") + Interpreter::toString(newBounds.width * newBounds.height));
		}
		SelfContainedARGB32Canvas::defineBounds(newBounds);
	}
};

struct FuzzerExecutor : public IVGExecutor {
	FuzzerExecutor(Canvas& canvas, const NuXPixels::AffineTransformation& initialTransform = NuXPixels::AffineTransformation())
		: IVGExecutor(canvas, initialTransform) { }
	virtual void trace(IMPD::Interpreter& interpreter, const IMPD::WideString& s) { }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	const std::string ivgSource(reinterpret_cast<const char*>(Data), reinterpret_cast<const char*>(Data) + Size);
	FuzzerCanvas canvas;
	try {
		{
			STLMapVariables topVars;
			FuzzerExecutor ivgExecutor(canvas);
			FormatInfo formatInfo;
			Interpreter impd(ivgExecutor, topVars, formatInfo);
			impd.run(ivgSource);
		}
	}
	catch (IMPD::Exception&) {
		;
	}
	return 0;
}
#endif

#ifndef LIBFUZZ
int main(int argc, const char* argv[]) {
	try {
		const char* usage = "Usage: IVG2PNG [--fonts <dir>] [--images <dir>] [--includes <dir>]"
				" [--background <color>] [--scale <factor>] <input.ivg> <output.png>\n";
		const char* inputPath = 0;
		const char* outputPath = 0;
		ARGB32::Pixel background = 0;
		bool haveBackground = false;
		NuXFiles::Path fontPath;
		int compressionLevel = Z_BEST_COMPRESSION;
		bool fast = false;
		NuXFiles::Path imagePath;
		NuXFiles::Path includePath;
		double scale = 1.0;
		for (int i = 1; i < argc; ++i) {
			std::string arg(argv[i]);
			if (arg == "--fast") {
				fast = true;
				compressionLevel = Z_BEST_SPEED;
			} else if (arg == "--fonts") {
				if (++i == argc) { std::cerr << usage; return 1; }
				fontPath = pathFromUserArgument(argv[i]);
			} else if (arg == "--images") {
				if (++i == argc) { std::cerr << usage; return 1; }
				imagePath = pathFromUserArgument(argv[i]);
			} else if (arg == "--includes") {
				if (++i == argc) { std::cerr << usage; return 1; }
				includePath = pathFromUserArgument(argv[i]);
			} else if (arg == "--background") {
				if (++i == argc) { std::cerr << usage; return 1; }
				background = parseColor(argv[i]);
				haveBackground = true;
			} else if (arg == "--scale") {
				if (++i == argc) { std::cerr << usage; return 1; }
				scale = Interpreter::toDouble(String(argv[i]));
				if (!std::isfinite(scale) || scale <= 0.0) {
					std::cerr << "Scale factor must be a positive finite number." << std::endl;
					return 1;
				}
			} else if (inputPath == 0) {
				inputPath = argv[i];
			} else if (outputPath == 0) {
				outputPath = argv[i];
			} else {
				std::cerr << usage;
				return 1;
			}
		}
		if (inputPath == 0 || outputPath == 0) {
			std::cerr << usage;
			return 1;
		}

		const NuXFiles::Path inputFilePath(pathFromUserArgument(inputPath));
		const NuXFiles::Path outputFilePath(pathFromUserArgument(outputPath));
		std::string ivgContents;
		readTextFile(inputFilePath, ivgContents);
		std::cerr << "Read source IVG..." << std::endl;

		SelfContainedARGB32Canvas canvas(scale);
		{
			STLMapVariables topVars;
			IVGExecutorWithExternalFiles ivgExecutor(canvas, fontPath, imagePath, includePath, inputFilePath
					, AffineTransformation().scale(scale));
			FormatInfo formatInfo;
			Interpreter impd(ivgExecutor, topVars, formatInfo);
			impd.run(ivgContents);
		}
		std::cerr << "Rasterized image..." << std::endl;

		SelfContainedRaster<ARGB32>* raster = canvas.accessRaster();
		if (raster == 0) throw std::runtime_error("IVG image is empty");
		IntRect bounds = raster->calcBounds();
		if (bounds.width <= 0 || bounds.height <= 0) throw std::runtime_error("IVG image is empty");

		if (haveBackground) {
			SelfContainedRaster<ARGB32> copy(*raster);
			(*raster) = Solid<ARGB32>(background) | copy;
		}
		std::vector<png_bytep> rowPointers(bounds.height);
		int imageStride = raster->getStride();
		ARGB32::Pixel* pixels = raster->getPixelPointer() + bounds.top * imageStride + bounds.left;
		for (int i = 0; i < bounds.height; ++i) {
			ARGB32::Pixel* p = pixels + i * imageStride;
			rowPointers[i] = reinterpret_cast<png_bytep>(p);
			for (int x = 0; x < bounds.width; ++x) {
				int a = (*p >> 24) & 0xFF;
				int r = (*p >> 16) & 0xFF;
				int g = (*p >> 8) & 0xFF;
				int b = (*p >> 0) & 0xFF;
				if (a != 0xFF && a != 0x00) {
					int m = 0xFFFF / a;
					r = (r * m) >> 8;
					g = (g * m) >> 8;
					b = (b * m) >> 8;
					assert(0 <= r && r < 0x100);
					assert(0 <= g && g < 0x100);
					assert(0 <= b && b < 0x100);
				}
				*p = (a << 24) | (r << 16) | (g << 8) | b;
				++p;
			}
		}
		std::cerr << "Converted to non-premultiplied alpha..." << std::endl;

		{
			NuXFiles::ReadWriteFile outputFile(outputFilePath, NuXFiles::PathAttributes(), true);
			PNGWriteContext writeContext = { &outputFile, NuXFiles::Int64(0) };
			png_structp png_ptr = 0;
			png_infop info_ptr = 0;
		
			try {
				png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, myPNGErrorFunction, 0);
				if (png_ptr == 0) throw std::runtime_error("Error writing PNG image : could not initialize");

				info_ptr = png_create_info_struct(png_ptr);
				if (info_ptr == 0) throw std::runtime_error("Error writing PNG image : could not initialize");

				png_set_compression_level(png_ptr, compressionLevel);
				if (fast) png_set_filter(png_ptr, PNG_FILTER_TYPE_BASE, PNG_FILTER_NONE);
				png_set_write_fn(png_ptr, &writeContext, myPNGWriteFunction, myPNGFlushFunction);

				png_set_IHDR(png_ptr, info_ptr, bounds.width, bounds.height, 8, PNG_COLOR_TYPE_RGB_ALPHA
						, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
				
				png_set_sRGB_gAMA_and_cHRM(png_ptr, info_ptr, PNG_sRGB_INTENT_ABSOLUTE);

				png_set_oFFs(png_ptr, info_ptr, bounds.left, bounds.top, PNG_OFFSET_PIXEL);

				png_set_rows(png_ptr, info_ptr, &rowPointers[0]);

				png_write_png(png_ptr, info_ptr, (isLittleEndian() ? PNG_TRANSFORM_BGR : PNG_TRANSFORM_SWAP_ALPHA), NULL);

				png_destroy_write_struct(&png_ptr, &info_ptr);
				outputFile.flush();
			}
			catch (...) {
				png_destroy_write_struct(&png_ptr, &info_ptr);
				throw;
			}
		}
		std::cerr << "Written to PNG." << std::endl;
	}
	catch (const IMPD::Exception& x) {
		std::cerr << "Exception: " << x.what() << std::endl;
		if (x.hasStatement()) std::cerr << "in statement: " << x.getStatement() << std::endl;
		return 1;
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
#endif
