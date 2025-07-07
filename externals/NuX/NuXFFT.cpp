#include "assert.h"
#include <algorithm>
#include <cmath>
#include "NuXFFT.h"

namespace NuXFFT {

const double PI = 3.1415926535897932384626433832795;

template<typename T> T square(T x) { return x * x; }

template<typename T> void reverseBinaryIndexing(int n, T data[]) {
	int j = 0;
	for (int i = 0; i < n; i += 2) {
		if (j > i) {
			std::swap(data[j], data[i]);
			std::swap(data[j + 1], data[i + 1]);
		}
		int m = n >> 1;
		while (m >= 2 && j + 1 > m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	}
}

template<typename T> void complexFFT(int n, T data[]) {
	assert(n >= 2 && (n & (n - 1)) == 0);

	reverseBinaryIndexing<T>(n, data);

	for (int mmax = 2; mmax < n; mmax *= 4) {
		if (mmax * 2 < n) {	// radix-4
			const T theta = T(-2.0 * PI) / (mmax * 2);
			// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
			const T wpRe = T(2.0) * square(sin(theta * T(0.5)));
			const T wpIm = sin(theta);
			T wRe = T(1.0);
			T wIm = T(0.0);

			for (int m = 0; m < mmax; m += 2) {
				const T w2Re = wRe * wRe - wIm * wIm;
				const T w2Im = wIm * wRe + wRe * wIm;
				const T w3Re = w2Re * wRe - w2Im * wIm;
				const T w3Im = w2Im * wRe + w2Re * wIm;

				for (int i = m; i < n; i += mmax * 4) {
					const int i1 = i + mmax;
					assert(0 <= i1 && i1 + 1 < n);
					const T d1w2Re = (data[i1 + 0] * w2Re - data[i1 + 1] * w2Im);
					const T d1w2Im = (data[i1 + 1] * w2Re + data[i1 + 0] * w2Im);

					const int i2 = i1 + mmax;
					assert(0 <= i2 && i2 + 1 < n);
					const T d2wRe = (data[i2 + 0] * wRe - data[i2 + 1] * wIm);
					const T d2wIm = (data[i2 + 1] * wRe + data[i2 + 0] * wIm);

					const int i3 = i2 + mmax;
					assert(0 <= i3 && i3 + 1 < n);
					const T d3w3Re = (data[i3 + 0] * w3Re - data[i3 + 1] * w3Im);
					const T d3w3Im = (data[i3 + 1] * w3Re + data[i3 + 0] * w3Im);
		 
					assert(0 <= i && i + 1 < n);
					const T d0Re = data[i + 0];
					const T d0Im = data[i + 1];
					
					data[i3 + 0] = (d0Re - d2wIm) - (d1w2Re - d3w3Im);
					data[i3 + 1] = (d0Im + d2wRe) - (d1w2Im + d3w3Re);
					data[i2 + 0] = (d0Re - d2wRe) + (d1w2Re - d3w3Re);
					data[i2 + 1] = (d0Im - d2wIm) + (d1w2Im - d3w3Im);
					data[i1 + 0] = (d0Re + d2wIm) - (d1w2Re + d3w3Im);
					data[i1 + 1] = (d0Im - d2wRe) - (d1w2Im - d3w3Re);
					data[i + 0] = (d0Re + d2wRe) + (d1w2Re + d3w3Re);
					data[i + 1] = (d0Im + d2wIm) + (d1w2Im + d3w3Im);
				}
				
				const T nwRe = wRe - wRe * wpRe - wIm * wpIm;
				const T nwIm = wIm - wIm * wpRe + wRe * wpIm;
				wRe = nwRe;
				wIm = nwIm;
			}
		} else {	// radix-2
			const T theta = T(-2.0 * PI) / mmax;
			// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
			const T wpRe = T(2.0) * square(sin(theta * T(0.5)));
			const T wpIm = sin(theta);
			T wRe = T(1.0);
			T wIm = T(0.0);
			
			for (int m = 0; m < mmax; m += 2) {
				for (int i = m; i < n; i += mmax * 2) {
					const int j = i + mmax;
					const T tRe = wRe * data[j + 0] - wIm * data[j + 1];
					const T tIm = wRe * data[j + 1] + wIm * data[j + 0];
					const T dRe = data[i + 0];
					const T dIm = data[i + 1];
					data[j + 0] = dRe - tRe;
					data[j + 1] = dIm - tIm;
					data[i + 0] = dRe + tRe;
					data[i + 1] = dIm + tIm;
				}
				const T nwRe = wRe - wRe * wpRe - wIm * wpIm;
				const T nwIm = wIm - wIm * wpRe + wRe * wpIm;
				wRe = nwRe;
				wIm = nwIm;
			}
		}
	}
}

template<typename T> void untangle(int n, T data[]) {
	const T re0 = data[0];
	const T im0 = data[1];
	data[0] = (re0 + im0);
	data[1] = (re0 - im0);

	const T theta = T(2.0 * PI) / n;
	// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
	const T wpRe = T(2.0) * square(sin(theta * T(0.5)));
	const T wpIm = sin(theta);
	T wRe = T(1.0);
	T wIm = T(0.0);
	for (int i = 2; i <= n / 2; i += 2) {
		const T nwRe = wRe - wRe * wpRe - wIm * wpIm;
		const T nwIm = wIm - wIm * wpRe + wRe * wpIm;
		wRe = nwRe;
		wIm = nwIm;
		
		const T re0 = data[i + 0];
		const T im0 = data[i + 1];
		const T re1 = data[n - i + 0];
		const T im1 = data[n - i + 1];

		const T r0r1 = re0 + re1;
		const T i0i1 = im0 - im1;
		
		const T i0i1wRe = (im0 + im1) * wRe;
		const T i0i1wIm = (im0 + im1) * wIm;
		const T r1r0wRe = (re1 - re0) * wRe;
		const T r0r1wIm = (re0 - re1) * wIm;

		data[i + 0] = T(0.5) * (i0i1wRe - r0r1wIm + r0r1);
		data[i + 1] = T(0.5) * (r1r0wRe - i0i1wIm + i0i1);
		data[n - i + 0] = T(0.5) * (r0r1wIm - i0i1wRe + r0r1);
		data[n - i + 1] = T(0.5) * (r1r0wRe - i0i1wIm - i0i1);
	}
}

template<typename T> void inverse(int n, T data[]) {
	assert(n >= 2 && (n & (n - 1)) == 0);
	if (n > 2) {
		const T g = static_cast<T>(1) / (n / 2);
		for (int i = 1; i < n / 4; ++i) {
			std::swap(data[i * 2], data[n - i * 2]);
			std::swap(data[i * 2 + 1], data[n - i * 2 + 1]);
			data[i * 2] *= g;
			data[n - i * 2] *= g;
			data[i * 2 + 1] *= g;
			data[n - i * 2 + 1] *= g;
		}
		data[n / 2] *= g;
		data[n / 2 + 1] *= g;
		data[0] *= g;
		data[1] *= g;
	}
	data[0] *= T(0.5);
	data[1] *= T(0.5);
}

template<typename T> void realFFT(int n, T data[]) {
	assert(n >= 2 && (n & (n - 1)) == 0);

	complexFFT(n, data);
	untangle(n, data);
}

template<typename T> void inverseRealFFT(int n, T data[]) {
	assert(n >= 2 && (n & (n - 1)) == 0);

	inverse(n, data);
	untangle(n, data);
	complexFFT(n, data);
}

template void complexFFT<float>(int n, float data[]);
template void complexFFT<double>(int n, double data[]);
template void realFFT<float>(int n, float data[]);
template void realFFT<double>(int n, double data[]);
template void inverseRealFFT<float>(int n, float data[]);
template void inverseRealFFT<double>(int n, double data[]);

#if (NUX_FFT_SIMD_SUPPORT)

	#if (NUX_FFT_SIMD_SSE)
		QFloat* allocateSIMD(size_t size) {
			QFloat* alloced = reinterpret_cast<QFloat*>(_mm_malloc(size * sizeof (QFloat), 16));
			if (alloced == 0) {
				throw std::bad_alloc();
			}
			return alloced;
		}

		void freeSIMD(QFloat* p) {
			assert(p != 0);
			_mm_free(p);
		}
	#elif (NUX_FFT_SIMD_NEON)
		QFloat* allocateSIMD(size_t size) {
			QFloat* alloced = reinterpret_cast<QFloat*>(aligned_alloc(16, size * sizeof (QFloat)));
			if (alloced == 0) {
				throw std::bad_alloc();
			}
			return alloced;
		}

		void freeSIMD(QFloat* p) {
			assert(p != 0);
			free(p);
		}
	#else
		#error Something is messed up.
	#endif

	template<> QFloat square(QFloat x) { return mulSIMD(x, x); }
	template<> void complexFFT(int n, QFloat data[]) {
		assert(n >= 2 && (n & (n - 1)) == 0);

		reverseBinaryIndexing<QFloat>(n, data);

		for (int mmax = 2; mmax < n; mmax *= 4) {
			if (mmax * 2 < n) {	// radix-4
				const float theta = float(-2.0 * PI) / (mmax * 2);
				// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
				const QFloat wpRe = loadSIMD(2.0f * square(sin(theta * 0.5f)));
				const QFloat wpIm = loadSIMD(sin(theta));
				QFloat wRe = loadSIMD(1.0f);
				QFloat wIm = loadSIMD(0.0f);

				for (int m = 0; m < mmax; m += 2) {
					const QFloat w2Re = subSIMD(mulSIMD(wRe, wRe), mulSIMD(wIm, wIm));
					const QFloat w2Im = addSIMD(mulSIMD(wIm, wRe), mulSIMD(wRe, wIm));
					const QFloat w3Re = subSIMD(mulSIMD(w2Re, wRe), mulSIMD(w2Im, wIm));
					const QFloat w3Im = addSIMD(mulSIMD(w2Im, wRe), mulSIMD(w2Re, wIm));
					
					for (int i = m; i < n; i += mmax * 4) {
						const int i1 = i + mmax;
						assert(0 <= i1 && i1 + 1 < n);
						const QFloat d1w2Re = subSIMD(mulSIMD(data[i1 + 0], w2Re), mulSIMD(data[i1 + 1], w2Im));
						const QFloat d1w2Im = addSIMD(mulSIMD(data[i1 + 1], w2Re), mulSIMD(data[i1 + 0], w2Im));

						const int i2 = i1 + mmax;
						assert(0 <= i2 && i2 + 1 < n);
						const QFloat d2wRe = subSIMD(mulSIMD(data[i2 + 0], wRe), mulSIMD(data[i2 + 1], wIm));
						const QFloat d2wIm = addSIMD(mulSIMD(data[i2 + 1], wRe), mulSIMD(data[i2 + 0], wIm));

						const int i3 = i2 + mmax;
						assert(0 <= i3 && i3 + 1 < n);
						const QFloat d3w3Re = subSIMD(mulSIMD(data[i3 + 0], w3Re), mulSIMD(data[i3 + 1], w3Im));
						const QFloat d3w3Im = addSIMD(mulSIMD(data[i3 + 1], w3Re), mulSIMD(data[i3 + 0], w3Im));
			 
						assert(0 <= i && i + 1 < n);
						const QFloat d0Re = data[i + 0];
						const QFloat d0Im = data[i + 1];
						
						data[i3 + 0] = subSIMD(subSIMD(d0Re, d2wIm), subSIMD(d1w2Re, d3w3Im));
						data[i3 + 1] = subSIMD(addSIMD(d0Im, d2wRe), addSIMD(d1w2Im, d3w3Re));
						data[i2 + 0] = addSIMD(subSIMD(d0Re, d2wRe), subSIMD(d1w2Re, d3w3Re));
						data[i2 + 1] = addSIMD(subSIMD(d0Im, d2wIm), subSIMD(d1w2Im, d3w3Im));
						data[i1 + 0] = subSIMD(addSIMD(d0Re, d2wIm), addSIMD(d1w2Re, d3w3Im));
						data[i1 + 1] = subSIMD(subSIMD(d0Im, d2wRe), subSIMD(d1w2Im, d3w3Re));
						data[i + 0] = addSIMD(addSIMD(d0Re, d2wRe), addSIMD(d1w2Re, d3w3Re));
						data[i + 1] = addSIMD(addSIMD(d0Im, d2wIm), addSIMD(d1w2Im, d3w3Im));
					}

					const QFloat nwRe = subSIMD(subSIMD(wRe, mulSIMD(wRe, wpRe)), mulSIMD(wIm, wpIm));
					const QFloat nwIm = addSIMD(subSIMD(wIm, mulSIMD(wIm, wpRe)), mulSIMD(wRe, wpIm));
					wRe = nwRe;
					wIm = nwIm;
				}
			} else {	// radix-2
				const float theta = float(-2.0 * PI) / mmax;
				// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
				const QFloat wpRe = loadSIMD(2.0f * square(sin(theta * 0.5f)));
				const QFloat wpIm = loadSIMD(sin(theta));
				QFloat wRe = loadSIMD(1.0f);
				QFloat wIm = loadSIMD(0.0f);
				
				for (int m = 0; m < mmax; m += 2) {
					for (int i = m; i < n; i += mmax * 2) {
						const int j = i + mmax;
						const QFloat tRe = subSIMD(mulSIMD(wRe, data[j + 0]), mulSIMD(wIm, data[j + 1]));
						const QFloat tIm = addSIMD(mulSIMD(wRe, data[j + 1]), mulSIMD(wIm, data[j + 0]));
						const QFloat dRe = data[i + 0];
						const QFloat dIm = data[i + 1];
						data[j + 0] = subSIMD(dRe, tRe);
						data[j + 1] = subSIMD(dIm, tIm);
						data[i + 0] = addSIMD(dRe, tRe);
						data[i + 1] = addSIMD(dIm, tIm);
					}
					const QFloat nwRe = subSIMD(subSIMD(wRe, mulSIMD(wRe, wpRe)), mulSIMD(wIm, wpIm));
					const QFloat nwIm = addSIMD(subSIMD(wIm, mulSIMD(wIm, wpRe)), mulSIMD(wRe, wpIm));
					wRe = nwRe;
					wIm = nwIm;
				}
			}
		}
	}

	template<> void untangle(int n, QFloat data[]) {
		const QFloat re0 = data[0];
		const QFloat im0 = data[1];
		data[0] = addSIMD(re0, im0);
		data[1] = subSIMD(re0, im0);

		const float theta = float(2.0 * PI) / n;
		// 2 * sin^2(t/2) = 1 - cos(t), improves precision with low rates compared to using cos
		const QFloat wpRe = loadSIMD(2.0f * square(sin(theta * 0.5f)));
		const QFloat wpIm = loadSIMD(sin(theta));
		QFloat wRe = loadSIMD(1.0f);
		QFloat wIm = loadSIMD(0.0f);
		const QFloat HALF = loadSIMD(0.5f);
		for (int i = 2; i <= n / 2; i += 2) {
			const QFloat nwRe = subSIMD(subSIMD(wRe, mulSIMD(wRe, wpRe)), mulSIMD(wIm, wpIm));
			const QFloat nwIm = addSIMD(subSIMD(wIm, mulSIMD(wIm, wpRe)), mulSIMD(wRe, wpIm));
			wRe = nwRe;
			wIm = nwIm;
			
			const QFloat re0 = data[i + 0];
			const QFloat im0 = data[i + 1];
			const QFloat re1 = data[n - i + 0];
			const QFloat im1 = data[n - i + 1];

			const QFloat r0r1 = addSIMD(re0, re1);
			const QFloat i0i1 = subSIMD(im0, im1);
			
			const QFloat i0i1wRe = mulSIMD(addSIMD(im0, im1), wRe);
			const QFloat i0i1wIm = mulSIMD(addSIMD(im0, im1), wIm);
			const QFloat r1r0wRe = mulSIMD(subSIMD(re1, re0), wRe);
			const QFloat r0r1wIm = mulSIMD(subSIMD(re0, re1), wIm);

			data[i + 0] = mulSIMD(HALF, addSIMD(subSIMD(i0i1wRe, r0r1wIm), r0r1));
			data[i + 1] = mulSIMD(HALF, addSIMD(subSIMD(r1r0wRe, i0i1wIm), i0i1));
			data[n - i + 0] = mulSIMD(HALF, addSIMD(subSIMD(r0r1wIm, i0i1wRe), r0r1));
			data[n - i + 1] = mulSIMD(HALF, subSIMD(subSIMD(r1r0wRe, i0i1wIm), i0i1));
		}
	}

	template<> void inverse(int n, QFloat data[]) {
		assert(n >= 2 && (n & (n - 1)) == 0);
		if (n > 2) {
			const QFloat g = loadSIMD(1.0f / (n / 2));
			for (int i = 1; i < n / 4; ++i) {
				std::swap(data[i * 2], data[n - i * 2]);
				std::swap(data[i * 2 + 1], data[n - i * 2 + 1]);
				data[i * 2] = mulSIMD(data[i * 2], g);
				data[n - i * 2] = mulSIMD(data[n - i * 2], g);
				data[i * 2 + 1] = mulSIMD(data[i * 2 + 1], g);
				data[n - i * 2 + 1] = mulSIMD(data[n - i * 2 + 1], g);
			}
			data[n / 2] = mulSIMD(data[n / 2], g);
			data[n / 2 + 1] = mulSIMD(data[n / 2 + 1], g);
			data[0] = mulSIMD(data[0], g);
			data[1] = mulSIMD(data[1], g);
		}
		const QFloat half = loadSIMD(0.5f);
		data[0] = mulSIMD(data[0], half);
		data[1] = mulSIMD(data[1], half);
	}

	template void realFFT<QFloat>(int n, QFloat data[]);
	template void inverseRealFFT<QFloat>(int n, QFloat data[]);

#endif

} /* namespace NuXFFT */
