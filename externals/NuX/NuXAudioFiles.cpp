#include "NuXAudioFiles.h"
#include "assert.h"
#include <math.h>

// This is a good source for info on WAV : http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html

namespace NuXAudioFiles {

template<typename T, typename U> T lossless_cast(U x) {
	assert(static_cast<U>(static_cast<T>(x)) == x);
	return static_cast<T>(x);
}

// Try to decide whether this is a little or big endian platform so that we can use specific optimizations.
// Note: it is ok to leave both of them false, in this case, generic cross-platform code will be used.

#if (defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN) \
		|| defined(__BIG_ENDIAN__) \
    	|| defined(__ARMEB__) \
    	|| defined(__THUMBEB__) \
    	|| defined(__AARCH64EB__) \
    	|| defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__) \
    	|| defined(__POWERPC__) || defined(_M_PPC)
	#define IS_LITTLE_ENDIAN 0
	#define IS_BIG_ENDIAN 1
#elif (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) \
    	|| defined(__LITTLE_ENDIAN__) \
    	|| defined(__ARMEL__) \
    	|| defined(__THUMBEL__) \
    	|| defined(__AARCH64EL__) \
    	|| defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) \
    	|| defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || defined(_M_ARM)
	#define IS_LITTLE_ENDIAN 1
	#define IS_BIG_ENDIAN 0
#else
	#define IS_LITTLE_ENDIAN 0
	#define IS_BIG_ENDIAN 0
#endif

const int CONVERSION_BUFFER_SIZE = 1024;

// Note: MSVC6 messes with std::min and std::max, so we use our own.

template<typename T> inline const T& minimum(const T& a, const T& b) { return (a < b) ? a : b; };
template<typename T> inline const T& maximum(const T& a, const T& b) { return (a > b) ? a : b; };

static const unsigned char* readBigInt32(const unsigned char* p, const unsigned char* e, int& x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 4 > e) {
		throw Exception("End of file error");
	}
	#if (IS_BIG_ENDIAN)
		union u { int i; };
		x = reinterpret_cast<const u*>(p)->i;
	#elif (!IS_BIG_ENDIAN)
		x = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	#endif
	return p + 4;
}

static const unsigned char* readBigInt16(const unsigned char* p, const unsigned char* e, short& x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 2 > e) {
		throw Exception("End of file error");
	}
	#if (IS_BIG_ENDIAN)
		union u { short s; };
		x = reinterpret_cast<const u*>(p)->s;
	#elif (!IS_BIG_ENDIAN)
		x = (p[0] << 8) | p[1];
	#endif
	return p + 2;
}

static const unsigned char* readLittleInt32(const unsigned char* p, const unsigned char* e, int& x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 4 > e) {
		throw Exception("End of file error");
	}
	#if (IS_LITTLE_ENDIAN)
		union u { int i; }; 
		x = reinterpret_cast<const u*>(p)->i;
	#elif (!IS_LITTLE_ENDIAN)
		x = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
	#endif
	return p + 4;
}

static const unsigned char* readLittleInt16(const unsigned char* p, const unsigned char* e, short& x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 2 > e) {
		throw Exception("End of file error");
	}
	#if (IS_LITTLE_ENDIAN)
		union u { short s; }; 
		x = reinterpret_cast<const u*>(p)->s;
	#elif (!IS_LITTLE_ENDIAN)
		x = (p[1] << 8) | p[0];
	#endif
	return p + 2;
}

static unsigned char* writeBigInt32(unsigned char* p, const unsigned char* e, int x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 4 > e) {
		throw Exception("End of file error");
	}
	#if (IS_BIG_ENDIAN)
		union u { int i; }; 
		reinterpret_cast<u*>(p)->i = x;
		return p + 4;
	#elif (!IS_BIG_ENDIAN)
		*p++ = static_cast<unsigned char>((x >> 24) & 0xFF);
		*p++ = static_cast<unsigned char>((x >> 16) & 0xFF);
		*p++ = static_cast<unsigned char>((x >> 8) & 0xFF);
		*p++ = static_cast<unsigned char>((x >> 0) & 0xFF);
		return p;
	#endif
}

static unsigned char* writeBigInt16(unsigned char* p, const unsigned char* e, int x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 2 > e) {
		throw Exception("End of file error");
	}
	#if (IS_BIG_ENDIAN)
		union u { short s; };
		reinterpret_cast<u*>(p)->s = x;
		return p + 2;
	#elif (!IS_BIG_ENDIAN)
		*p++ = static_cast<unsigned char>((x >> 8) & 0xFF);
		*p++ = static_cast<unsigned char>((x >> 0) & 0xFF);
		return p;
	#endif
}


static unsigned char* writeLittleInt32(unsigned char* p, const unsigned char* e, int x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 4 > e) {
		throw Exception("End of file error");
	}
	#if (IS_LITTLE_ENDIAN)
		union u { int i; }; 
		reinterpret_cast<u*>(p)->i = x;
		return p + 4;
	#elif (!IS_LITTLE_ENDIAN)
		p[0] = static_cast<unsigned char>((x >> 0) & 0xFF);
		p[1] = static_cast<unsigned char>((x >> 8) & 0xFF);
		p[2] = static_cast<unsigned char>((x >> 16) & 0xFF);
		p[3] = static_cast<unsigned char>((x >> 24) & 0xFF);
		return p + 4;
	#endif
}

static unsigned char* writeLittleInt16(unsigned char* p, const unsigned char* e, short x)
{
	assert(p != 0);
	assert(e != 0);

	if (p + 2 > e) {
		throw Exception("End of file error");
	}
	#if (IS_LITTLE_ENDIAN)
		union u { short s; }; 
		reinterpret_cast<u*>(p)->s = x;
		return p + 2;
	#elif (!IS_LITTLE_ENDIAN)
		p[0] = static_cast<unsigned char>((x >> 0) & 0xFF);
		p[1] = static_cast<unsigned char>((x >> 8) & 0xFF);
		return p + 2;
	#endif
}

