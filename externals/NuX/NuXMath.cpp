#include <iostream>
#include "NuXMath.h"

#ifdef _WIN32
   #define IS_WINDOWS 1
#endif

#ifdef __APPLE__
	#define IS_MAC 1
#endif

#if (IS_WINDOWS)
	#include <Windows.h>
	#include <time.h>
#endif

#if (IS_MAC)
	#include <mach/mach_time.h>
	#include <libkern/OSAtomic.h>
#endif

namespace NuXMath {

/* --- XorshiftRandom2x32 --- */

void XorshiftRandom2x32::randomSeed() {
#if (IS_MAC)
	{
		const uint64_t t = ::mach_absolute_time();
		px = static_cast<uint32_t>(time(0)) ^ static_cast<uint32_t>(t & 0xFFFFFFFFU);
		py = static_cast<uint32_t>(clock()) ^ static_cast<uint32_t>((t >> 32) & 0xFFFFFFFFU);
	}
	static int32_t counter;
	::OSAtomicIncrement32Barrier(&counter);
	const unsigned int z = counter;
#elif (IS_WINDOWS)
	{
		::LARGE_INTEGER count;
		::BOOL success = ::QueryPerformanceCounter(&count);
		if (!success) {
			count.LowPart = 0;
			count.HighPart = 0;
		}
		px = static_cast<uint32_t>(time(0)) ^ count.LowPart;
		py = static_cast<uint32_t>(::GetTickCount()) ^ count.HighPart;
	}
	static LONG counter;
	const unsigned int z = ::InterlockedIncrement((LPLONG)(&counter));
#else
	#error Unsupported platform.
#endif
	px += (z << 16) | (z >> 16);
	px |= 1; // In the extremely unlikely case both x and y are 0.
	for (int i = 0; i < 32; ++i) {
		nextUnsignedInt();
	}
}

XorshiftRandom2x32 XorshiftRandom2x32::randomSeeded() {
	XorshiftRandom2x32 prng;
	prng.randomSeed();
	return prng;
}

int Fraction::gcd(int a, int b) {
	while (a != b) {
		if (a > b) {
			a -= b;
		} else {
			b -= a;
		}
	}
	return a;
}

std::ostream& operator<<(std::ostream &strm, const Fraction &a) {
	if (a.denominator == 1) {
		strm << a.numerator;
	} else {
		strm << a.numerator << "/" << a.denominator;
	}
	return strm;
}

bool unitTest() {
	Fraction a(1, 3);
	Fraction b(3, 28);
	Fraction c;

	c = a + b;
	assert(c.numerator == 37);
	assert(c.denominator == 84);
	
	c = a - b;
	assert(c.numerator == 19);
	assert(c.denominator == 84);

	c = a * b;
	assert(c.numerator == 1);
	assert(c.denominator == 28);

	c = a / b;
	assert(c.numerator == 28);
	assert(c.denominator == 9);

	c = 1 / c;
	assert(c.numerator == 9);
	assert(c.denominator == 28);

	c = -1 * b;
	assert(c.numerator == -3);
	assert(c.denominator == 28);

	c = b * -1;
	assert(c.numerator == -3);
	assert(c.denominator == 28);

	c = Fraction(-100, 3);
	assert(c.to<int>() == -33);
	assert(c.to<float>() == -(100.0f / 3.0f));
	assert(c.to<double>() == -(100.0 / 3.0));

	a -= b;
	assert(a.numerator == 19);
	assert(a.denominator == 84);
	
	a *= 2;
	assert(a.numerator == 19);
	assert(a.denominator == 42);

	a *= 84;
	assert(a.numerator == 38);
	assert(a.denominator == 1);

	a += Fraction(1, 3);
	assert(a.numerator == 115);
	assert(a.denominator == 3);

	a /= 5;
	assert(a.numerator == 23);
	assert(a.denominator == 3);

	a = -a;
	assert(a.numerator == -23);
	assert(a.denominator == 3);

	assert(a == Fraction(-23, 3));
	assert(a == Fraction(-46, 6));
	assert(a != Fraction(23, 3));
	
	a = -a;
	assert(a == Fraction(23, 3));

	assert(a < Fraction(24, 3));
	assert(a <= Fraction(24, 3));
	assert(!(a > Fraction(24, 3)));
	assert(!(a >= Fraction(24, 3)));
	assert(a < Fraction(47, 6));
	assert(a <= Fraction(47, 6));
	assert(!(a > Fraction(47, 6)));
	assert(!(a >= Fraction(47, 6)));
	assert(a > Fraction(22, 6));
	assert(a >= Fraction(22, 6));
	assert(!(a < Fraction(22, 6)));
	assert(!(a <= Fraction(22, 6)));
	assert(a > Fraction(45, 6));
	assert(a >= Fraction(45, 6));
	assert(!(a < Fraction(45, 6)));
	assert(!(a <= Fraction(45, 6)));
	assert(!(a < Fraction(23, 3)));
	assert(a <= Fraction(23, 3));
	assert(!(a > Fraction(23, 3)));
	assert(a >= Fraction(23, 3));
	
	a = Fraction(23, 3);
	assert(a.round() == 8);
	assert(a.floor() == 7);
	assert(a.ceil() == 8);

	a = Fraction(-23, 3);
	assert(a.round() == -8);
	assert(a.floor() == -8);
	assert(a.ceil() == -7);

	a = Fraction(24, 3);
	assert(a.round() == 8);
	assert(a.floor() == 8);
	assert(a.ceil() == 8);

	a = Fraction(-24, 3);
	assert(a.round() == -8);
	assert(a.floor() == -8);
	assert(a.ceil() == -8);

	a = Fraction(25, 3);
	assert(a.round() == 8);
	assert(a.floor() == 8);
	assert(a.ceil() == 9);

	a = Fraction(-25, 3);
	assert(a.round() == -8);
	assert(a.floor() == -9);
	assert(a.ceil() == -8);

	a = Fraction(25, 2);
	assert(a.round() == 13);
	assert(a.floor() == 12);
	assert(a.ceil() == 13);

	a = Fraction(-25, 2);
	assert(a.round() == -13);
	assert(a.floor() == -13);
	assert(a.ceil() == -12);

	return true;
}

} // namespace NuXMath

#ifdef REGISTER_UNIT_TEST
REGISTER_UNIT_TEST(NuXMath::unitTest)
#endif
