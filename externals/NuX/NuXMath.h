#ifndef NuXMath_h
#define NuXMath_h

#include "assert.h"

#if (defined(__INTEL_COMPILER))

	#include <mathimf.h>
	
	// mathimf.h doesn't declare overloaded float-versions
	
	__forceinline float abs(float x) throw() { return fabsf(x); }
	__forceinline double abs(double x) throw() { return fabs(x); }
	__forceinline float exp(float x) throw() { return expf(x); }
	__forceinline float exp2(float x) throw() { return exp2f(x); }
	__forceinline float exp10(float x) throw() { return exp10f(x); }
	__forceinline float pow(float x, float y) throw() { return powf(x, y); }
	__forceinline float log(float x) throw() { return logf(x); }
	__forceinline float log2(float x) throw() { return log2f(x); }
	__forceinline float log10(float x) throw() { return log10f(x); }
	__forceinline float sqrt(float x) throw() { return sqrtf(x); }
	__forceinline float cbrt(float x) throw() { return cbrtf(x); }
	__forceinline float sin(float x) throw() { return sinf(x); }
	__forceinline float cos(float x) throw() { return cosf(x); }
	__forceinline float tan(float x) throw() { return tanf(x); }
	__forceinline float asin(float x) throw() { return asinf(x); }
	__forceinline float acos(float x) throw() { return acosf(x); }
	__forceinline float atan(float x) throw() { return atanf(x); }
	__forceinline float atan2(float x, float y) throw() { return atan2f(x, y); }
	__forceinline float ceil(float x) throw() { return ceilf(x); }
	__forceinline float floor(float x) throw() { return floorf(x); }
//	__forceinline float round(float x) throw() { return roundf(x); }
	__forceinline float fmod(float x, float y) throw() { return fmodf(x, y); }

	#if (defined(__GNUC__))
		template<typename T> __forceinline T min(T x, T y) throw() { return x <? y; }
		template<typename T> __forceinline T max(T x, T y) throw() { return x >? y; }
	#endif
	
	#if (defined(_MSC_VER))
		// Intel vector-optimizes __min and __max inside loops, but not std::min / std::max since they are function calls.
		#define min __min
		#define max __max
	#endif

#elif (defined(__GNUC__))

	#include <cmath>

	// gcc doesn't declare overloaded float-versions
	// update 20190818: it does since SDK 10.9

	using std::abs;
#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1090
	inline __attribute__((always_inline)) float exp(float x) throw() { return expf(x); }
	inline __attribute__((always_inline)) float exp2(float x) throw() { return exp2f(x); }
	inline __attribute__((always_inline)) float exp10(float x) throw() { return expf(float(M_LN10) * x); }
#if !defined __clang__
	inline __attribute__((always_inline)) double exp10(double x) throw() { return exp(M_LN10 * x); }
#endif
	inline __attribute__((always_inline)) float pow(float x, float y) throw() { return powf(x, y); }
	inline __attribute__((always_inline)) float log(float x) throw() { return logf(x); }
	inline __attribute__((always_inline)) float log2(float x) throw() { return log2f(x); }
	inline __attribute__((always_inline)) float log10(float x) throw() { return log10f(x); }
	inline __attribute__((always_inline)) float sqrt(float x) throw() { return sqrtf(x); }
	inline __attribute__((always_inline)) float cbrt(float x) throw() { return cbrtf(x); }
	inline __attribute__((always_inline)) float sin(float x) throw() { return sinf(x); }
	inline __attribute__((always_inline)) float cos(float x) throw() { return cosf(x); }
	inline __attribute__((always_inline)) float tan(float x) throw() { return tanf(x); }
	inline __attribute__((always_inline)) float asin(float x) throw() { return asinf(x); }
	inline __attribute__((always_inline)) float acos(float x) throw() { return acosf(x); }
	inline __attribute__((always_inline)) float atan(float x) throw() { return atanf(x); }
	inline __attribute__((always_inline)) float atan2(float x, float y) throw() { return atan2f(x, y); }
	inline __attribute__((always_inline)) float ceil(float x) throw() { return ceilf(x); }
	inline __attribute__((always_inline)) float floor(float x) throw() { return floorf(x); }
//	inline __attribute__((always_inline)) float round(float x) throw() { return roundf(x); }
	inline __attribute__((always_inline)) float fmod(float x, float y) throw() { return fmodf(x, y); }
#endif

#if (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 2))
	template<typename T> inline __attribute__((always_inline)) T min(T x, T y) throw() { return x <? y; }
	template<typename T> inline __attribute__((always_inline)) T max(T x, T y) throw() { return x >? y; }