static unsigned char* writeIEEE80(unsigned char* p, unsigned char* e, double x) {
	assert(p != 0);
	assert(e != 0);

	if (p + 10 > e) {
		throw Exception("End of file error");
	}
	
	if (x == 0.0) {
		memset(p, 0, 10);
		return p + 10;
	}
	
	double ax = fabs(x);
	const unsigned short exp = static_cast<unsigned short>(log(ax) / log(2.0) + 16383.0) | (x < 0.0 ? 0x8000 : 0);
	ax *= pow(2.0, 31.0 + 16383.0 - static_cast<double>(exp & 0x7FFF));
	const unsigned int high = static_cast<unsigned int>(ax);
	const unsigned int low = static_cast<unsigned int>((ax - high) * pow(2.0, 32.0));

	p = writeBigInt16(p, e, exp);
	p = writeBigInt32(p, e, high);
	p = writeBigInt32(p, e, low);
	
	return p;
}

static const unsigned char* readIEEE80(const unsigned char* p, const unsigned char* e, double& x) {
	assert(p != 0);
	assert(e != 0);

	if (p + 10 > e) {
		throw Exception("End of file error");
	}

	static const unsigned char ZEROS[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	if (memcmp(p, ZEROS, 10) == 0) {
		x = 0.0;
		return p + 10;
	}
	
	short exp;
	int high;
	int low;
	p = readBigInt16(p, e, exp);
	p = readBigInt32(p, e, high);
	p = readBigInt32(p, e, low);
	
	double val = static_cast<double>(static_cast<unsigned int>(low)) * pow(2.0, -63.0);
	val += static_cast<double>(static_cast<unsigned int>(high)) * pow(2.0, -31.0);
	val *= pow(2.0, static_cast<double>(exp & 0x7FFF) - 16383.0);
	x = (exp & 0x8000) ? -val : val;
	
	return p;
}

static void readBigEndianSamples(const unsigned char* p, const unsigned char* e, int bytesPerSample, int shift, int* d) {
	switch (bytesPerSample) {
		case 1: {
			while (p < e) {
				*d++ = static_cast<signed char>(*p++) >> shift;
			}
			break;
		}
		
		case 2: {
			while (p < e) {
				if (IS_BIG_ENDIAN) {
					*d++ = *reinterpret_cast<const short*>(p) >> shift;
				} else {
					*d++ = ((static_cast<char>(p[0]) << 8) | p[1]) >> shift;
				}
				p += 2;
			}
			break;
		}
		
		case 3: {
			while (p < e) {
				*d++ = ((static_cast<char>(p[0]) << 16) | (p[1] << 8) | p[2]) >> shift;
				p += 3;
			}
			break;
		}

		case 4: {
			while (p < e) {
				if (IS_BIG_ENDIAN) {
					*d++ = *reinterpret_cast<const int*>(p) >> shift;
				} else {
					*d++ = ((static_cast<char>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]) >> shift;
				}
				p += 4;
			}
			break;
		}

		default: assert(0);
	}
}

static void readLittleEndianSamples(const unsigned char* p, const unsigned char* e, int bytesPerSample, int shift, int* d) {
	switch (bytesPerSample) {
		case 1: {
			while (p < e) {
				*d++ = static_cast<signed char>(*p++) >> shift;
			}
			break;
		}
		
		case 2: {
			while (p < e) {
				if (IS_LITTLE_ENDIAN) {
					*d++ = *reinterpret_cast<const short*>(p) >> shift;
				} else {
					*d++ = ((static_cast<char>(p[1]) << 8) | p[0]) >> shift;
				}
				p += 2;
			}
			break;
		}
		
		case 3: {
			while (p < e) {
				*d++ = ((static_cast<char>(p[2]) << 16) | (p[1] << 8) | p[0]) >> shift;
				p += 3;
			}
			break;
		}

		case 4: {
			while (p < e) {
				if (IS_LITTLE_ENDIAN) {
					*d++ = *reinterpret_cast<const int*>(p) >> shift;
				} else {
					*d++ = ((static_cast<char>(p[3]) << 24) | (p[2] << 16) | (p[1] << 8) | p[0]) >> shift;
				}
				p += 4;
			}
			break;
		}

		default: assert(0);
	}
}

static void readFloatToIntAudio(AudioReader* reader, int channelCount, int offset, int count, int* frames) {
	float buffer[CONVERSION_BUFFER_SIZE / sizeof (float)];
	const int bufferFrameCount = lossless_cast<int>((CONVERSION_BUFFER_SIZE / sizeof (float)) / channelCount);
	assert(bufferFrameCount > 0);
	for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
		int subCount = minimum(bufferFrameCount, count - subOffset);
		reader->readInterleavedFloatAudio(offset + subOffset, subCount, buffer);
		for (int i = 0; i < subCount * channelCount; ++i) {
			const double x = floor(buffer[i] * (double)(1U << 31) + 0.5);
			const int y = lossless_cast<int>(minimum(maximum(x, -(double)(1U << 31)), (double)((1U << 31) - 1)));
			frames[subOffset * channelCount + i] = y;
		}
	}
}

static void readIntToFloatAudio(AudioReader* reader, int channelCount, int sampleBits, int offset, int count, float* frames) {
	int buffer[CONVERSION_BUFFER_SIZE / sizeof (int)];
	const double g = 1.0 / lossless_cast<unsigned int>(1 << (sampleBits - 1));
	const int bufferFrameCount = lossless_cast<int>((CONVERSION_BUFFER_SIZE / sizeof (int)) / channelCount);
	assert(bufferFrameCount > 0);
	for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
		int subCount = minimum(bufferFrameCount, count - subOffset);
		reader->readInterleavedIntAudio(offset + subOffset, subCount, &buffer[0]);
		for (int i = 0; i < subCount * channelCount; ++i) {
			frames[subOffset * channelCount + i] = static_cast<float>(buffer[i] * g);
		}
	}
}

