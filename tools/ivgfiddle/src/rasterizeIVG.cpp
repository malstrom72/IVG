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

#include <emscripten.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
#endif
#include <iostream>
#include <fstream>
#include <string>
#include <istream>
#include <ostream>
#include <iterator>
#include <sstream>
#include <cstdint>
#include <cmath>
#include "../src/IVG.h"

using namespace std;
using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

class IVGExecutorWithExternalFonts : public IVGExecutor {
	public:	IVGExecutorWithExternalFonts(Canvas& canvas, const AffineTransformation& xform)
					: IVGExecutor(canvas, xform) {
			}
			virtual std::vector<const Font*> lookupFonts(Interpreter& interpreter, const WideString& fontName
					, const UniString& forString) {
				(void)interpreter;
				(void)forString;
				std::pair< FontMap::iterator, bool > insertResult = loadedFonts.insert(std::make_pair(fontName, Font()));
				if (insertResult.second) {
					const std::string fontName8Bit(fontName.begin(), fontName.end());
					String fontCode;
					{
						std::ifstream fileStream((fontName8Bit + ".ivgfont").c_str());
						if (!fileStream.good()) {
							return std::vector<const Font*>();
						}
						fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
						const std::istreambuf_iterator<Char> it(fileStream);
						const std::istreambuf_iterator<Char> end;
						fontCode = std::string(it, end);
					}
					FontParser fontParser;
					STLMapVariables vars;
					FormatInfo formatInfo;
					Interpreter impd(fontParser, vars, formatInfo);
					impd.run(fontCode);
					insertResult.first->second = fontParser.finalizeFont();
				}
				return std::vector<const Font*>(1, &insertResult.first->second);
			}
	protected:	FontMap loadedFonts;
};

namespace {
const int MAX_RASTER_DIMENSION = 16384;
const long long MAX_RASTER_PIXELS = 67108864LL;
const size_t VECTOR_HEAP_RESERVE_BYTES = 2 * 1024 * 1024;

size_t computeFreeHeapBytes();

IntRect computeScaledBounds(const IntRect& bounds, double rescale)
{
        if (rescale == 1.0) {
                return bounds;
        }
        const double scaledLeft = static_cast<double>(bounds.left) * rescale;
        const double scaledTop = static_cast<double>(bounds.top) * rescale;
        const double scaledRight = static_cast<double>(bounds.left + bounds.width) * rescale;
        const double scaledBottom = static_cast<double>(bounds.top + bounds.height) * rescale;
        IntRect result;
        result.left = static_cast<int>(std::floor(scaledLeft));
        result.top = static_cast<int>(std::floor(scaledTop));
        result.width = static_cast<int>(std::ceil(scaledRight) - result.left);
        result.height = static_cast<int>(std::ceil(scaledBottom) - result.top);
        return result;
}

class GuardedSelfContainedARGB32Canvas : public SelfContainedARGB32Canvas {
        public: GuardedSelfContainedARGB32Canvas(double rescaleBounds, long long pixelBudget,
                                size_t heapReserveBytes)
                                        : SelfContainedARGB32Canvas(rescaleBounds)
                                        , maxRasterPixels(pixelBudget)
                                        , heapReserve(heapReserveBytes) {
                        }

        public: virtual void defineBounds(const IntRect& newBounds) {
                        const IntRect scaledBounds = computeScaledBounds(newBounds, rescaleBounds);
                        preflightBounds(scaledBounds);
                        SelfContainedARGB32Canvas::defineBounds(newBounds);
                }

        private: void preflightBounds(const IntRect& scaledBounds) const {
                        if (scaledBounds.width <= 0 || scaledBounds.height <= 0) {
                                return;
                        }
                        const long long pixelCount = static_cast<long long>(scaledBounds.width)
                                * static_cast<long long>(scaledBounds.height);
                        if (scaledBounds.width > MAX_RASTER_DIMENSION || scaledBounds.height > MAX_RASTER_DIMENSION) {
                                std::ostringstream message;
                                message << "Rasterization aborted: scaled bounds " << scaledBounds.width << "x"
                                        << scaledBounds.height << " exceed the " << MAX_RASTER_DIMENSION
                                        << "px dimension cap.";
                                throw runtime_error(message.str());
                        }
                        if (maxRasterPixels > 0 && pixelCount > maxRasterPixels) {
                                std::ostringstream message;
                                message << "Rasterization aborted: " << scaledBounds.width << "x" << scaledBounds.height
                                        << " = " << pixelCount << " pixels exceeds the " << maxRasterPixels
                                        << " pixel budget.";
                                throw runtime_error(message.str());
                        }
                        const size_t requiredPixelBytes = static_cast<size_t>(scaledBounds.width)
                                * static_cast<size_t>(scaledBounds.height) * 4u;
                        const size_t requiredBytes = 4u * 4u + requiredPixelBytes;
#ifdef __EMSCRIPTEN__
                        const size_t freeHeapBytes = computeFreeHeapBytes();
                        if (freeHeapBytes > 0 && requiredBytes + heapReserve > freeHeapBytes) {
                                std::ostringstream message;
                                message << "Rasterization aborted: " << requiredBytes << " bytes required but only "
                                        << freeHeapBytes << " bytes free in the WebAssembly heap.";
                                throw runtime_error(message.str());
                        }
#else
                        (void)requiredBytes;
#endif
                }