#else
	template<typename T, typename U> inline __attribute__((always_inline)) T min(T x, U y) throw() { return x < y ? x : y; }
	template<typename T, typename U> inline __attribute__((always_inline)) T max(T x, U y) throw() { return x < y ? y : x; }
#if 0 // These do not get translated to minss and maxss, probably because of different NAN rules
	inline __attribute__((always_inline)) float min(float x, float y) throw() { return fminf(x, y); }
	inline __attribute__((always_inline)) float max(float x, float y) throw() { return fmaxf(x, y); }
	inline __attribute__((always_inline)) double min(float x, double y) throw() { return fmin(x, y); }
	inline __attribute__((always_inline)) double max(float x, double y) throw() { return fmax(x, y); }
	inline __attribute__((always_inline)) double min(double x, float y) throw() { return fmin(x, y); }
	inline __attribute__((always_inline)) double max(double x, float y) throw() { return fmax(x, y); }
	inline __attribute__((always_inline)) double min(double x, double y) throw() { return fmin(x, y); }
	inline __attribute__((always_inline)) double max(double x, double y) throw() { return fmax(x, y); }
#endif
#endif

#elif (defined(_MSC_VER))

	#include <math.h>
	#include <stdlib.h>
	#include <algorithm>
	
	#if (_MSC_VER < 1400)
		__forceinline float abs(float x) throw() { return fabsf(x); }
		__forceinline double abs(double x) throw() { return fabs(x); }
	#endif

	#undef min
	#undef max

	template<typename T, typename U> inline __forceinline T min(T x, U y) throw() { return x < y ? x : y; }
	template<typename T, typename U> inline __forceinline T max(T x, U y) throw() { return x < y ? y : x; }
	template<typename T> __forceinline T log2(T x) throw() { return log(x) * static_cast<T>(1.44269504088896340736); }

#else

	#include <math.h>

	inline float abs(float x) throw() { return fabsf(x); }
	inline double abs(double x) throw() { return fabs(x); }

#endif

/* --- Math --- */

#include <iostream>

namespace NuXMath {

	const double SQRT05 = 0.70710678118654752440084436210484;
	const float SQRT05f = 0.70710678118654752440084436210484f;
	const double SQRT2 = 1.4142135623730950488016887242096;
	const float SQRT2f = 1.4142135623730950488016887242096f;
	const double PI = 3.1415926535897932384626433832795;
	const float PIf = 3.1415926535897932384626433832795f;
	const double PI2 = 6.2831853071795864769252867665590;
	const float PI2f = 6.2831853071795864769252867665590f;
	const double EULER = 2.7182818284590452353602874713526;
	const float EULERf = 2.7182818284590452353602874713526f;
	const double LN2 = 0.69314718055994530941723212145817;
	const float LN2f = 0.69314718055994530941723212145817f;

	inline int mod(int x, int y) throw() { return x % y; }
	inline float mod(float x, float y) throw() { return fmodf(x, y); }
	inline double mod(double x, double y) throw() { return fmod(x, y); }

// FIX : not really clang problem I think, rather PH LLVM problem
#if !defined __clang__
	template<typename T> T powopt(T x, T y) throw() { assert(x > 0); return exp(y * log(x)); }
#else
	#define powopt pow
#endif
	// FIX : rename to roundHalfUp
	template<typename T> T roundUp(T x) throw() { return floor(x + (T)(0.5)); }
	template<typename T> T square(T x) throw() { return x * x; }
	template<typename T> T cube(T x) throw() { return x * x * x; }
	template<typename T> T pow2(T x) throw() { return x * x; }
	template<typename T> T pow3(T x) throw() { return x * x * x; }
	template<typename T> T pow4(T x) throw() { return pow2(pow2(x)); }
	template<typename T> T pow5(T x) throw() { return pow4(x) * x; }
	template<typename T> T pow6(T x) throw() { return pow2(pow3(x)); }
	template<typename T> T pow7(T x) throw() { return pow6(x) * x; }
	template<typename T> T pow8(T x) throw() { return pow2(pow4(x)); }
	template<typename T> T sign(T x) throw() { return (x < 0) ? static_cast<T>(-1) : ((x > 0) ? static_cast<T>(1) : 0); }
	template<typename T, typename U> T minimum(T x, U y) throw() { return min(x, y); } // Some compilers find it hard to optimize stl's min and max
	template<typename T, typename U> T maximum(T x, U y) throw() { return max(x, y); } // Some compilers find it hard to optimize stl's min and max
	template<typename T> T clamp(T x, T mini, T maxi) throw() { assert(mini <= maxi); return min(max(x, mini), maxi); }
	template<typename T> T gate(T x, T threshold) throw() { return (abs(x) < threshold) ? 0 : x; }
	template<typename T> bool inRange(T x, T mini, T maxi) throw() { return (mini <= maxi) ? (x >= mini && x <= maxi) : (x >= maxi && x <= mini); }
	template<typename T> T fract(T x) throw() { return x - floor(x); }
	