static void writeFloatToIntAudio(AudioWriter* writer, int channelCount, int sampleBits, int offset, int count, const float* frames) {
	int buffer[CONVERSION_BUFFER_SIZE / sizeof (int)];
	const double g = lossless_cast<unsigned int>(1 << (sampleBits - 1));
	const int bufferFrameCount = lossless_cast<int>((CONVERSION_BUFFER_SIZE / sizeof (int)) / channelCount);
	assert(bufferFrameCount > 0);
	for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
		const int subCount = minimum(bufferFrameCount, count - subOffset);
		assert(subCount > 0);
		assert(channelCount > 0);
		for (int i = 0; i < subCount * channelCount; ++i) {
			const double x = floor(frames[subOffset * channelCount + i] * g + 0.5);
			buffer[i] = lossless_cast<int>(minimum(maximum(x, -g), g - 1.0));
		}
		writer->writeInterleavedIntAudio(offset + subOffset, subCount, buffer);
	}
}

static void writeIntToFloatAudio(AudioWriter* writer, int channelCount, int offset, int count, const int* frames) {
	float buffer[CONVERSION_BUFFER_SIZE / sizeof (float)];
	const int bufferFrameCount = lossless_cast<int>((CONVERSION_BUFFER_SIZE / sizeof (float)) / channelCount);
	assert(bufferFrameCount > 0);
	for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
		const int subCount = minimum(bufferFrameCount, count - subOffset);
		assert(subCount > 0);
		assert(channelCount > 0);
		for (int i = 0; i < subCount * channelCount; ++i) {
			buffer[i] = static_cast<float>(frames[subOffset * channelCount + i] * (1.0 / (double)(0x80000000UL)));
		}
		writer->writeInterleavedFloatAudio(offset + subOffset, subCount, buffer);
	}
}

/* --- WAVWriter --- */

// FIX : no application seem to be able to read 12-bit (etc) files, am I doing something wrong?

WAVWriter::WAVWriter(int channelCount, int sampleRate, bool isFloating, int sampleBits, ByteWriter& byteWriter
		, int estimatedFrameCount)
	: byteWriter(byteWriter)
	, channelCount(channelCount)
	, sampleBits(sampleBits)
	, bytesPerFrame(channelCount * ((sampleBits + 7) / 8))
	, floatOutput(isFloating)
	, writtenDataChunkSize(0)	// FIX : use estimatedFrameCount to estimate the size
	, writtenRIFFChunkSize(0)
	, writtenFrameCount(estimatedFrameCount)
	, currentFileSize(0)
	, currentFrameCount(0)
	, dataChunkOffset(0)
	, factChunkOffset(0)
{
	assert(!isFloating || sampleBits == 32);	// FIX : support 64 bits
	assert(isFloating || sampleBits >= 1 && sampleBits <= 32);
	
	bool extensible = ((sampleBits != 8 && sampleBits != 16) || channelCount > 2);
	int bytesPerSecond = bytesPerFrame * sampleRate;
	
	unsigned char buffer[256];
	
	unsigned char* p = buffer;
	unsigned char* e = buffer + 256;
	p = writeBigInt32(p, e, 'RIFF');
	p = writeLittleInt32(p, e, writtenRIFFChunkSize);
	p = writeBigInt32(p, e, 'WAVE');

	p = writeBigInt32(p, e, 'fmt ');
	p = writeLittleInt32(p, e, (extensible ? 40 : (floatOutput ? 18 : 16)));

	p = writeLittleInt16(p, e, (extensible ? 0xFFFE : (floatOutput ? 3 : 1)));
	p = writeLittleInt16(p, e, channelCount);
	p = writeLittleInt32(p, e, sampleRate);
	p = writeLittleInt32(p, e, bytesPerSecond);
	p = writeLittleInt16(p, e, bytesPerFrame);
	p = writeLittleInt16(p, e, (extensible ? (sampleBits + 7) & ~7 : sampleBits));
	
	if (extensible) {
		p = writeLittleInt16(p, e, 22);
		p = writeLittleInt16(p, e, sampleBits);
		p = writeLittleInt32(p, e, 0);
		p = writeLittleInt16(p, e, (floatOutput ? 3 : 1));
		static const unsigned char GUID[14] = { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 };
		memcpy(p, GUID, 14);
		p += 14;
	} else if (floatOutput) {
		p = writeLittleInt16(p, e, 0);
	}

	if (floatOutput) {
		{ // Test binary IEEE 754 compatibility (which is required for writeInterleavedFloatAudio to work)
			assert(sizeof (float) == sizeof (unsigned int));
			static const float FLOATS[8] = { 0.0f, 1.0f, -1.0f, 0.5f, -0.00390625f, -0.00000001f, 123.456f, 3.14159f };
			static const unsigned int INTS[8] = { 0x00000000, 0x3f800000, 0xbf800000, 0x3f000000, 0xbb800000, 0xb22bcc77, 0x42f6e979, 0x40490fd0 };
			unsigned int casted[8];
			for (int i = 0; i < 8; ++i) *reinterpret_cast<float*>(&casted[i]) = FLOATS[i];
			(void)INTS;
			assert(std::equal(casted, casted + 8, INTS));
		}

		p = writeBigInt32(p, e, 'fact');
		p = writeLittleInt32(p, e, 4);
		factChunkOffset = lossless_cast<int>(p - buffer);
		p = writeLittleInt32(p, e, writtenFrameCount);
	}
	
	byteWriter.writeBytes(0, lossless_cast<int>(p - buffer), buffer);
	currentFileSize = lossless_cast<int>(p - buffer);
}

void WAVWriter::writeInDataChunk(int byteOffset, int byteCount, const unsigned char* bytes)
{
	if (dataChunkOffset == 0) {
		unsigned char buffer[8];
		unsigned char* p = buffer;
		unsigned char* e = buffer + 8;
		p = writeBigInt32(p, e, 'data');
		p = writeLittleInt32(p, e, writtenDataChunkSize);
		assert(p == e);
		byteWriter.writeBytes(currentFileSize, lossless_cast<int>(p - buffer), buffer);
		currentFileSize += lossless_cast<int>(p - buffer);
		dataChunkOffset = currentFileSize;
	}
	assert(dataChunkOffset + byteOffset <= currentFileSize);
	if (byteCount > 0) {
		byteWriter.writeBytes(dataChunkOffset + byteOffset, byteCount, bytes);
		if (dataChunkOffset + byteOffset + byteCount > currentFileSize) {
			currentFileSize = dataChunkOffset + byteOffset + byteCount;
		}
	}
}
	