        private: const long long maxRasterPixels;
        private: const size_t heapReserve;
};

size_t computeFreeHeapBytes()
{
#ifdef __EMSCRIPTEN__
	uintptr_t* sbrkPointer = emscripten_get_sbrk_ptr();
	if (sbrkPointer != 0) {
		const uintptr_t currentBrk = *sbrkPointer;
		const size_t heapBytes = emscripten_get_heap_size();
		if (heapBytes > currentBrk) {
			return heapBytes - static_cast<size_t>(currentBrk);
		}
	}
	return 0;
#else
	return 0;
#endif
}

}

extern "C" {

EMSCRIPTEN_KEEPALIVE
uint8_t* rasterizeIVG(const char* ivgSource, double scaling) {
	uint8_t* pixelsArray = 0;
	try {
                GuardedSelfContainedARGB32Canvas canvas(scaling, MAX_RASTER_PIXELS, VECTOR_HEAP_RESERVE_BYTES);
		{
			STLMapVariables topVars;
			IVGExecutorWithExternalFonts ivgExecutor(canvas, AffineTransformation().scale(scaling));
			FormatInfo formatInfo;
			Interpreter impd(ivgExecutor, topVars, formatInfo);
			const string source(ivgSource);
			impd.run(source);
		}

		SelfContainedRaster<ARGB32>* raster = canvas.accessRaster();
                if (raster == 0) {
                        throw runtime_error("IVG image is empty");
                }
                const IntRect bounds = raster->calcBounds();
                if (bounds.width <= 0 || bounds.height <= 0) {
                        throw runtime_error("IVG image is empty");
                }
		if (bounds.width > MAX_RASTER_DIMENSION || bounds.height > MAX_RASTER_DIMENSION) {
			std::ostringstream message;
			message << "Rasterization aborted: scaled bounds " << bounds.width << "x" << bounds.height
				<< " exceed the " << MAX_RASTER_DIMENSION << "px dimension cap.";
			throw runtime_error(message.str());
		}
		const long long pixelCount = static_cast<long long>(bounds.width) * static_cast<long long>(bounds.height);
		if (pixelCount > MAX_RASTER_PIXELS) {
			std::ostringstream message;
			message << "Rasterization aborted: " << bounds.width << "x" << bounds.height
				<< " = " << pixelCount << " pixels exceeds the " << MAX_RASTER_PIXELS << " pixel budget.";
			throw runtime_error(message.str());
		}
                const int imageStride = raster->getStride();
                const ARGB32::Pixel* sourcePixels = raster->getPixelPointer() + bounds.top * imageStride + bounds.left;
                const size_t requiredPixelBytes = static_cast<size_t>(bounds.width) * static_cast<size_t>(bounds.height) * 4u;
                const size_t requiredBytes = 4u * 4u + requiredPixelBytes;
#ifdef __EMSCRIPTEN__
		const size_t freeHeapBytes = computeFreeHeapBytes();
		if (freeHeapBytes > 0 && requiredBytes + VECTOR_HEAP_RESERVE_BYTES > freeHeapBytes) {
			std::ostringstream message;
			message << "Rasterization aborted: " << requiredBytes << " bytes required but only "
				<< freeHeapBytes << " bytes free in the WebAssembly heap.";
			throw runtime_error(message.str());
		}
#endif
                pixelsArray = new uint8_t[requiredBytes];
		*reinterpret_cast<uint32_t*>(pixelsArray + 0) = bounds.left;
		*reinterpret_cast<uint32_t*>(pixelsArray + 4) = bounds.top;
		*reinterpret_cast<uint32_t*>(pixelsArray + 8) = bounds.width;
		*reinterpret_cast<uint32_t*>(pixelsArray + 12) = bounds.height;
		uint8_t* dp = pixelsArray + 16;
		for (int y = 0; y < bounds.height; ++y) {
			const ARGB32::Pixel* sp = sourcePixels + y * imageStride;
			for (int x = 0; x < bounds.width; ++x) {
				const uint32_t p = *sp;
				++sp;
				const int a = (p >> 24) & 0xFF;
				int r = (p >> 16) & 0xFF;
				int g = (p >> 8) & 0xFF;
				int b = p & 0xFF;
				if (a != 0xFF && a != 0x00) {	// convert from pre-multiplied argb
					const int m = 0xFFFF / a;
					const int r = (r * m) >> 8;
					const int g = (g * m) >> 8;
					const int b = (b * m) >> 8;
					assert(0 <= r && r < 0x100);
					assert(0 <= g && g < 0x100);
					assert(0 <= b && b < 0x100);
				}
				dp[0] = r;
				dp[1] = g;
				dp[2] = b;
				dp[3] = a;
				dp += 4;
			}
		}
		assert(dp == pixelsArray + 4 * 4 + bounds.width * bounds.height * 4);
	}
	catch (const IMPD::Exception& x) {
		delete [] pixelsArray;
		cout << x.what() << endl;
		if (x.hasStatement()) {
			cout << "in statement: " << x.getStatement() << endl;
		}
		cout << flush;
		return 0;
	}
	catch (const exception& x) {
		delete [] pixelsArray;
		cout << x.what() << endl << flush;
		return 0;
	}
	catch (...) {
		delete [] pixelsArray;
		cout << "General exception" << endl << flush;
		return 0;
	}
	return pixelsArray;
}

EMSCRIPTEN_KEEPALIVE
void deallocatePixels(uint32_t* pixelsArray) {
	delete [] pixelsArray;
}

EMSCRIPTEN_KEEPALIVE
size_t getFreeHeapBytes() {
	return computeFreeHeapBytes();
}

}