	#if (!defined(__INTEL_COMPILER))
		template<typename T> T exp10(T x) throw() { return exp(x * static_cast<T>(2.3025850929940456840)); }
	#endif
	#if (defined(__INTEL_COMPILER) || defined(__APPLE__))
//		template<typename T> T round(T y) throw() { return ::round(y); }
	#elif !(defined(__INTEL_COMPILER) || defined(__APPLE__))
//		template<typename T> T round(T y) throw() { return floor(y + static_cast<T>(0.5)); }
		template<typename T> T exp2(T x) throw() { return exp(x * static_cast<T>(0.69314718055994530941)); }
		template<typename T> T cbrt(T x) throw() { return (x < 0) ? -pow(-x, static_cast<T>(1.0 / 3.0)) : pow(x, static_cast<T>(1.0 / 3.0)); }
	#endif

	inline int sign(int x) throw() { return (x > 0) - (x < 0); }
#if !defined(_MSC_VER)
	inline float sign(float x) throw() { return copysignf(1.0f, x); } // Tests have shown that copysignf on LLVM is about 10% faster (only) than ternary operator solution. Haven't tested the other compilers.
	inline double sign(double x) throw() { return copysign(1.0, x); }
#endif
	inline int exp2(int x) throw() { return -(x >= 0) & (1 << x); }

	// FIX : this one isn't really bonafide, it only works with int's ... what would be a good definition? floor div?
	template<class T> T unsignedDiv(T x, T y) throw() {
		assert(y >= 0);
		if (x >= 0) return x / y;
		else return (x - y + 1) / y;
	}

	// FIX : call this floor mod instead? (see http://en.wikipedia.org/wiki/Modulo_operation article on modulo)
	template<class T> T unsignedMod(T x, T y) throw() { assert(y >= 0); return mod(mod(x, y) + y, y); }

	template<typename T> T lerp(T from, T to, T x) throw() { return from + (to - from) * x; }

	template<typename T> T scale(T x, T inFrom, T inTo, T outFrom, T outTo) throw() {
		return outFrom + (outTo - outFrom) * (x - inFrom) / (inTo - inFrom);
	}

	template<typename T> T logScale(T x, T inFrom, T inTo, T outFrom, T outTo) throw() {
		return outFrom * powopt((outTo / outFrom), (x - inFrom) / (inTo - inFrom));
	}

	template<typename T> T inverseLogScale(T y, T inFrom, T inTo, T outFrom, T outTo) throw() {
		return outFrom + log(y / inFrom) / log(inTo / inFrom) * (outTo - outFrom);
	}

	template<typename T> T productLogScale(T x, T inFrom, T inTo, T outFrom, T outTo) throw() {
		float x0 = (x - inFrom) / (inTo - inFrom);
		return ((abs(outFrom) < abs(outTo)) ? x0 : (1.0f - x0)) * outFrom * powopt((outTo / outFrom), x0);
	}

	template<typename T> T bounce(T x, T mini, T maxi) throw() {
		return mini + abs(mod(abs(x - maxi), (2 * (maxi - mini))) - (maxi - mini));
	}

	template<typename F> int floatToIntEvenDistribution(F y, int steps) throw() {
		assert(0 <= y && y <= 1);
		return min(static_cast<int>(y * steps), steps - 1);
	}
	
	template<typename F> int floatToIntRounded(F y, int steps) throw() {
		assert(0 <= y && y <= 1);
		return static_cast<int>(y * (steps - 1) + static_cast<F>(0.5f));
	}
	
	template<typename F> F intToFloat(int i, int steps) throw() {
		assert(0 <= i && i < steps);
		return i / static_cast<F>(steps - 1);
	}