void WAVWriter::writeInterleavedFloatAudio(int offset, int count, const float* frames)
{
	if (!floatOutput) {
		writeFloatToIntAudio(this, channelCount, sampleBits, offset, count, frames);
	} else {
		if (IS_LITTLE_ENDIAN) {
			writeInDataChunk(offset * bytesPerFrame, count * bytesPerFrame
					, reinterpret_cast<const unsigned char*>(frames));
		} else {
			unsigned char buffer[CONVERSION_BUFFER_SIZE];
			unsigned char* p = buffer;
			unsigned char* e = p + CONVERSION_BUFFER_SIZE;
			int o = offset * bytesPerFrame;
			for (int i = 0; i < count * channelCount; ++i) {
				p = writeLittleInt32(p, e, reinterpret_cast<const unsigned int*>(frames)[i]);
				if (p >= e) {
					writeInDataChunk(o, lossless_cast<int>(p - buffer), buffer);
					o += lossless_cast<int>(p - buffer);
					p = buffer;
				}
			}
			writeInDataChunk(o, lossless_cast<int>(p - buffer), buffer);
		}
		if (offset + count > currentFrameCount) currentFrameCount = offset + count;
	}
}

void WAVWriter::writeInterleavedIntAudio(int offset, int count, const int* frames) {
	if (floatOutput) {
		assert(sampleBits == 32);
		writeIntToFloatAudio(this, channelCount, offset, count, frames);
	} else {
		assert(channelCount > 0);
		assert(bytesPerFrame % channelCount == 0);
		const int bytesPerSample = bytesPerFrame / channelCount;
		const int shift = bytesPerSample * 8 - sampleBits;
		assert(shift >= 0);

		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		assert(bytesPerFrame > 0);
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / bytesPerFrame;
		assert(bufferFrameCount > 0);
		const int* s = frames;

		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			const int byteCount = minimum(bufferFrameCount, (count - subOffset)) * bytesPerFrame;
			assert(byteCount > 0);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			// FIX : optimize for little-endian machines, and optimize in general
			switch (bytesPerSample) {
				default: assert(0);
				
				case 1: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						assert(static_cast<unsigned int>(y + 0x80) < 0x100);
						*p++ = lossless_cast<unsigned char>((y + 0x80) & 0xFF);
					}
					break;
				}
				
				case 2: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						assert(static_cast<unsigned int>(y + 0x8000) < 0x10000);
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
					}
					break;
				}
				
				case 3: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						assert(static_cast<unsigned int>(y + 0x800000) < 0x1000000);
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 16) & 0xFF);
					}
					break;
				}

				case 4: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 16) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 24) & 0xFF);
					}
					break;
				}
			}
			writeInDataChunk((offset + subOffset) * bytesPerFrame, byteCount, buffer);
		}
		if (offset + count > currentFrameCount) currentFrameCount = offset + count;
		assert(s == &frames[count * channelCount]);
	}
}

void WAVWriter::flushAudioData() {
	unsigned char buffer[4];

	if (dataChunkOffset == 0) {
		writeInDataChunk(0, 0, 0);
	}

	const int dataChunkSize = currentFileSize - dataChunkOffset;
	if ((dataChunkSize & 1) != 0) {
		buffer[0] = 0;
		writeInDataChunk(dataChunkSize, 1, buffer);
	}

	if (writtenDataChunkSize != dataChunkSize) {
		writeLittleInt32(buffer, buffer + 4, dataChunkSize);
		byteWriter.writeBytes(dataChunkOffset - 4, 4, buffer);
		writtenDataChunkSize = dataChunkSize;
	}

	const int riffChunkSize = currentFileSize - 8;
	if (writtenRIFFChunkSize != riffChunkSize) {
		writeLittleInt32(buffer, buffer + 4, riffChunkSize);
		byteWriter.writeBytes(4, 4, buffer);
		writtenRIFFChunkSize = riffChunkSize;
	}
	
	if (floatOutput) {
		if (writtenFrameCount != currentFrameCount) {
			writeLittleInt32(buffer, buffer + 4, currentFrameCount);
			byteWriter.writeBytes(factChunkOffset, 4, buffer);
			writtenFrameCount = currentFrameCount;
		}
	}
	
	// Double-check that another immediate call to flushAudioData() won't write any data.
	
	assert(dataChunkOffset != 0);
	assert(((writtenDataChunkSize + 1) & ~1) == currentFileSize - dataChunkOffset);
	assert(writtenRIFFChunkSize == currentFileSize - 8);
	assert(!floatOutput || writtenFrameCount == currentFrameCount);
}

WAVWriter::~WAVWriter()
{
	try {
		flushAudioData();
	}
	catch (...) {
		assert(0);
	}
}

/* --- WAVReader --- */

