/**
	\file NuXPNG.h

	NuXPNG is a library for:

	TODO

	NuXPNG is part of the NuEdge X-Platform Library / NuX.
	Written by Magnus Lidstroem
	(C) NuEdge Development 2005
	All Rights Reserved

	NuX design goals:
	
	1) Cross platform with effective OS-specific implementations for Windows XP and Mac OS X Carbon.
	2) Emphasis on native platform approaches and solutions. Follows rules and conventions of the supported platforms to maximum extent.
	3) Light-weight with small header files that do not depend on heavy platform-specific headers.
	4) Self-contained components with few dependencies. Few source files. Easily integrated into existing projects in whole or in part.
	5) Minimalistic but flexible approach, providing as few necessary building blocks as possible without sacrificing versatility.
	6) Easily understood standard C++ code, avoiding complex templates and using only a small set of the Standard C++ Library and STL.
	7) Self-explanatory code with inline documentation. Clear and consistent naming conventions.
*/

#ifndef NuXPNG_h
#define NuXPNG_h

#include <exception>
#include <string>

namespace NuXPNG {

class Exception : public std::exception {
	public:		Exception(const std::string& errorString) : errorString(errorString) { }
	public:		virtual const char *what() const throw() { return errorString.c_str(); }
	public:		std::string getErrorString() const { return errorString; }
	public:		virtual ~Exception() throw() { }
	private:	std::string errorString;
};

class ByteInput {
	public:		virtual void readBytes(int count, unsigned char* bytes) = 0;											///< Read \p count bytes from the input stream into \p bytes. If not enough bytes are available, an exception should be thrown.
};

typedef std::pair<std::string, std::string> PNGTextKVPair;

class PNGReader {
	protected:	class Impl;
	public:		PNGReader(ByteInput& input);																			///< Start parsing the PNG-file. Once constructed, you may use all methods in this class (but only read the entire image once). Pass a derivative of ByteInput in \p input (this instance must be kept alive during the life-time of the PNGReader instance). This constructor will throw exceptions on errors.
	public:		int getChannels() const;																				///< Returns number of channels in image source. 1 = gray-scale or palette, 2 = gray-scale + alpha (or palette with alpha), 3 = rgb, 4 = rgb + alpha. Notice that regardless of source channel configuration, all images are readable in either AARRGGBB (native endianess) or 8-bit grayscale format.
	public:		int getWidth() const;																					///< Returns width of source image in pixels.
	public:		int getHeight() const;																					///< Returns width of source image in pixels.
	public:		void assignTargetGamma(double gamma = 2.2);																///< Sets target gamma and enables gamma correction.
	public:		void readImageScanlines32Bit(unsigned int** scanlinePointers, bool premultiplyAlpha = true);			///< \p scanlinePointers should point to scanlines of ints for the entire image. Returned ints are always in AARRGGBB native endianess format, regardless of source format. I.e. "pixel & 0xFF" is always the blue channel.
	public:		void readImage32Bit(unsigned int* image, bool premultiplyAlpha = true);									///< A simpler method if you do not need the flexibility of readImageScanlines32Bit. \p image must point to at least width * height ints.
	public:		void readImageScanlines8Bit(unsigned char** scanlinePointers);											///< \p scanlinePointers should point to scanlines of bytes for the entire image. Returned bytes are always grayscale (from black = 0 to white = 255), regardless of source format.
	public:		void readImage8Bit(unsigned char* image);																///< A simpler method if you do not need the flexibility of readImageScanlines8Bit. \p image must point to at least width * height bytes.
	public:		void getPNGTexts(std::vector<PNGTextKVPair>& texts);													///< Important: call as last step after reading the image.
	public:		virtual ~PNGReader();
	protected:	Impl* impl;
};

template<class T> class ByteInputAdapter : public ByteInput {
	public:		ByteInputAdapter(T& byteInput) : byteInput(byteInput) { }
	public:		virtual void readBytes(int count, unsigned char* bytes) { byteInput.readBytes(count, bytes); }
	public:		T& byteInput;
};

} /* namespace NuXPNG */

#endif
