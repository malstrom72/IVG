//
//  NuXNN.h
//  NuXNNLab
//
//  Created by Magnus Lidström on 2019-05-29.
//  Copyright © 2019 Magnus Lidström. All rights reserved.
//

#ifndef NuXNN_h
#define NuXNN_h

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace NuXNN {

struct Exception : public std::runtime_error {
	Exception(const char* errorString) : std::runtime_error(errorString) { }
};

template<typename T> const T& minimum(const T& a, const T& b) { return (a < b) ? a : b; };
template<typename T> const T& maximum(const T& a, const T& b) { return (a > b) ? a : b; };
template<typename T> bool isNaN(const T& d) { return d != d; }

inline float leakyReLU(float x, float alpha = 0.3f) { return x * (x <= 0.0f ? alpha : 1.0f); }
inline float relu(float x) { return maximum(x, 0.0f); }
inline float softSign(float x) { return x / (fabsf(x) + 1.0f); }
inline float hardSigmoid(float x) { return std::min(std::max(0.5f + x * 0.2f, 0.0f), 1.0f); }

void processDense(int inputCount, int outputCount, const float* input, int weightsStride, const float* weights
		, const float* biases, size_t sizeofOutputArray, float* output);	// input may not be == output, weightsStride should be inputCount or (inputCount + 3) & ~3 for SIMD alignment
void processReLU(int count, const float* input, float* output);	// input may be == output
void processSoftSign(int count, const float* input, float* output);	// input may be == output
void processHardSigmoid(int count, const float* input, float* output);	// input may be == output
void processLeakyReLU(int count, const float* input, float* output, float alpha = 0.3f);	// input may be == output
void processSoftmax(int count, const float* input, float* output, float temperature = 1.0);	// input may be == output
void processSoftmax(int count, const double* input, double* output, double temperature = 1.0);	// input may be == output

class ByteStream {
	public:		virtual void readBytes(int count, unsigned char* bytes) = 0;
				unsigned char readByte();
				unsigned int readUnsignedInt32();
				float readFloat32();
				void readFloat32s(int n, float* floats);
				void readFloat16s(int n, float* floats);
				virtual ~ByteStream();
};

class Layer {
	public:		static Layer* createFromStream(ByteStream& inputStream, int inputSize);
				Layer(int inputSize);
				int getInputSize() const { return inputSize; }
				int getOutputSize() const { return outputSize; }
				virtual int getMinimumBufferSize() const { return 0; }
				virtual void process(const float* input, float* output, float* bufferMemory) const = 0;
				virtual ~Layer();
	
	protected:	const int inputSize;
				int outputSize;
};

class Net {
	public:		Net(ByteStream& inputStream);
				int getInputSize() const { return rootLayer->getInputSize(); }
				int getOutputSize() const { return rootLayer->getOutputSize(); }
				int getMinimumBufferSize() const { return rootLayer->getMinimumBufferSize(); }
				std::string getName() const { return name; }
				time_t getCreationDate() const { return created; }	// 0 = unknown
				void predict(const float* input, float* output, float* bufferMemory = 0) const;	// bufferMemory is optional and will be temporarily allocated and freed automatically if omitted (or passed as null).
				virtual ~Net();
	protected:	Layer* rootLayer;
				std::string name;	// utf8
				time_t created;
	private:	Net(const Net& copy); // N/A
				Net& operator=(const Net& copy); // N/A
};

#ifndef NDEBUG
bool unitTest();
#endif

} /* namespace NuXNN */

#endif /* NuXNN_h */