	class XorshiftRandom2x32 {
		public:		static XorshiftRandom2x32 randomSeeded();
		public:		XorshiftRandom2x32(unsigned int seed0 = 123456789, unsigned int seed1 = 362436069);
		public:		void randomSeed();
		public:		unsigned int nextUnsignedInt() throw();
		public:		unsigned int nextUnsignedInt(unsigned int maxx) throw(); // Range [0,maxx]
		public:		double nextDouble() throw();
		public:		double operator()() throw();
		public:		float nextFloat() throw();
		public:		void setState(unsigned int x, unsigned int y) throw();
		public:		void getState(unsigned int& x, unsigned int& y) throw();
		protected:	unsigned int px;
		protected:	unsigned int py;
	};
	inline XorshiftRandom2x32::XorshiftRandom2x32(unsigned int seed0, unsigned int seed1) : px(seed0), py(seed1) { }
	inline unsigned int XorshiftRandom2x32::nextUnsignedInt() throw() {
		unsigned int t = px ^ (px << 10);
		px = py;
		py = py ^ (py >> 13) ^ t ^ (t >> 10);
		return py;
	}
	
	// From MersenneTwister by by Makoto Matsumoto, Takuji Nishimura, and Shawn Cokus, Richard J. Wagner, Magnus Jonsson
	inline unsigned int XorshiftRandom2x32::nextUnsignedInt(unsigned int maxx) throw() {
		unsigned int used = maxx;
		used |= used >> 1;
		used |= used >> 2;
		used |= used >> 4;
		used |= used >> 8;
		used |= used >> 16;
		
		unsigned int i;
		do {
			i = nextUnsignedInt() & used;
		} while (i > maxx);
		return i;
	}

	inline double XorshiftRandom2x32::nextDouble() throw() {
		nextUnsignedInt();
		return py * 2.3283064365386962890625e-10 + px * 5.42101086242752217003726400434970855712890625e-20;
	}

	inline double XorshiftRandom2x32::operator()() throw() {
		return nextDouble();
	}

	inline float XorshiftRandom2x32::nextFloat() throw() {
		return static_cast<float>(nextUnsignedInt() * 2.3283064365386962890625e-10);
	}
	
	inline void XorshiftRandom2x32::setState(unsigned int x, unsigned int y) throw() { px = x; py = y; }
	inline void XorshiftRandom2x32::getState(unsigned int& x, unsigned int& y) throw() { x = px; y = py; }

	template<class PRNG_FUNCTOR> class NormalRandom {
		public:		NormalRandom(PRNG_FUNCTOR& prng) : prng(prng), haveNextGaussian(false), nextGaussian(0.0) { }
		
		public:		double normalRand(double mean, double deviation) {
						double out;
						if (haveNextGaussian) {
							haveNextGaussian = false;
							out = nextGaussian * deviation + mean;
						} else {
							double v1;
							double v2;
							double s;
							do {
								v1 = prng() * 2.0 - 1.0;
								v2 = prng() * 2.0 - 1.0;
								s = v1 * v1 + v2 * v2;
							} while (s >= 1.0 || s == 0.0);
							double multiplier = sqrt(-2.0 * log(s) / s);
							nextGaussian = v2 * multiplier;
							haveNextGaussian = true;
							out = v1 * multiplier * deviation + mean;
						}
						return out;
					}
		
		public:		double limitedNormalRand(double mean, double deviation, double mini, double maxi) {
						double v;
						do {
							v = normalRand(mean, deviation);
						} while (v < mini || v > maxi);
						return v;
					}

		public:		double clampedNormalRand(double mean, double deviation, double mini, double maxi) {
						return clamp(normalRand(mean, deviation), mini, maxi);
					}

		public:		double bouncedNormalRand(double mean, double deviation, double mini, double maxi) {
						return bounce(normalRand(mean, deviation), mini, maxi);
					}

		public:		double wrappedNormalRand(double mean, double deviation, double mini, double maxi) {
						double v = normalRand(mean, deviation);
						while (v < mini) {
							v += (maxi - mini);
						}
						while (v > maxi) {
							v -= (maxi - mini);
						}
						return v;
					}
		
		protected:	PRNG_FUNCTOR& prng;
		protected:	bool haveNextGaussian;
		protected:	double nextGaussian;
	};

	// based on https://martin-thoma.com/fractions-in-cpp/

	class Fraction;
	Fraction operator+(const Fraction& l, const Fraction& r);
	Fraction operator-(const Fraction& l, const Fraction& r);
	Fraction operator*(const Fraction& l, const Fraction& r);
	Fraction operator/(const Fraction& l, const Fraction& r);
	class Fraction {
		protected:	int gcd(int a, int b);