WAVReader::WAVReader(ByteReader& reader)
	: reader(reader)
	, sampleRate(0)
	, frameCount(0)
	, isFloatingPoint(false)
	, sampleBits(0)
	, channelCount(0)
	, bytesPerFrame(0)
	, sampleDataOffset(0)
{
	unsigned char buffer[40];

	reader.readBytes(0, 12, buffer);
	const unsigned char* p = buffer;
	const unsigned char* e = buffer + 12;
	int riffMagic;
	int riffChunkSize;
	int riffFormat;
	p = readBigInt32(p, e, riffMagic);
	p = readLittleInt32(p, e, riffChunkSize);
	p = readBigInt32(p, e, riffFormat);
	if (riffMagic != 'RIFF' || riffChunkSize < 4 || riffFormat != 'WAVE') {
		throw Exception("Invalid WAV file (not valid RIFF WAVE format)");
	}
	int offset = 12;
	int chunkEnd = offset + riffChunkSize - 4;
	bool gotSamples = false;
	bool gotFormat = false;
	int sampleDataSize = 0;
	while (offset < chunkEnd && (!gotFormat || !gotSamples)) {
		reader.readBytes(offset, 8, buffer);
		int chunkID;
		int chunkSize;
		p = buffer;
		e = buffer + 8;
		p = readBigInt32(p, e, chunkID);
		p = readLittleInt32(p, e, chunkSize);
		if (chunkSize < 0) {
			throw Exception("Invalid WAV file (encountered an invalid chunk size)");
		}
		if (chunkID == 'fmt ') {
			if (gotFormat) {
				throw Exception("Invalid WAV file (found more than one format chunk)");
			}
			if (chunkSize < 16) {
				throw Exception("Invalid WAV file (format chunk too small)");
			}
			chunkSize = minimum(chunkSize, 40);
			reader.readBytes(offset + 8, chunkSize, buffer);
			p = buffer;
			e = buffer + chunkSize;
			assert(e <= buffer + sizeof (buffer));
			short formatTag;
			p = readLittleInt16(p, e, formatTag);
			p = readLittleInt16(p, e, channelCount);
			if (channelCount < 0) {
				throw Exception("Invalid WAV file (invalid channel count)");
			}
			p = readLittleInt32(p, e, sampleRate);
			p += 4; // skip bytes per second
			p = readLittleInt16(p, e, bytesPerFrame);
			p = readLittleInt16(p, e, sampleBits);
			isFloatingPoint = false;

			switch (formatTag) {
				case 1: break; // PCM

				case 3: { // IEEE float
					if (sampleBits != 32) {
						throw Exception("Cannot read WAV file (unsupported float format)");
					}
					isFloatingPoint = true;
					break;
				}

				case (short)(0xFFFE): {
					if (chunkSize < 40) {
						throw Exception("Invalid WAV file (format chunk too small)");
					}
					short extensionSize;
					p = readLittleInt16(p, e, extensionSize);
					if (extensionSize < 22) {
						throw Exception("Invalid WAV file (extension size too small)");
					}
					int speakerPositionMask;
					p = readLittleInt16(p, e, sampleBits);
					if (sampleBits < 1 || sampleBits > bytesPerFrame / channelCount * 8) {
						throw Exception("Invalid WAV file (invalid bit resolution)");
					}
					p = readLittleInt32(p, e, speakerPositionMask);
					short newFormatTag;
					p = readLittleInt16(p, e, newFormatTag);
					switch (newFormatTag) {
						case 1: break; // PCM

						case 3: { // IEEE float
							if (sampleBits != 32) {
								throw Exception("Cannot read WAV file (unsupported float format)");
							}
							isFloatingPoint = true;
							break;
						}

						default: throw Exception("Cannot read WAV file (unsupported format tag)");
					}
					break;
				}

				default: throw Exception("Cannot read WAV file (unsupported format tag)");
			}
			if (sampleBits < 1 || sampleBits > 32) {
				throw Exception("Cannot read WAV file (unsupported bit resolution)");
			}
			if (bytesPerFrame != channelCount * ((sampleBits + 7) / 8)) {
				throw Exception("Invalid WAV file (invalid block align)");
			}
			gotFormat = true;
		} else if (chunkID == 'data') {
			if (!gotFormat) {
				throw Exception("Invalid WAV file (data chunk precedes format chunk)");
			}
			if (gotSamples) {
				throw Exception("Invalid WAV file (found more than one data chunk)");
			}
			sampleDataOffset = offset + 8;
			sampleDataSize = chunkSize;
			frameCount = sampleDataSize / bytesPerFrame;
			gotSamples = true;
		}
		offset += chunkSize + 8;
		if (offset >= chunkEnd) {
			break;
		}
		// Align to even byte offset (after we check for eof).
		offset = ((offset + 1) & ~1);
	}
	if (offset > chunkEnd) {
		throw Exception("Invalid WAV file (invalid chunk sizes)");
	}
	if (!gotFormat) {
		throw Exception("Invalid WAV file (missing format chunk)");
	}
}

int WAVReader::getFrameCount() {
	return frameCount;
}

int WAVReader::getChannelCount() {
	return channelCount;
}

double WAVReader::getSampleRate() {
	return sampleRate;
}

bool WAVReader::areSamplesFloat() {
	return isFloatingPoint;
}

int WAVReader::getBitResolution() {
	return sampleBits;
}

void WAVReader::readInterleavedIntAudio(int offset, int count, int* frames) {
	assert(offset >= 0);
	assert(count >= 0);
	assert(frames != 0);
	assert(offset + count <= frameCount);
	if (count == 0) {
		return;
	}
	assert(sampleDataOffset != 0);
	if (isFloatingPoint) {
		assert(sampleBits == 32);
		readFloatToIntAudio(this, channelCount, offset, count, frames);
	} else {
		assert(channelCount > 0);
		assert(bytesPerFrame % channelCount == 0);
		const int bytesPerSample = bytesPerFrame / channelCount;
		const int shift = bytesPerSample * 8 - sampleBits;
		assert(shift >= 0);

		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		assert(bytesPerFrame > 0);
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / bytesPerFrame;
		assert(bufferFrameCount > 0);

		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			int byteCount = minimum(bufferFrameCount, (count - subOffset)) * bytesPerFrame;
			reader.readBytes(sampleDataOffset + (offset + subOffset) * bytesPerFrame, byteCount, buffer);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			if (bytesPerSample == 1) {	// special case in WAV for 8-bit samples centered around 128
				int* d = &frames[subOffset * channelCount];
				while (p < e) {
					*d++ = (*p++ - 128) >> shift;
				}
				assert(d <= &frames[count * channelCount]);
			} else {
				readLittleEndianSamples(p, e, bytesPerSample, shift, &frames[subOffset * channelCount]);
			}
		}
	}
}

