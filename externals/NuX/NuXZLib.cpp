#include "assert.h"
#include <exception>
#include <sstream>
#include <zlib/zlib.h>
#include "NuXZLib.h"

namespace NuXZLib {

template<typename T, typename U> T lossless_cast(U x) {
	assert(static_cast<U>(static_cast<T>(x)) == x);
	return static_cast<T>(x);
}

Exception::Exception(int errorCode, const char* message) : errorCode(errorCode), message(message == 0 ? "" : message) { }
const char* Exception::what() const throw() {
	std::ostringstream ss;
	ss << (message.empty() ? "zlib error" : message) << " [" << errorCode << ']';
	whatString = ss.str();
	return whatString.c_str();
}
Exception::~Exception() throw() { }

class Stream::Impl { public: z_stream zs; };

Stream::Stream() {
	impl = new Impl;
	memset(&impl->zs, 0, sizeof (z_stream));
}

void Stream::setInput(int inputCount, const unsigned char* inputBytes) {
	assert(impl != 0);
	assert(impl->zs.avail_in == 0); // Did you try to call setInput() again without generating output in between?
	assert(inputCount >= 0);
	impl->zs.avail_in = inputCount;
	impl->zs.next_in = (inputCount == 0 ? 0 : const_cast<unsigned char*>(inputBytes));
}

void Stream::setInputEOF() {
	assert(impl != 0);
	impl->zs.avail_in = 0;
	impl->zs.next_in = 0;
}

bool Stream::isAtInputEOF() const {
	assert(impl != 0);
	return (impl->zs.next_in == 0);
}

bool Stream::generateOutput(int outputSpace, unsigned char* outputBytes, int& count) {
	assert(impl != 0);
	assert(outputSpace >= 0);
	if (impl->zs.next_in != 0 && impl->zs.avail_in == 0) return false;
	impl->zs.avail_out = outputSpace;
	impl->zs.next_out = outputBytes;
	int status = process();
	count = outputSpace - impl->zs.avail_out;
	if (status == Z_STREAM_END) impl->zs.next_in = 0;
	else if (status != Z_OK) throw Exception(status, impl->zs.msg);
	return (count != 0);
}

void Stream::close() {
	if (impl != 0) {
		int status = doClose();
		const char* errorMessage = impl->zs.msg;
		delete impl;
		impl = 0;
		if (status != Z_OK) throw Exception(status, errorMessage);
	}
}

int Stream::memoryToMemory(int inputCount, const unsigned char* inputBytes
		, int outputSpace, unsigned char* outputBytes) {
	int outputCount = 0;
	setInput(inputCount, inputBytes);
	int count;
	while (generateOutput(outputSpace - outputCount, outputBytes + outputCount, count))
		outputCount += count;
	setInputEOF();
	while (generateOutput(outputSpace - outputCount, outputBytes + outputCount, count))
		outputCount += count;
	close();
	return outputCount;
}

Stream::~Stream() {
	delete impl;
	impl = 0;
}

Deflater::Deflater(bool gzipFormat, int compressionLevel) {
	int status;
	if (!gzipFormat)
		status = deflateInit(&impl->zs, compressionLevel);
	else
		status = deflateInit2(&impl->zs, compressionLevel, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if (status != Z_OK) throw Exception(status, impl->zs.msg);
}

int Deflater::process() {
	assert(impl != 0);
	return deflate(&impl->zs, (impl->zs.next_in == 0 ? Z_FINISH : Z_NO_FLUSH));
}

int Deflater::doClose() {
	assert(impl != 0);
	return deflateEnd(&impl->zs);
}

Deflater::~Deflater() {
	if (impl != 0) {
		int status = doClose();
		(void)status;
		assert(status == Z_OK || status == Z_DATA_ERROR); // Z_DATA_ERROR indicates all data was not processed, which is normal if we destruct abruptly.
	}
}

Inflater::Inflater() {
	int status = inflateInit2(&impl->zs, 47);
	if (status != Z_OK) throw Exception(status, impl->zs.msg);
}

int Inflater::process() {
	assert(impl != 0);
	return inflate(&impl->zs, (impl->zs.next_in == 0 ? Z_FINISH : Z_NO_FLUSH));
}

int Inflater::doClose() {
	assert(impl != 0);
	return inflateEnd(&impl->zs);
}

Inflater::~Inflater() {
	if (impl != 0) {
		int status = doClose();
		(void)status;
		assert(status == Z_OK || status == Z_DATA_ERROR); // Z_DATA_ERROR indicates all data was not processed, which is normal if we destruct abruptly.
	}
}

};
