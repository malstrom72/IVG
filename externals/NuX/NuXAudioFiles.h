/*
	NuXFiles is a library for:
	
	???
	
	NuXFiles is part of the NuEdge X-Platform Library / NuX.
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

#ifndef NuXAudioFiles_h
#define NuXAudioFiles_h

#include <exception>
#include <cstring>
#include <string>

namespace NuXAudioFiles {

class Exception : public std::exception {
	public:		Exception(const std::string& errorString) : errorString(errorString) { } // 'errorString' is the string returned by 'what()'.
	public:		virtual const char *what() const throw() { return errorString.c_str(); } // Returns a string describing the error.
	public:		virtual ~Exception() throw() { };
	private:	std::string errorString;
};

class AudioReader {
	public:		virtual int getFrameCount() = 0;
	public:		virtual int getChannelCount() = 0;
	public:		virtual double getSampleRate() = 0;
	public:		virtual bool areSamplesFloat() = 0; // Returns true for floating point format, in which case 'readInterleavedFloatAudio' returns the exact original samples and 'readInterleavedIntAudio' returns 32-bit signed integer values.
	public:		virtual int getBitResolution() = 0;
	public:		virtual void readInterleavedFloatAudio(int offset, int count, float* frames) = 0; // Returns 'count' * <channel count> floating point samples in 'frames' from 'offset' into the wav-file. If file samples are integers they will be normalized to the range -1.0 to 1.0. Multiple channels are returned "interleaved". It is illegal to read outside the range [0,frameCount).
	public:		virtual void readInterleavedIntAudio(int offset, int count, int* frames) = 0; // Returns 'count' * <channel count> integer samples in 'frames' from 'offset' into the wav-file. If file samples are integers, <x> number of least significant bits will be used as returned by 'getBitResolution' (i.e. for a 12-bit original, the samples are between -2048 and 2047). If file samples are floating point, the returned samples will be normalized to 32-bit (incl sign bit) and clipped. Multiple channels are returned "interleaved". Returned data is always signed. It is illegal to read outside the range [0,frameCount).
	public:		virtual ~AudioReader() = 0;
};

class AudioWriter {
	public:		virtual void writeInterleavedFloatAudio(int offset, int count, const float* frames) = 0;
	public:		virtual void writeInterleavedIntAudio(int offset, int count, const int* frames) = 0;
	public:		virtual void flushAudioData() = 0; ///< flushAudioData() SHOULD be called from the implementing object's destructor, but since destructors may not throw you might want to call it explictly before destroying the object to catch any errors (calling flush() more than once is legal).
	public:		virtual ~AudioWriter() = 0;
};

class ByteReader {
	public:		virtual void readBytes(int offset, int count, unsigned char* bytes) = 0;
	public:		virtual ~ByteReader() = 0;
};

class ByteWriter {
	public:		virtual void writeBytes(int offset, int count, const unsigned char* bytes) = 0;
	public:		virtual ~ByteWriter() = 0;
};

class WAVWriter : public AudioWriter {
	public:		WAVWriter(int channelCount, int sampleRate, bool isFloating, int sampleBits, ByteWriter& byteWriter, int estimatedFrameCount = 0);
	public:		virtual void writeInterleavedFloatAudio(int offset, int count, const float* frames);	// offset must be <= currently written length
	public:		virtual void writeInterleavedIntAudio(int offset, int count, const int* frames);		// offset must be <= currently written length
	public:		virtual void flushAudioData(); ///< flushAudioData() is called from the destructor, but since destructors may not throw you might want to call it explictly before destroying the object to catch any errors (calling flushAudioData() more than once is legal).
	public:		virtual ~WAVWriter();
	protected:	void writeInDataChunk(int byteOffset, int byteCount, const unsigned char* bytes);
	protected:	ByteWriter& byteWriter;
	protected:	const short channelCount;
	protected:	const short sampleBits;
	protected:	const short bytesPerFrame;
	protected:	const bool floatOutput;
	protected:	int writtenDataChunkSize;
	protected:	int writtenRIFFChunkSize;
	protected:	int writtenFrameCount;
	protected:	int currentFileSize;
	protected:	int currentFrameCount;
	protected:	int dataChunkOffset;
	protected:	int factChunkOffset;
};

class WAVReader : public AudioReader {
	public:		WAVReader(ByteReader& reader);
	public:		virtual int getFrameCount();
	public:		virtual int getChannelCount();
	public:		virtual double getSampleRate();
	public:		virtual bool areSamplesFloat();
	public:		virtual int getBitResolution();
	public:		virtual void readInterleavedIntAudio(int offset, int count, int* frames);
	public:		virtual void readInterleavedFloatAudio(int offset, int count, float* frames);
	protected:	ByteReader& reader;
	protected:	int sampleRate;
	protected:	int frameCount;
	protected:	bool isFloatingPoint;
	protected:	short sampleBits;
	protected:	short channelCount;
	protected:	short bytesPerFrame;
	protected:	int sampleDataOffset;
};

class AIFFReader : public AudioReader {
	protected:	enum Format {
					UNKNOWN,
					BIG_ENDIAN_PCM,
					LITTLE_ENDIAN_PCM,
					BIG_ENDIAN_FLOAT_32
				};
	public:		AIFFReader(ByteReader& reader);
	public:		virtual int getFrameCount();
	public:		virtual int getChannelCount();
	public:		virtual double getSampleRate();
	public:		virtual bool areSamplesFloat();
	public:		virtual int getBitResolution();
	public:		virtual void readInterleavedIntAudio(int offset, int count, int* frames);
	public:		virtual void readInterleavedFloatAudio(int offset, int count, float* frames);
	protected:	ByteReader& reader;
	protected:	double sampleRate;
	protected:	int frameCount;
	protected:	Format format;
	protected:	short sampleBits;
	protected:	short channelCount;
	protected:	int sampleDataOffset;
};

class AIFFWriter : public AudioWriter {
	public:		AIFFWriter(int channelCount, int sampleRate, bool isFloating, int sampleBits, ByteWriter& byteWriter, int estimatedFrameCount = 0);
	public:		virtual void writeInterleavedFloatAudio(int offset, int count, const float* frames);	// offset must be <= currently written length
	public:		virtual void writeInterleavedIntAudio(int offset, int count, const int* frames);		// offset must be <= currently written length
	public:		virtual void flushAudioData(); ///< flushAudioData() is called from the destructor, but since destructors may not throw you might want to call it explictly before destroying the object to catch any errors (calling flushAudioData() more than once is legal).
	public:		virtual ~AIFFWriter();
	protected:	void writeInSoundDataChunk(int byteOffset, int byteCount, const unsigned char* bytes);
	protected:	ByteWriter& byteWriter;
	protected:	const short channelCount;
	protected:	const short sampleBits;
	protected:	const short bytesPerFrame;
	protected:	const bool floatOutput;
	protected:	int writtenSoundDataChunkSize;
	protected:	int writtenFormChunkSize;
	protected:	int writtenFrameCount;
	protected:	int currentFileSize;
	protected:	int currentFrameCount;
	protected:	int frameCountOffset;
	protected:	int soundDataOffset;
};

} /* namespace NuXAudioFiles */

#endif