void WAVReader::readInterleavedFloatAudio(int offset, int count, float* frames) {
	assert(offset >= 0);
	assert(count >= 0);
	assert(frames != 0);
	assert(offset + count <= frameCount);
	if (count == 0) {
		return;
	}
	assert(sampleDataOffset != 0);
	if (!isFloatingPoint) {
		readIntToFloatAudio(this, channelCount, sampleBits, offset, count, frames);
	} else {
		assert(bytesPerFrame == 4 * channelCount);
		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / (4 * channelCount);
		assert(bufferFrameCount > 0);
		float* d = frames;
		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			int byteCount = minimum(bufferFrameCount, (count - subOffset)) * (4 * channelCount);
			reader.readBytes(sampleDataOffset + (offset + subOffset) * (4 * channelCount), byteCount, buffer);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			// FIX : optimize for little-endian machines
			// Attention: this code expects the C++ representation of float to be in IEEE 754 format!
			assert(sizeof (float) == 4 && sizeof (unsigned int) == 4);
			while (p < e) {
				union { unsigned int ui; float f; } u;
				u.ui = ((static_cast<char>(p[3]) << 24) | (p[2] << 16) | (p[1] << 8) | p[0]);
				*d++ = u.f;
				p += 4;
			}
		}
		assert(d == &frames[count * channelCount]);
	}
}

/* --- AIFFReader --- */

AIFFReader::AIFFReader(ByteReader& reader)
	: reader(reader)
	, sampleRate(0.0f)
	, frameCount(0)
	, format(UNKNOWN)
	, sampleBits(0)
	, channelCount(0)
	, sampleDataOffset(0)
{
	unsigned char buffer[256];

	reader.readBytes(0, 12, buffer);
	const unsigned char* p = buffer;
	const unsigned char* e = buffer + 12;
	int formId;
	int chunkSize;
	int formType;
	p = readBigInt32(p, e, formId);
	p = readBigInt32(p, e, chunkSize);
	p = readBigInt32(p, e, formType);
	if (formId != 'FORM' || chunkSize < 4 || (formType != 'AIFF' && formType != 'AIFC')) {
		throw Exception("Invalid AIFF file (invalid IFF FORM)");
	}
	const bool isAIFFC = (formType == 'AIFC');
	const int formChunkEnd = 8 + chunkSize;
	int offset = 12;
	bool gotSSND = false;
	bool gotCOMM = false;
	while (offset < formChunkEnd && (!gotCOMM || !gotSSND)) {
		reader.readBytes(offset, 8, buffer);
		int chunkID;
		int chunkSize;
		p = buffer;
		e = buffer + 8;
		p = readBigInt32(p, e, chunkID);
		p = readBigInt32(p, e, chunkSize);
		if (chunkSize < 0) {
			throw Exception("Invalid AIFF file (encountered an invalid chunk size)");
		}
		switch (chunkID) {
			case 'COMM': {
				if (gotCOMM) {
					throw Exception("Invalid AIFF file (found more than one COMM chunk)");
				}
				const int commHeaderSize = (isAIFFC ? 23 : 18);
				if (chunkSize < commHeaderSize) {
					throw Exception("Invalid AIFF file (COMM chunk too small)");
				}
				reader.readBytes(offset + 8, commHeaderSize, buffer);
				p = buffer;
				e = buffer + commHeaderSize;
				assert(e <= buffer + sizeof (buffer));
				
				p = readBigInt16(p, e, channelCount);
				p = readBigInt32(p, e, frameCount);
				p = readBigInt16(p, e, sampleBits);
				p = readIEEE80(p, e, sampleRate);

				if (channelCount < 0) {
					throw Exception("Invalid AIFF file (invalid channel count)");
				}
				format = BIG_ENDIAN_PCM;
				
				if (isAIFFC) {
					int compressionType;
					p = readBigInt32(p, e, compressionType);
					bool validType = false;
					switch (compressionType) {
						case 'NONE':
						case 'twos': validType = true; break;
						case 'sowt': format = LITTLE_ENDIAN_PCM; validType = true; break;
						case 'fl32':
						case 'FL32': format = BIG_ENDIAN_FLOAT_32; validType = true; break;
					}
					if (!validType) {
						int compressionNameLength = *p;
						assert(p <= e);
						if (compressionNameLength > 0) {
							reader.readBytes(offset + 8 + commHeaderSize, compressionNameLength, buffer);
							const std::string compressionName(buffer + 0, buffer + compressionNameLength);
							throw Exception(std::string("Cannot read AIFF file (unsupported compression type: ") + compressionName + ")");
						}
						throw Exception("Cannot read AIFF file (unsupported compression type)");
					}
				}

				if (sampleBits < 1 || sampleBits > 32) {
					throw Exception("Cannot read AIFF file (unsupported bit resolution)");
				}

				gotCOMM = true;
				break;
			}
			case 'SSND': {
				if (gotSSND) {
					throw Exception("Invalid AIFF file (found more than one SSND chunk)");
				}
				if (chunkSize < 8) {
					throw Exception("Invalid AIFF file (SSND chunk too small)");
				}
				reader.readBytes(offset + 8, 8, buffer);
				p = buffer;
				e = buffer + 8;
				assert(e <= buffer + sizeof (buffer));
				int blockOffset;
				int blockSize;
				p = readBigInt32(p, e, blockOffset);
				p = readBigInt32(p, e, blockSize);
				if (blockOffset < 0 || blockOffset > chunkSize - 8) {
					throw Exception("Invalid AIFF file (invalid sound data offset)");
				}
				sampleDataOffset = offset + 8 + 8 + blockOffset;
				gotSSND = true;
				break;
			}
		}
		offset += ((chunkSize + 1) & ~1) + 8;
	}
	if (offset > formChunkEnd) {
		throw Exception("Invalid AIFF file (invalid chunk sizes)");
	}
	if (!gotCOMM) {
		throw Exception("Invalid AIFF file (missing COMM chunk)");
	}
	if (!gotSSND && frameCount != 0) {
		throw Exception("Invalid AIFF file (missing data chunk)");
	}
}

int AIFFReader::getFrameCount() {
	return frameCount;
}

int AIFFReader::getChannelCount() {
	return channelCount;
}

