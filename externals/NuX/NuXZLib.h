/**
	\file NuXZLib.h
*/

#ifndef NuXZLib_h
#define NuXZLib_h

#include <exception>
#include <string>

namespace NuXZLib {

class Exception : public std::exception {
	public:		Exception(int errorCode, const char* message);
	public:		virtual const char* what() const throw();
	public:		virtual ~Exception() throw();
	public:		int errorCode;
	public:		std::string message;	
	public:		mutable std::string whatString;
};

/**
	Stream is a private super class. You should instantiate Deflater (for compression) or Inflater (for decompression).
	
	Regardless if you compress or decompress you use the methods defined in this class to provide input data and
	generate output data.
	
	Here is an example of how to do this:
	
		{
			unsigned char inputBuffer[1024];
			unsigned char outputBuffer[1024];

			do {
				if (inStream.bad()) throw Exception("Problem with input stream");
				if (!inStream.good()) zstream.setInputEOF();
				else {
					inStream.read(reinterpret_cast<char*>(inputBuffer), 1024);
					assert(inStream.gcount() > 0);
					zstream.setInput(static_cast<int>(inStream.gcount()), inputBuffer);
				}
				int count;
				while (zstream.generateOutput(1024, outputBuffer, count))
					outStream.write(reinterpret_cast<const char*>(outputBuffer), count);
			} while (!zstream.isAtInputEOF());
		}
	
	Alternatively, if the entire file fits into memory you can call memoryToMemory(). In this case, you should not call
	any other method before or after.
*/
class Stream {
				/**
					Provides input bytes for compression (Deflater) or decompression (Inflater). After calling
					setInput() you should not call it or setInputEOF() again before consuming the generated output with
					generateOutput().
					
					Calling with \p inputCount == 0 is exactly the same as calling setInputEOF()
				*/
	public:		void setInput(int inputCount, const unsigned char* inputBytes);
				
				/**
					Signals that there will be no more input. After a call to this method you should never call
					setInput() again on this instance. You should keep generating output though until generateOutput()
					returns false. Notice that for inflating (decompression) the stream will automatically enter EOF
					state when the compressed input stream is exhausted. But it never hurts to explicitly call this
					method too.
				*/
	public:		void setInputEOF();
	
				/**
					Returns true if either setInputEOF() has been called or a logical end of stream has been encountered
					(on decompression).
					
					You normally keep on processing until isAtInputEOF() returns true.
				*/
	public:		bool isAtInputEOF() const;
	
				/**
					Consume input and generate compressed or decompressed output. Before calling this routine you need
					to have provided input with setInput() and setInputEOF(). False is returned if no more output can be
					generated, either because more input is needed or because the output is finished. Continue calling
					generateOutput() until it returns false as there may be more output waiting to be flushed.
					
					\p outputSpace must be greater than 0. \p count will contain the actual number of bytes available
					in \p outputBytes.
				*/
	public:		bool generateOutput(int outputSpace, unsigned char* outputBytes, int& count);
	
				/**
					Explicitly closes the stream and performs some validity checks (that all generate output has been
					consumed etc). Can and will throw exceptions on failure, and this is the main purpose of having
					this method. Otherwise the Stream destructor will automatically close the stream, but it will never
					throw exceptions (since it is a destructor).
					
					After calling this method you should never call any other method on this instance (but it is ok to
					call close() more than once).
				*/
	public:		void close();
	
				/**
					As an alternative to buffered streaming using the other methods you can perform the full
					compression / decompression by calling this method. The returned value is the actual length of the
					compressed or decompressed data. If \p outputSpace was not sufficiently large an exception will be
					thrown (errorCode: Z_BUF_ERROR / -5).
				*/
	public:		int memoryToMemory(int inputCount, const unsigned char* inputBytes
						, int outputSpace, unsigned char* outputBytes);
	
				/**
					Destructs, closes the output stream and frees all allocated temporary memory.
				*/
	public:		virtual ~Stream();

	protected:	class Impl;
	protected:	Stream();
	protected:	virtual int process() = 0;
	protected:	virtual int doClose() = 0;
	protected:	Impl* impl;
};

/**
	Instantiate Deflater for compressing data. See Stream for general info on how to use the class.
*/
class Deflater : public Stream {
				/**
					Construct Deflater with \p gzipFormat == true to create a gzip-compatible file with a standard
					gzip header (filled with valid dummy data). The gzip file format can be identified with the header
					0x1f 0x8b. The last 4 bytes in a gzip file is the uncompressed file size in little endian byte
					order. See http://www.ietf.org/rfc/rfc1952.txt for more info on gzip.
					
					/p compressionLevel is 0 (no compression, fast) to 9 (max compression, slow).
				*/
	public:		Deflater(bool gzipFormat = false, int compressionLevel = 6);
	public:		virtual ~Deflater();
	protected:	virtual int process();
	protected:	virtual int doClose();
};

/**
	Instantiate Inflater for decompressing data. See Stream for general info on how to use the class.
*/
class Inflater : public Stream {
				/**
					No need to specify if gzip format or not with the Inflater. It is automatically detected.
				*/
	public:		Inflater();
	public:		virtual ~Inflater();
	protected:	virtual int process();
	protected:	virtual int doClose();
};

};

#endif
