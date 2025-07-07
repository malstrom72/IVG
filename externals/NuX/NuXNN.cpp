//
//  NuXNN.cpp
//  NuXNNLab
//
//  Created by Magnus Lidström on 2019-05-29.
//  Copyright © 2019 Magnus Lidström. All rights reserved.
//

#include "assert.h"
#include <algorithm>
#include <NuX/NuXDebug.h>
#include <NuX/NuXSIMD.h>
#include "NuXNN.h"

using namespace NuXSIMD;

namespace NuXNN {

class Sequential : public Layer {
	typedef Layer super;
	public:		Sequential(ByteStream& inputStream, int inputSize);
				virtual int getMinimumBufferSize() const;
				virtual void process(const float* input, float* output, float* bufferMemory) const;
				virtual ~Sequential();
	protected:	void destroyLayers();
				std::vector<Layer*> layers;
				int secondBufferOffset;
				int childBuffersOffset;
				int childBuffersSize;
};

class TimeDistributed : public Layer {
	typedef Layer super;
	public:		TimeDistributed(ByteStream& inputStream, int inputSize);
				virtual int getMinimumBufferSize() const;
				virtual void process(const float* input, float* output, float* bufferMemory) const;
				virtual ~TimeDistributed();
	protected:	int steps;
				Layer* layer;
};

class Embedding : public Layer {
	typedef Layer super;
	public:		Embedding(ByteStream& inputStream, int inputSize, bool halfPrecisionFloats);
				virtual int getMinimumBufferSize() const;
				virtual void process(const float* input, float* output, float* bufferMemory) const;
				virtual ~Embedding();
	protected:	int vocabularySize;
				int embeddingSize;
				const float* weights;
};

class VAELayer : public Layer {
	typedef Layer super;
	public:		VAELayer(ByteStream& inputStream, int inputSize);
				virtual int getMinimumBufferSize() const;
				virtual void process(const float* input, float* output, float* bufferMemory) const;
				virtual ~VAELayer();
	protected:	Layer* meanLayer;
				Layer* logVarLayer;
};

class Transpose : public Layer {
	typedef Layer super;
	public:		Transpose(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
	protected:	int stride;
};

class Dense : public Layer {
	typedef Layer super;
	public:		Dense(ByteStream& inputStream, int inputSize, bool halfPrecisionFloats);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
				virtual ~Dense();
	protected:	void destroyWeights();
				const int weightsStride;
				const float* weights;
				const float* biases;
};

class ReLU : public Layer {
	typedef Layer super;
	public:		ReLU(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
};

class SoftSign : public Layer {
	typedef Layer super;
	public:		SoftSign(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
};

class HardSigmoid : public Layer {
	typedef Layer super;
	public:		HardSigmoid(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
};

class Softmax : public Layer {
	typedef Layer super;
	public:		Softmax(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
};

class LeakyReLU : public Layer {
	typedef Layer super;
	public:		LeakyReLU(ByteStream& inputStream, int inputSize);
				virtual void process(const float* input, float* output, float* bufferMemory) const;
	protected:	float alpha;
};

// sizeofOutputArray is only used for assert
void processDense(int inputCount, int outputCount, const float* input, int weightsStride, const float* weights
		, const float* biases, size_t sizeofOutputArray, float* output) {
	assert(static_cast<size_t>(outputCount) * sizeof (float) <= sizeofOutputArray);
	assert(input != output);

	if (!isAligned(input) || !isAligned(weights) || (weightsStride & 3) != 0) {
		for (int outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
			const float* w = weights + weightsStride * outputIndex;
			float s = biases[outputIndex];
			for (int inputIndex = 0; inputIndex < inputCount; ++inputIndex) {
				s += *w * input[inputIndex];
				++w;
			}
			output[outputIndex] = s;
		}
	} else {
		for (int outputIndex = 0; outputIndex < outputCount; ++outputIndex) {
			const QFloat* w4 = reinterpret_cast<const QFloat*>(weights + weightsStride * outputIndex);
			QFloat s0_4 = CONST_QFLOAT(0.0f);
			QFloat s1_4 = CONST_QFLOAT(0.0f);
			QFloat s2_4 = CONST_QFLOAT(0.0f);
			QFloat s3_4 = CONST_QFLOAT(0.0f);
			const QFloat* i4 = reinterpret_cast<const QFloat*>(input);
			const QFloat* ie4 = i4 + inputCount / 4;
			while (i4 + 4 <= ie4) {
				s0_4 = mulAdd(w4[0], i4[0], s0_4);
				s1_4 = mulAdd(w4[1], i4[1], s1_4);
				s2_4 = mulAdd(w4[2], i4[2], s2_4);
				s3_4 = mulAdd(w4[3], i4[3], s3_4);
				i4 += 4;
				w4 += 4;
			}
			QFloat s4 = add(add(s0_4, s1_4), add(s2_4, s3_4));
			while (i4 < ie4) {
				s4 = mulAdd(*w4, *i4, s4);
				++i4;
				++w4;
			}
			SIMD_ALIGN(float expanded[4]);
			storeAligned(s4, expanded);
			float sum = expanded[0] + expanded[1] + expanded[2] + expanded[3];
			const float* w = reinterpret_cast<const float*>(w4);
			const float* i = reinterpret_cast<const float*>(i4);
			const float* ie = input + inputCount;
			while (i < ie) {
				sum += *w * *i;
				++i;
				++w;
			}
			output[outputIndex] = biases[outputIndex] + sum;
		}
	}
}

void processReLU(int count, const float* input, float* output) {
	for (int i = 0; i < count; ++i) {
		output[i] = relu(input[i]);
	}
}

void processSoftSign(int count, const float* input, float* output) {
	for (int i = 0; i < count; ++i) {
		output[i] = softSign(input[i]);
	}
}

void processHardSigmoid(int count, const float* input, float* output) {
	for (int i = 0; i < count; ++i) {
		output[i] = hardSigmoid(input[i]);
	}
}

void processLeakyReLU(int count, const float* input, float* output, float alpha) {
	for (int i = 0; i < count; ++i) {
		assert(!isNaN(input[i]));
		output[i] = leakyReLU(input[i], alpha);
	}
}

void processSoftmax(int count, const float* input, float* output, float temperature) {
	assert(count > 0);
	assert(count > 0);
	const float rcpTemperature = 1.0f / temperature;
	float sum = 0.0f;
	for (int i = 0; i < count; ++i) {
		output[i] = input[i] * rcpTemperature;
	}
	float maxi = output[0];
	for (int i = 1; i < count; ++i) {
		maxi = std::max(maxi, output[i]);
	}
	for (int i = 0; i < count; ++i) {
		const float y = exp(output[i] - maxi);
		assert(!isNaN(y) && y > 0.0 && y < 1e10);
		sum += y;
		output[i] = y;
	}
	assert(sum > 0.0f);
	const float g = 1.0f / sum;
	bool ok = false;
	for (int i = 0; i < count; ++i) {
		output[i] *= g;
		if (output[i] > 0.0f) {
			ok = true;
		}
		assert(!isNaN(output[i]));
	}
	assert(ok);
}

void processSoftmax(int count, const double* input, double* output, double temperature) {
	assert(count > 0);
	const double rcpTemperature = 1.0 / temperature;
	double sum = 0.0;
	for (int i = 0; i < count; ++i) {
		output[i] = input[i] * rcpTemperature;
	}
	double maxi = output[0];
	for (int i = 1; i < count; ++i) {
		maxi = std::max(maxi, output[i]);
	}
	for (int i = 0; i < count; ++i) {
		const double y = exp(output[i] - maxi);
		assert(!isNaN(y));
		assert(y >= 0.0);
		assert(y < 1e30);
		sum += y;
		output[i] = y;
	}
	assert(sum > 0.0);
	const double g = 1.0 / sum;
	bool ok = false;
	for (int i = 0; i < count; ++i) {
		output[i] *= g;
		ok = ok || (output[i] > 0.0);
		assert(!isNaN(output[i]));
	}
	assert(ok);
}

#if (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) \
    	|| defined(__LITTLE_ENDIAN__) \
    	|| defined(__ARMEL__) \
    	|| defined(__THUMBEL__) \
    	|| defined(__AARCH64EL__) \
    	|| defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) \
    	|| defined(_M_IX86) || defined(_M_X64) || defined(_M_IA64) || defined(_M_ARM)
	#define IS_LITTLE_ENDIAN 1
#else
	#define IS_LITTLE_ENDIAN 0
#endif

static unsigned int asUnsignedInt16(const unsigned char* p) {
	assert(sizeof (unsigned short) == 2);
#if (IS_LITTLE_ENDIAN)
	union U { unsigned char b[2]; unsigned short s; };
	return reinterpret_cast<const U*>(p)->s;
#else
	return (p[1] << 8) | p[0];
#endif
}

static unsigned int asUnsignedInt32(const unsigned char* p) {
	assert(sizeof (unsigned int) == 4);
#if (IS_LITTLE_ENDIAN)
	union U { unsigned char b[4]; unsigned int i; };
	return reinterpret_cast<const U*>(p)->i;
#else
	return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
#endif
}

static float asFloat32(const unsigned char* p) {
	assert(sizeof (float) == 4);
	union U { unsigned char b[4]; unsigned int ui; float f; };
#if (IS_LITTLE_ENDIAN)
	return reinterpret_cast<const U*>(p)->f;
#else
	U u;
	u.ui = asUnsignedInt32(p);
	return u.f;
#endif
}

unsigned char ByteStream::readByte() {
	unsigned char buffer[1];
	readBytes(1, buffer);
	return buffer[0];
}

unsigned int ByteStream::readUnsignedInt32() {
	assert(sizeof (unsigned int) == 4);
	unsigned char buffer[4];
	readBytes(4, buffer);
	return asUnsignedInt32(buffer);
}

float ByteStream::readFloat32() {
	assert(sizeof (float) == 4);
	unsigned char buffer[4];
	readBytes(4, buffer);
	return asFloat32(buffer);
}

void ByteStream::readFloat32s(const int n, float* const floats) {
	assert(sizeof (float) == 4);
#if (IS_LITTLE_ENDIAN)
	readBytes(n * 4, reinterpret_cast<unsigned char*>(floats));
#else
	unsigned char buffer[2048];
	int offset = 0;
	while (offset < n) {
		const int count = minimum(n - offset, (int)(sizeof (buffer) / 4));
		readBytes(count * 4, buffer);
		for (int i = 0; i < count; ++i) {
			floats[offset + i] = asFloat32(buffer + i * 4);
		}
		offset += count;
	}
#endif
}

void ByteStream::readFloat16s(const int n, float* const floats) {
	unsigned char buffer[2048];
	int offset = 0;
	while (offset < n) {
		const int count = minimum(n - offset, (int)(sizeof (buffer) / 2));
		readBytes(count * 2, buffer);
		for (int i = 0; i < count; ++i) {
			const unsigned short v = asUnsignedInt16(buffer + i * 2);
			const int exponent = (v >> 10) & 31;
			const float f = ((v & 0x8000) != 0 ? -1.0f : 1.0f)
					* (exponent >= 31 ? std::numeric_limits<float>::infinity()
					: (exponent > 0 ? ldexp(static_cast<float>((v & 0x3FF) + 0x400), exponent - 25)
					: ldexp(static_cast<float>(v & 0x3FF), -24)));
			floats[offset + i] = f;
		}
		offset += count;
	}
}

ByteStream::~ByteStream() { }

ReLU::ReLU(ByteStream&, int inputSize) : super(inputSize) { }

void ReLU::process(const float* input, float* output, float*) const {
	processReLU(outputSize, input, output);
}

SoftSign::SoftSign(ByteStream&, int inputSize) : super(inputSize) { }

void SoftSign::process(const float* input, float* output, float*) const {
	processSoftSign(outputSize, input, output);
}

HardSigmoid::HardSigmoid(ByteStream&, int inputSize) : super(inputSize) { }

void HardSigmoid::process(const float* input, float* output, float*) const {
	processHardSigmoid(outputSize, input, output);
}

Softmax::Softmax(ByteStream&, int inputSize) : super(inputSize) { }

void Softmax::process(const float* input, float* output, float*) const {
	processSoftmax(outputSize, input, output);
}

LeakyReLU::LeakyReLU(ByteStream& inputStream, int inputSize) : super(inputSize), alpha(0.3f) {
	alpha = inputStream.readFloat32();
}

void LeakyReLU::process(const float* input, float* output, float*) const {
	processLeakyReLU(outputSize, input, output, alpha);
}

Dense::Dense(ByteStream& inputStream, int inputSize, bool halfPrecisionFloats)
		: super(inputSize), weights(0), biases(0), weightsStride((inputSize + 3) & ~3) {
	try {
		outputSize = inputStream.readUnsignedInt32();
		float* w = allocateAligned<float>(outputSize * weightsStride);
		weights = w;
		float* b = allocateAligned<float>(outputSize);
		biases = b;
		for (int outputIndex = 0; outputIndex < outputSize; ++outputIndex) {
			if (halfPrecisionFloats) {
				inputStream.readFloat16s(inputSize, w + weightsStride * outputIndex);
			} else {
				inputStream.readFloat32s(inputSize, w + weightsStride * outputIndex);
			}
		}
		if (halfPrecisionFloats) {
			inputStream.readFloat16s(outputSize, b);
		} else {
			inputStream.readFloat32s(outputSize, b);
		}
	}
	catch (...) {
		destroyWeights();
		throw;
	}
}

void Dense::process(const float* input, float* output, float*) const {
	processDense(inputSize, outputSize, input, weightsStride, weights, biases, inputSize * outputSize * sizeof (float)
			, output);
}

void Dense::destroyWeights() {
	freeAligned(const_cast<float*>(weights));
	weights = 0;
	freeAligned(const_cast<float*>(biases));
	biases = 0;
}

Dense::~Dense() { destroyWeights(); }

Layer::Layer(int inputSize) : inputSize(inputSize), outputSize(inputSize) { }

Layer::~Layer() { }

Sequential::Sequential(ByteStream& inputStream, int inputSize) : super(inputSize)
		, secondBufferOffset(0), childBuffersOffset(0), childBuffersSize(0) {
	try {
		int lastOutputSize = inputSize;
		int sizes[2] = { 0, 0 };
		Layer* l = 0;
		do {
			l = createFromStream(inputStream, lastOutputSize);
			if (l != 0) {
				if (!layers.empty()) {
					const size_t i = layers.size() - 1;
					sizes[i & 1] = maximum(sizes[i & 1], lastOutputSize);
				}
				lastOutputSize = l->getOutputSize();
				childBuffersSize = maximum(childBuffersSize, l->getMinimumBufferSize());
				layers.push_back(l);
			}
		} while (l != 0);
		secondBufferOffset = (sizes[0] + 3) & ~3;
		childBuffersOffset = secondBufferOffset + ((sizes[1] + 3) & ~3);
		outputSize = lastOutputSize;
	}
	catch (...) {
		destroyLayers();
		throw;
	}
}

int Sequential::getMinimumBufferSize() const { return childBuffersOffset + childBuffersSize; }

void Sequential::process(const float* input, float* output, float* bufferMemory) const {
	const float* lastOutput = input;
	const size_t layerCount = layers.size();
	for (size_t i = 0; i < layerCount; ++i) {
		assert(layers[i] != 0);
		float* const thisOutput = (i == layerCount - 1
				? output : ((i & 1) == 0 ? bufferMemory : bufferMemory + secondBufferOffset));
		layers[i]->process(lastOutput, thisOutput, bufferMemory + childBuffersOffset);
		lastOutput = thisOutput;
	}
}

void Sequential::destroyLayers() {
	for (std::vector<Layer*>::iterator it = layers.end(); it != layers.begin();) {
		--it;
		delete *it;
		*it = 0;
	}
	layers.clear();
}

Sequential::~Sequential() { destroyLayers(); }

/* --- TimeDistributed --- */

TimeDistributed::TimeDistributed(ByteStream& inputStream, int inputSize)
		: super(inputSize), steps(0), layer(0) {
	try {
		steps = inputStream.readUnsignedInt32();
		const int stepSize = inputSize / steps;
		if (stepSize * steps != inputSize) {
			throw Exception("Invalid data in NuXNN TimeDistributed layer");
		}
		layer = createFromStream(inputStream, stepSize);
		if (layer == 0) {
			throw Exception("Missing inner layer for NuXNN TimeDistributed layer");
		}
		outputSize = steps * layer->getOutputSize();
	}
	catch (...) {
		delete layer;
		layer = 0;
		throw;
	}
}

int TimeDistributed::getMinimumBufferSize() const { return layer->getMinimumBufferSize(); }

void TimeDistributed::process(const float* input, float* output, float* bufferMemory) const {
	const int inputStepSize = layer->getInputSize();
	const int outputStepSize = layer->getOutputSize();
	for (int i = 0; i < steps; ++i) {
		layer->process(input + inputStepSize * i, output + outputStepSize * i, bufferMemory);
	}
}

TimeDistributed::~TimeDistributed() {
	delete layer;
	layer = 0;
}

/* --- Embedding --- */

Embedding::Embedding(ByteStream& inputStream, int inputSize, bool halfPrecisionFloats)
		: super(inputSize) {
	try {
		vocabularySize = inputStream.readUnsignedInt32();
		embeddingSize = inputStream.readUnsignedInt32();
		const int weightsSize = vocabularySize * embeddingSize;
		weights = allocateAligned<float>(weightsSize);
		if (halfPrecisionFloats) {
			inputStream.readFloat16s(weightsSize, const_cast<float*>(weights));
		} else {
			inputStream.readFloat32s(weightsSize, const_cast<float*>(weights));
		}
		outputSize = inputSize * embeddingSize;
	}
	catch (...) {
		freeAligned(const_cast<float*>(weights));
		weights = 0;
		throw;
	}
}

int Embedding::getMinimumBufferSize() const { return 0; }

void Embedding::process(const float* input, float* output, float* bufferMemory) const {
	(void)bufferMemory;
	for (int i = 0; i < inputSize; ++i) {
		const int index = static_cast<int>(input[i]);
		assert(0 <= index && index < vocabularySize);
		const int tableOffset = clamp(index, 0, vocabularySize - 1) * embeddingSize;
		std::copy(weights + tableOffset, weights + tableOffset + embeddingSize, output + (i * embeddingSize));
	}
}

Embedding::~Embedding() {
	freeAligned(const_cast<float*>(weights));
	weights = 0;
}

/* --- VAELayer --- */

VAELayer::VAELayer(ByteStream& inputStream, int inputSize) : super(inputSize), meanLayer(0), logVarLayer(0) {
	try {
		meanLayer = createFromStream(inputStream, inputSize);
		logVarLayer = createFromStream(inputStream, inputSize);
		assert(logVarLayer->getOutputSize() == meanLayer->getOutputSize());
		outputSize = meanLayer->getOutputSize() + logVarLayer->getOutputSize();
	}
	catch (...) {
		delete meanLayer;
		meanLayer = 0;
		delete logVarLayer;
		logVarLayer = 0;
		throw;
	}
}

int VAELayer::getMinimumBufferSize() const {
	return maximum(meanLayer->getMinimumBufferSize(), logVarLayer->getMinimumBufferSize());
}

void VAELayer::process(const float* input, float* output, float* bufferMemory) const {
	meanLayer->process(input, output, bufferMemory);
	logVarLayer->process(input, output + meanLayer->getOutputSize(), bufferMemory);
}

VAELayer::~VAELayer() {
	delete meanLayer;
	meanLayer = 0;
	delete logVarLayer;
	logVarLayer = 0;
}

Transpose::Transpose(ByteStream& inputStream, int inputSize) : super(inputSize), stride(0) {
	stride = inputStream.readUnsignedInt32();
	if (inputSize % stride != 0) {
		throw Exception("Invalid input size / stride for NuXNN Transpose layer");
	}
}

void Transpose::process(const float* input, float* output, float*) const {
	int j = 0;
	for (int i = 0; i < outputSize; ++i) {
		output[i] = input[j];
		j += stride;
		if (j >= inputSize) {
			j -= inputSize - 1;
		}
	}
}

Layer* Layer::createFromStream(ByteStream& inputStream, int inputSize) {
	const unsigned int tag = inputStream.readUnsignedInt32();
	switch (tag) {
		case 0xa7fb7d64U: return new Sequential(inputStream, inputSize);
		case 0x9cb138bcU:
		case 0x5a5591ebU: return new Dense(inputStream, inputSize, tag == 0x9cb138bcU);
		case 0x7ae5068aU: return new VAELayer(inputStream, inputSize);
		case 0xf36cdc69U: return new LeakyReLU(inputStream, inputSize);
		case 0xb31199c7U: return new ReLU(inputStream, inputSize);
		case 0x4f2ef159U: return new Softmax(inputStream, inputSize);
		case 0x988fbaa9U: return new SoftSign(inputStream, inputSize);
		case 0x6cce4e99U: return new TimeDistributed(inputStream, inputSize);
		case 0xa396ebd3U: return new Transpose(inputStream, inputSize);
		case 0xaad272a1U: return new HardSigmoid(inputStream, inputSize);
		case 0xacf23f63U:
		case 0x9dcff7b1U: return new Embedding(inputStream, inputSize, tag == 0xacf23f63U);
		// case 0xd5b8e08eU: return new Sigmoid(inputStream, inputSize);
		// case 0xd9fd8e7bU: return new Tanh(inputStream, inputSize);
		case 0: return 0;
		default: throw Exception("Unknown layer tag in NuXNN");
	}
}

Net::Net(ByteStream& inputStream) : rootLayer(0) {
	const unsigned int magic = inputStream.readUnsignedInt32();
	if (magic != 0x8d77306fU && magic != 0x8d773070U) {
		throw Exception("Invalid NuXNN format");
	}
	if (magic == 0x8d773070U) {
		unsigned char nameLength[1];
		inputStream.readBytes(1, nameLength);
		std::vector<unsigned char> nameBuffer(nameLength[0]);
		if (!nameBuffer.empty()) {
			inputStream.readBytes(nameLength[0], nameBuffer.data());
			name = std::string(nameBuffer.begin(), nameBuffer.end());
		}
		created = inputStream.readUnsignedInt32();
	}
	const unsigned int inputSize = inputStream.readUnsignedInt32();
	rootLayer = Layer::createFromStream(inputStream, inputSize);
	if (rootLayer == 0) {
		throw Exception("Missing NuXNN root layer");
	}
}

void Net::predict(const float* input, float* output, float* bufferMemory) const {
	float* allocatedBuffer = 0;
	try {
		if (bufferMemory == 0) {
			const int bufferSize = getMinimumBufferSize();
			if (bufferSize != 0) {
				allocatedBuffer = allocateAligned<float>(bufferSize);
			}
		}
		rootLayer->process(input, output, (allocatedBuffer != 0 ? allocatedBuffer : bufferMemory));
		if (allocatedBuffer != 0) {
			freeAligned(allocatedBuffer);
			allocatedBuffer = 0;
		}
	}
	catch (...) {
		if (allocatedBuffer != 0) {
			freeAligned(allocatedBuffer);
			allocatedBuffer = 0;
		}
		throw;
	}
}

Net::~Net() {
	delete rootLayer;
	rootLayer = 0;
}

#ifndef NDEBUG
const unsigned char TEST_BYTES[] = {
	0x00, 0x00, 0x01, 0x00, 0xff, 0x03, 0x00, 0x04, 0x55, 0x35, 0xff, 0x3b, 0x00,
	0x3c, 0x01, 0x3c, 0xff, 0x7b, 0x00, 0x7c, 0x00, 0x80, 0x00, 0xc0, 0x00, 0xfc
};

struct MyByteStream : public ByteStream {
	MyByteStream() : offset(0) { }
	virtual void readBytes(int count, unsigned char* bytes) {
		assert(offset + count <= (int)(sizeof (TEST_BYTES) / sizeof (*TEST_BYTES)));
		std::copy(TEST_BYTES + offset, TEST_BYTES + offset + count, bytes);
		offset += count;
	}
	int offset;
};

// used to avoid problems with comparisons to infinity under aggressive floating point optimization
static uint32_t floatToBits(float f) {
    uint32_t bits;
    assert(sizeof (f) == sizeof (bits));
    std::memcpy(&bits, &f, sizeof (f));
    return bits;
}

bool unitTest() {
	MyByteStream byteStream;
	float floats[13];
	byteStream.readFloat16s(13, floats);
	assert(floats[0] == 0.0f);
	assert(floats[1] == 5.960464477539063e-8f);
	assert(floats[2] == 0.00006097555160522461f);
	assert(floats[3] == 0.00006103515625f);
	assert(floats[4] == 0.333251953125f);
	assert(floats[5] == 0.99951171875f);
	assert(floats[6] == 1.0f);
	assert(floats[7] == 1.0009765625f);
	assert(floats[8] == 65504.0f);
	assert(floatToBits(floats[9]) == floatToBits(std::numeric_limits<double>::infinity()));
	assert(floats[10] == -0.0f);
	assert(floats[11] == -2.0f);
	assert(floatToBits(floats[12]) == floatToBits(-std::numeric_limits<double>::infinity()));
	return true;
}

#endif // NDEBUG

} /* namespace NuXNN */

#ifndef NDEBUG
#ifdef REGISTER_UNIT_TEST
REGISTER_UNIT_TEST(NuXNN::unitTest)
#endif
#endif