double AIFFReader::getSampleRate() {
	return sampleRate;
}

bool AIFFReader::areSamplesFloat() {
	return format == BIG_ENDIAN_FLOAT_32;
}

int AIFFReader::getBitResolution() {
	return sampleBits;
}

void AIFFReader::readInterleavedIntAudio(int offset, int count, int* frames) {
	assert(offset >= 0);
	assert(count >= 0);
	assert(frames != 0);
	assert(offset + count <= frameCount);
	if (count == 0) {
		return;
	}
	assert(sampleDataOffset != 0);
	if (format == BIG_ENDIAN_FLOAT_32) {
		assert(sampleBits == 32);
		return readFloatToIntAudio(this, channelCount, offset, count, frames);
	} else {
		assert(channelCount > 0);
		const int bytesPerSample = (sampleBits + 7) / 8;
		const int bytesPerFrame = bytesPerSample * channelCount;
		assert(bytesPerFrame > 0);
		const int shift = bytesPerSample * 8 - sampleBits;
		assert(shift >= 0);

		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / bytesPerFrame;
		assert(bufferFrameCount > 0);
		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			const int byteCount = minimum(bufferFrameCount, (count - subOffset)) * bytesPerFrame;
			assert(byteCount > 0);
			reader.readBytes(sampleDataOffset + (offset + subOffset) * bytesPerFrame, byteCount, buffer);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			
			switch (format) {
				case BIG_ENDIAN_PCM: {
					readBigEndianSamples(p, e, bytesPerSample, shift, &frames[subOffset * channelCount]);
					break;
				}
				case LITTLE_ENDIAN_PCM: {
					readLittleEndianSamples(p, e, bytesPerSample, shift, &frames[subOffset * channelCount]);
					break;
				}
				default: assert(0);
			}
		}
	}
}

void AIFFReader::readInterleavedFloatAudio(int offset, int count, float* frames) {
	assert(offset >= 0);
	assert(count >= 0);
	assert(frames != 0);
	assert(offset + count <= frameCount);
	if (count == 0) {
		return;
	}
	assert(sampleDataOffset != 0);
	if (format != BIG_ENDIAN_FLOAT_32) {
		readIntToFloatAudio(this, channelCount, sampleBits, offset, count, frames);
	} else {
		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / (4 * channelCount);
		assert(bufferFrameCount > 0);
		float* d = frames;
		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			const int byteCount = minimum(bufferFrameCount, (count - subOffset)) * (4 * channelCount);
			reader.readBytes(sampleDataOffset + (offset + subOffset) * (4 * channelCount), byteCount, buffer);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			// FIX : optimize for big-endian machines
			// Attention: this code expects the C++ representation of float to be in IEEE 754 format!
			assert(sizeof (float) == 4 && sizeof (unsigned int) == 4);
			while (p < e) {
				union { unsigned int ui; float f; } u;
				u.ui = ((static_cast<char>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
				*d++ = u.f;
				p += 4;
			}
		}
		assert(d == &frames[count * channelCount]);
	}
}

/* --- AIFFWriter --- */

AIFFWriter::AIFFWriter(int channelCount, int sampleRate, bool isFloating, int sampleBits, ByteWriter& byteWriter, int estimatedFrameCount)
	: byteWriter(byteWriter)
	, channelCount(channelCount)
	, sampleBits(sampleBits)
	, bytesPerFrame(channelCount * ((sampleBits + 7) / 8))
	, floatOutput(isFloating)
	, writtenSoundDataChunkSize(0)	// FIX : use estimatedFrameCount to estimate the size
	, writtenFormChunkSize(0)
	, writtenFrameCount(estimatedFrameCount)
	, currentFileSize(0)
	, currentFrameCount(0)
	, frameCountOffset(0)
	, soundDataOffset(0)
{
	assert(!isFloating || sampleBits == 32);
	assert(isFloating || sampleBits >= 1 && sampleBits <= 32);
	
	const bool isAIFFC = isFloating;

	unsigned char buffer[256];
	
	unsigned char* p = buffer;
	unsigned char* e = buffer + 256;
	p = writeBigInt32(p, e, 'FORM');
	p = writeBigInt32(p, e, writtenFormChunkSize);
	p = writeBigInt32(p, e, (isAIFFC ? 'AIFC' : 'AIFF'));
	
	if (isAIFFC) {
		p = writeBigInt32(p, e, 'FVER');
		p = writeBigInt32(p, e, 4);
		p = writeBigInt32(p, e, 0xA2805140U);
	}
	
	const int commHeaderSize = (isAIFFC ? 23 : 18);
	p = writeBigInt32(p, e, 'COMM');
	p = writeBigInt32(p, e, commHeaderSize);
	const unsigned char* b = p;
	(void) b;
	p = writeBigInt16(p, e, channelCount);
	frameCountOffset = lossless_cast<int>(p - buffer);
	p = writeBigInt32(p, e, estimatedFrameCount);
	p = writeBigInt16(p, e, sampleBits);
	p = writeIEEE80(p, e, sampleRate);
	assert(p - b == 18);
	if (isAIFFC) {
		p = writeBigInt32(p, e, 'fl32');
		*p++ = 0;
		*p++ = 0;	// pad byte
		assert(p - b == 24);
	}
	byteWriter.writeBytes(0, lossless_cast<int>(p - buffer), buffer);
	currentFileSize = lossless_cast<int>(p - buffer);
}

void AIFFWriter::writeInSoundDataChunk(int byteOffset, int byteCount, const unsigned char* bytes) {
	if (soundDataOffset == 0) {
		unsigned char buffer[16];
		unsigned char* p = buffer;
		unsigned char* e = buffer + 16;
		p = writeBigInt32(p, e, 'SSND');
		p = writeBigInt32(p, e, writtenSoundDataChunkSize);
		p = writeBigInt32(p, e, 0);	// offset
		p = writeBigInt32(p, e, 0);	// block size
		assert(p == e);
		byteWriter.writeBytes(currentFileSize, lossless_cast<int>(p - buffer), buffer);
		currentFileSize += lossless_cast<int>(p - buffer);
		soundDataOffset = currentFileSize;
	}
	assert(soundDataOffset + byteOffset <= currentFileSize);
	if (byteCount > 0) {
		byteWriter.writeBytes(soundDataOffset + byteOffset, byteCount, bytes);
		if (soundDataOffset + byteOffset + byteCount > currentFileSize) {
			currentFileSize = soundDataOffset + byteOffset + byteCount;
		}
	}
}
	
void AIFFWriter::writeInterleavedFloatAudio(int offset, int count, const float* frames) {
	if (!floatOutput) {
		writeFloatToIntAudio(this, channelCount, sampleBits, offset, count, frames);
	} else {
		if (IS_BIG_ENDIAN) {
			writeInSoundDataChunk(offset * bytesPerFrame, count * bytesPerFrame
					, reinterpret_cast<const unsigned char*>(frames));
		} else {
			unsigned char buffer[CONVERSION_BUFFER_SIZE];
			unsigned char* p = buffer;
			unsigned char* e = p + CONVERSION_BUFFER_SIZE;
			int o = offset * bytesPerFrame;
			for (int i = 0; i < count * channelCount; ++i) {
				p = writeBigInt32(p, e, reinterpret_cast<const unsigned int*>(frames)[i]);
				if (p >= e) {
					writeInSoundDataChunk(o, lossless_cast<int>(p - buffer), buffer);
					o += lossless_cast<int>(p - buffer);
					p = buffer;
				}
			}
			writeInSoundDataChunk(o, lossless_cast<int>(p - buffer), buffer);
		}
		if (offset + count > currentFrameCount) {
			currentFrameCount = offset + count;
		}
	}
}

void AIFFWriter::writeInterleavedIntAudio(int offset, int count, const int* frames) {
	if (floatOutput) {
		assert(sampleBits == 32);
		writeIntToFloatAudio(this, channelCount, offset, count, frames);
	} else {
		assert(channelCount > 0);
		assert(bytesPerFrame % channelCount == 0);
		const int bytesPerSample = bytesPerFrame / channelCount;
		const int bytesPerFrame = bytesPerSample * channelCount;
		const int shift = bytesPerSample * 8 - sampleBits;
		assert(shift >= 0);

		unsigned char buffer[CONVERSION_BUFFER_SIZE];
		assert(bytesPerFrame > 0);
		const int bufferFrameCount = CONVERSION_BUFFER_SIZE / bytesPerFrame;
		assert(bufferFrameCount > 0);
		const int* s = frames;
		
		for (int subOffset = 0; subOffset < count; subOffset += bufferFrameCount) {
			const int byteCount = minimum(bufferFrameCount, (count - subOffset)) * bytesPerFrame;
			assert(byteCount > 0);
			unsigned char* p = buffer;
			unsigned char* e = buffer + byteCount;
			// FIX : optimize for big-endian machines, and optimize in general
			switch (bytesPerSample) {
				default: assert(0);
				
				case 1: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						*p++ = static_cast<signed char>(*s++ << shift);
					}
					break;
				}
				
				case 2: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						assert(static_cast<unsigned int>(y + 0x8000) < 0x10000);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
					}
					break;
				}
				
				case 3: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						assert(static_cast<unsigned int>(y + 0x800000) < 0x1000000);
						*p++ = static_cast<unsigned char>((y >> 16) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
					}
					break;
				}

				case 4: {
					while (p < e) {
						assert(((*s) << shift) >> shift == (*s));
						const int y = *s++ << shift;
						*p++ = static_cast<unsigned char>((y >> 24) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 16) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 8) & 0xFF);
						*p++ = static_cast<unsigned char>((y >> 0) & 0xFF);
					}
					break;
				}
			}
			writeInSoundDataChunk((offset + subOffset) * bytesPerFrame, byteCount, buffer);
		}
		if (offset + count > currentFrameCount) {
			currentFrameCount = offset + count;
		}
		assert(s == &frames[count * channelCount]);
	}
}