		public:		Fraction(int n = 0) : numerator(n), denominator(1) { }
		public:		Fraction(int n, int d) : numerator(0), denominator(1) {
						assert(d != 0);
						if (d == 1) {
							numerator = n;
						} else if (n != 0) {
							int sign = 1;
							if (n < 0) {
								sign *= -1;
								n *= -1;
							}
							if (d < 0) {
								sign *= -1;
								d *= -1;
							}
							const int tmp = gcd(n, d);
							assert(tmp > 0);
							numerator = n / tmp * sign;
							denominator = d / tmp;
						}
					}

					template<typename T> T to() const { return static_cast<T>(numerator) / denominator; }
					int floor() const { return (numerator < 0 ? numerator - (denominator - 1) : numerator) / denominator; }
					int ceil() const { return (numerator > 0 ? numerator + (denominator - 1) : numerator) / denominator; }
					// note: round() does away from zero rounding like C++ round(), not like JS round() and others that round towards infinity.
					int round() const { return (numerator > 0 ? numerator + (denominator / 2) : numerator - (denominator / 2)) / denominator; }
					Fraction& operator+=(const Fraction& r) { (*this) = (*this) + r; return (*this); }
					Fraction& operator-=(const Fraction& r) { (*this) = (*this) - r; return (*this); }
					Fraction& operator*=(const Fraction& r) { (*this) = (*this) * r; return (*this); }
					Fraction& operator/=(const Fraction& r) { (*this) = (*this) / r; return (*this); }

					Fraction operator+() const { return (*this); }
					Fraction operator-() const { return Fraction(-numerator, denominator); }

					bool operator==(const Fraction& r) const { return numerator == r.numerator && denominator == r.denominator; }
					bool operator!=(const Fraction& r) const { return !(*this == r); }
					bool operator<(const Fraction& r) const { return numerator * r.denominator < r.numerator * denominator; }
					bool operator>(const Fraction& r) const { return r < (*this); }
					bool operator<=(const Fraction& r) const { return numerator * r.denominator <= r.numerator * denominator; }
					bool operator>=(const Fraction& r) const { return r <= (*this); }

					int numerator;
					int denominator;
	};

	inline Fraction operator+(const Fraction& l, const Fraction& r) { return Fraction(l.numerator * r.denominator + r.numerator * l.denominator, l.denominator * r.denominator); }
	inline Fraction operator-(const Fraction& l, const Fraction& r) { return Fraction(l.numerator * r.denominator - r.numerator * l.denominator, l.denominator * r.denominator); }
	inline Fraction operator*(const Fraction& l, const Fraction& r) { return Fraction(l.numerator * r.numerator, l.denominator * r.denominator); }
	inline Fraction operator/(const Fraction& l, const Fraction& r) { return Fraction(l.numerator * r.denominator, l.denominator * r.numerator); }
	std::ostream& operator<<(std::ostream &strm, const Fraction &a);
	
	template<class F, typename T, typename U> T bisect(const F& fn, T y, U low = T(0.0), U high = T(1.0), int maxSteps = sizeof (U) * 8) {
		if (fn(high) < fn(low)) {
			std::swap(low, high);
		}
		U x = low + (high - low) / 2;
		for (int i = 0; i < maxSteps && x != low && x != high; ++i) {
			if (fn(x) < y) {
				low = x;
			} else {
				high = x;
			}
			x = low + (high - low) / 2;
		}
		return x;
	}

	bool unitTest();

	template<class RandomIt> void shuffle(RandomIt first, RandomIt last, XorshiftRandom2x32& prng) {
		typedef typename std::iterator_traits<RandomIt>::difference_type IndexType;
		const IndexType n = std::distance(first, last);
		for (IndexType i = n - 1; i >= 0; --i) {
			const IndexType j = static_cast<IndexType>(prng.nextUnsignedInt(static_cast<unsigned int>(i)));
			std::swap(*(first + i), *(first + j));
		}
	}

	template<typename RandomIt> void semiShuffle(RandomIt first, RandomIt last, int degree, XorshiftRandom2x32& prng) {
		assert(degree >= 0);
		typedef typename std::iterator_traits<RandomIt>::difference_type IndexType;
		const IndexType n = std::distance(first, last);
		for (IndexType i = 0; i < n; ++i) {
			const IndexType minJ = std::max(static_cast<IndexType>(0), i - degree);
			const IndexType maxJ = std::min(n - 1, i + degree);
			const IndexType j = minJ + static_cast<IndexType>(prng.nextUnsignedInt(static_cast<unsigned int>(maxJ - minJ)));
			std::swap(*(first + i), *(first + j));
		}
	}

}	// namespace NuXMath

#endif
