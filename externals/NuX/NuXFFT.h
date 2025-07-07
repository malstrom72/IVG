#ifndef NuXFFT_h
#define NuXFFT_h

#include "assert.h"

#if !defined(NUX_FFT_SIMD_SUPPORT)
	#if (_M_IX86_FP || defined(_M_AMD64) || defined(_M_X64))
		#define NUX_FFT_SIMD_SUPPORT 1
		#define NUX_FFT_SIMD_SSE 1
		#define NUX_FFT_SIMD_NEON 0
	#elif (__SSE__)
		#define NUX_FFT_SIMD_SUPPORT 1
		#define NUX_FFT_SIMD_SSE 1
		#define NUX_FFT_SIMD_NEON 0
	#elif (__ARM_NEON__)
		#define NUX_FFT_SIMD_SUPPORT 1
		#define NUX_FFT_SIMD_SSE 0
		#define NUX_FFT_SIMD_NEON 1
	#endif
#endif

#if (NUX_FFT_SIMD_SUPPORT)
	#if (NUX_FFT_SIMD_SSE)
		#include <emmintrin.h>
	#elif (NUX_FFT_SIMD_NEON)
		#include <arm_neon.h>
	#endif
#endif

namespace NuXFFT {
	
#if (NUX_FFT_SIMD_SUPPORT)
	#if defined(_MSC_VER)
		#define NUX_FFT_SIMD_ALIGN(x) __declspec(align(16)) x
	#elif defined(__GNUC__)
		#define NUX_FFT_SIMD_ALIGN(x) x __attribute__ ((aligned (16)))
	#endif
	
	#if (NUX_FFT_SIMD_SSE)
		typedef __m128 QFloat;
		inline bool isSIMDAligned(const void* p) { return ((reinterpret_cast<intptr_t>(p) & 0xF) == 0); }
		QFloat* allocateSIMD(size_t size);
		void freeSIMD(QFloat* p);
		inline QFloat loadSIMD(const float y[4]) { assert(isSIMDAligned(y)); return _mm_load_ps(y); }
		inline QFloat loadSIMD(float y) { return _mm_set_ps1(y); }
		inline QFloat addSIMD(QFloat x, QFloat y) { return _mm_add_ps(x, y); }
		inline QFloat subSIMD(QFloat x, QFloat y) { return _mm_sub_ps(x, y); }
		inline QFloat mulSIMD(QFloat x, QFloat y) { return _mm_mul_ps(x, y); }
		inline void storeSIMD(QFloat x, float y[4]) { assert(isSIMDAligned(y)); _mm_store_ps(y, x); }
	#elif (NUX_FFT_SIMD_NEON)
		typedef float32x4_t QFloat;
		inline bool isSIMDAligned(const void* p) { return ((reinterpret_cast<intptr_t>(p) & 0xF) == 0); }
		QFloat* allocateSIMD(size_t size);
		void freeSIMD(QFloat* p);
		inline QFloat loadSIMD(const float y[4]) { assert(isSIMDAligned(y)); return vld1q_f32(y); }
		inline QFloat loadSIMD(float y) { return vdupq_n_f32(y); }
		inline QFloat addSIMD(QFloat x, QFloat y) { return vaddq_f32(x, y); }
		inline QFloat subSIMD(QFloat x, QFloat y) { return vsubq_f32(x, y); }
		inline QFloat mulSIMD(QFloat x, QFloat y) { return vmulq_f32(x, y); }
		inline void storeSIMD(QFloat x, float y[4]) { assert(isSIMDAligned(y)); vst1q_f32(y, x); }
	#else
		#error Something is messed up.
	#endif
#endif // (NUX_FFT_SIMD_SUPPORT)

/*
	Performs in-place FFT transform on complex numbers.

	n is number of float elements (i.e. number of complex pairs * 2)
	data is an array of complex pairs, i.e. data[0] = real(element[0]), data[1] = imag(element[0])
	n must be a power of 2
*/
template<typename T> void complexFFT(int n, T data[]);

/*
	Performs faster FFT of real valued data.
	
	n is number of real values (i.e. data array should be n elements)
	n must be a power of 2
 
 	on return:
 
	data[0] = dc
	data[1] = nyquist
	data[2..n-1] are complex pairs

	amplitude = sqrt(square(data[bin * 2 + 0]) + square(data[bin * 2 + 1])) * 2.0 / n;
	phase = atan2(data[bin * 2 + 0], -data[bin * 2 + 1]);
*/
template<typename T> void realFFT(int n, T data[]);
template<typename T> void inverseRealFFT(int n, T data[]);

} /* namespace NuXFFT */

#endif
