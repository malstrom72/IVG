#include <emscripten.h>
#include <iostream>
#include <fstream>
#include <string>
#include <istream>
#include <ostream>
#include <iterator>
#include "../src/IVG.h"

using namespace std;
using namespace IVG;
using namespace IMPD;
using namespace NuXPixels;

class IVGExecutorWithExternalFonts : public IVGExecutor {
	public:		IVGExecutorWithExternalFonts(Canvas& canvas, const AffineTransformation& xform)
					: IVGExecutor(canvas, xform) {
				}
				virtual const Font* lookupFont(Interpreter& interpreter
						, const WideString& fontName) {
					(void)interpreter;
					std::pair< FontMap::iterator, bool > insertResult
							= loadedFonts.insert( std::make_pair(fontName, Font()) );
					if (insertResult.second) {
						const std::string fontName8Bit(fontName.begin(), fontName.end());
						String fontCode;
						{
							std::ifstream fileStream((fontName8Bit + ".ivgfont").c_str());
							if (!fileStream.good()) {
								return 0;
							}
							fileStream.exceptions(std::ios_base::badbit | std::ios_base::failbit);
							const std::istreambuf_iterator<Char> it(fileStream);
							const std::istreambuf_iterator<Char> end;
							fontCode = std::string(it, end);
						}
						FontParser fontParser;
						STLMapVariables vars;
						Interpreter impd(fontParser, vars);
						impd.run(fontCode);
						insertResult.first->second = fontParser.finalizeFont();
					}
					return &insertResult.first->second;
				}
	protected:	FontMap loadedFonts;
};

extern "C" {

EMSCRIPTEN_KEEPALIVE
uint8_t* rasterizeIVG(const char* ivgSource, double scaling) {
	uint8_t* pixelsArray = 0;
	try {
		SelfContainedARGB32Canvas canvas(scaling);
		{
			STLMapVariables topVars;
			IVGExecutorWithExternalFonts ivgExecutor(canvas, AffineTransformation().scale(scaling));
			Interpreter impd(ivgExecutor, topVars);
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
		const int imageStride = raster->getStride();
		const ARGB32::Pixel* sourcePixels = raster->getPixelPointer() + bounds.top * imageStride + bounds.left;
		pixelsArray = new uint8_t[4 * 4 + bounds.width * bounds.height * 4];
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

}