void AIFFWriter::flushAudioData() {
	unsigned char buffer[4];

	if (soundDataOffset == 0) {
		writeInSoundDataChunk(0, 0, 0);
	}

	const int soundDataSize = currentFileSize - soundDataOffset;
	if ((soundDataSize & 1) != 0) {
		buffer[0] = 0;
		writeInSoundDataChunk(soundDataSize, 1, buffer);
	}

	const int soundDataChunkSize = soundDataSize + 8;
	if (writtenSoundDataChunkSize != soundDataChunkSize) {
		writeBigInt32(buffer, buffer + 4, soundDataChunkSize);
		byteWriter.writeBytes(soundDataOffset - 12, 4, buffer);
		writtenSoundDataChunkSize = soundDataChunkSize;
	}

	const int formChunkSize = currentFileSize - 8;
	if (writtenFormChunkSize != formChunkSize) {
		writeBigInt32(buffer, buffer + 4, formChunkSize);
		byteWriter.writeBytes(4, 4, buffer);
		writtenFormChunkSize = formChunkSize;
	}
	
	if (writtenFrameCount != currentFrameCount) {
		writeBigInt32(buffer, buffer + 4, currentFrameCount);
		byteWriter.writeBytes(frameCountOffset, 4, buffer);
		writtenFrameCount = currentFrameCount;
	}
	
	// Double-check that another immediate call to flushAudioData() won't write any data.
	
	assert(soundDataOffset != 0);
	assert(((writtenSoundDataChunkSize + 1) & ~1) == currentFileSize - (soundDataOffset - 8));
	assert(writtenFormChunkSize == currentFileSize - 8);
	assert(writtenFrameCount == currentFrameCount);
}

AIFFWriter::~AIFFWriter()
{
	try {
		flushAudioData();
	}
	catch (...) {
		assert(0);
	}
}

AudioReader::~AudioReader()
{
}

AudioWriter::~AudioWriter()
{
}

ByteReader::~ByteReader()
{
}

ByteWriter::~ByteWriter()
{
}

} /* namespace NuXAudioFiles */
