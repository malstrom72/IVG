/*
	IVG-2 back-port only. IVGSnapshot belongs to IVG-3, whose IMPD has the UTF-8 conversions below; IVG-2's IMPD does
	not, and the back-port must not change the library, so they are copied here unchanged from IVG-3's src/IMPD.cpp.
	Declared static in namespace IMPD so IVGSnapshot's calls compile as they are.
*/

#ifndef IVG2Compat_h
#define IVG2Compat_h

#include "src/IMPD.h"

namespace IMPD {

/*
	UTF-8 validation and decoding, ported unchanged from NEdLib's IsValidUTF8(), CalcUTF8ToUTF32Size() and
	ConvertUTF8ToUTF32(). isValidUTF8() is the perimeter check; the two converters assume it has passed and
	assert the same conditions.
*/
static bool isValidUTF8(size_t utf8Size, const char* utf8Chars) {
	size_t i = 0;
	while (i < utf8Size) {
		unsigned char c = static_cast<unsigned char>(utf8Chars[i]);
		if ((c & 0x80) == 0) {
			// 1-byte
			++i;
		} else if ((c & 0xE0) == 0xC0) {
			// 2-byte: 110xxxxx 10xxxxxx, no overlongs (>= 0xC2)
			if (i + 1 >= utf8Size || (utf8Chars[i + 1] & 0xC0) != 0x80 || c < 0xC2) {
				return false;
			}
			i += 2;
		} else if ((c & 0xF0) == 0xE0) {
			// 3-byte: 1110xxxx 10xxxxxx 10xxxxxx; exclude surrogates, overlongs
			if (i + 2 >= utf8Size) {
				return false;
			}
			const unsigned char b1 = static_cast<unsigned char>(utf8Chars[i + 1]);
			const unsigned char b2 = static_cast<unsigned char>(utf8Chars[i + 2]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (c == 0xE0 && b1 < 0xA0)
					|| (c == 0xED && b1 > 0x9F)) {
				return false;
			}
			i += 3;
		} else if ((c & 0xF8) == 0xF0) {
			// 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx; max U+10FFFF
			if (i + 3 >= utf8Size) {
				return false;
			}
			const unsigned char b1 = static_cast<unsigned char>(utf8Chars[i + 1]);
			const unsigned char b2 = static_cast<unsigned char>(utf8Chars[i + 2]);
			const unsigned char b3 = static_cast<unsigned char>(utf8Chars[i + 3]);
			if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80
					|| (c < 0xF0 || c > 0xF4) || (c == 0xF0 && b1 < 0x90)
					|| (c == 0xF4 && b1 > 0x8F)) {
				return false;
			}
			i += 4;
		} else {
			return false;
		}
	}
	return true;
}

static size_t calcUTF8ToUTF32Size(size_t utf8Size, const char* utf8Chars) {
	size_t n = 0;
	for (size_t i = 0; i < utf8Size; ++i) {
		UniChar c = static_cast<unsigned char>(utf8Chars[i]);
		if ((c & 0x80) == 0) { // 1-byte character
			n += 1;
		} else if ((c & 0xE0) == 0xC0) { // 2-byte character
			assert(i + 1 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80);
			assert(c >= 0xC2);
			n += 1;
			i += 1;
		} else if ((c & 0xF0) == 0xE0) { // 3-byte character
			assert(i + 2 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80 && ((utf8Chars[i + 2]) & 0xC0) == 0x80);
			assert(c != 0xE0 || static_cast<unsigned char>(utf8Chars[i + 1]) >= 0xA0);
			assert(c != 0xED || static_cast<unsigned char>(utf8Chars[i + 1]) <= 0x9F);
			n += 1;
			i += 2;
		} else if ((c & 0xF8) == 0xF0) { // 4-byte character
			assert(i + 3 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80 && ((utf8Chars[i + 2]) & 0xC0) == 0x80
					&& ((utf8Chars[i + 3]) & 0xC0) == 0x80);
			assert(c >= 0xF0 && c <= 0xF4);
			assert(c != 0xF0 || static_cast<unsigned char>(utf8Chars[i + 1]) >= 0x90);
			assert(c != 0xF4 || static_cast<unsigned char>(utf8Chars[i + 1]) <= 0x8F);
			n += 1;
			i += 3;
		} else {
			assert(false); // Invalid UTF-8 start byte
		}
	}
	return n;
}

static size_t convertUTF8ToUTF32(size_t utf8Size, const char* utf8Chars, UniChar* utf32Chars) {
	size_t outIndex = 0;
	for (size_t i = 0; i < utf8Size;) {
		UniChar c = static_cast<unsigned char>(utf8Chars[i]);
		if ((c & 0x80) == 0) { // 1-byte character
			i += 1;
		} else if ((c & 0xE0) == 0xC0) { // 2-byte character
			assert(i + 1 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80);
			assert(c >= 0xC2);
			c = ((c & 0x1F) << 6) | (static_cast<unsigned char>(utf8Chars[i + 1]) & 0x3F);
			i += 2;
		} else if ((c & 0xF0) == 0xE0) { // 3-byte character
			assert(i + 2 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80 && ((utf8Chars[i + 2]) & 0xC0) == 0x80);
			assert(c != 0xE0 || static_cast<unsigned char>(utf8Chars[i + 1]) >= 0xA0);
			assert(c != 0xED || static_cast<unsigned char>(utf8Chars[i + 1]) <= 0x9F);
			c = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(utf8Chars[i + 1]) & 0x3F) << 6)
					| (static_cast<unsigned char>(utf8Chars[i + 2]) & 0x3F);
			i += 3;
		} else if ((c & 0xF8) == 0xF0) { // 4-byte character
			assert(i + 3 < utf8Size);
			assert(((utf8Chars[i + 1]) & 0xC0) == 0x80 && ((utf8Chars[i + 2]) & 0xC0) == 0x80
					&& ((utf8Chars[i + 3]) & 0xC0) == 0x80);
			assert(c >= 0xF0 && c <= 0xF4);
			assert(c != 0xF0 || static_cast<unsigned char>(utf8Chars[i + 1]) >= 0x90);
			assert(c != 0xF4 || static_cast<unsigned char>(utf8Chars[i + 1]) <= 0x8F);
			c = ((c & 0x07) << 18) | ((static_cast<unsigned char>(utf8Chars[i + 1]) & 0x3F) << 12)
					| ((static_cast<unsigned char>(utf8Chars[i + 2]) & 0x3F) << 6)
					| (static_cast<unsigned char>(utf8Chars[i + 3]) & 0x3F);
			i += 4;
		} else {
			assert(false); // Invalid UTF-8 start byte
		}

		utf32Chars[outIndex++] = c;
	}
	return outIndex;
}

/*
	Converts UTF-8 to UTF-32, throwing a run-time error if `r` is not well-formed. UTF-8 reaching IMPD comes from a
	file system, a command line or a document, so this validates rather than assuming.
*/
static UniString convertUTF8ToUniString(const StringRange& r) {
	const size_t size = r.e - r.b;
	if (size == 0) {
		return UniString();
	}
	const char* chars = &*r.b;
	if (!isValidUTF8(size, chars)) {
		Interpreter::throwRunTimeError("Invalid UTF-8");
	}
	UniString result(calcUTF8ToUTF32Size(size, chars), 0);
	convertUTF8ToUTF32(size, chars, &result[0]);
	return result;
}

/*
	Converts UTF-8 to the platform wide string, throwing as convertUTF8ToUniString() does.
*/
static WideString convertUTF8ToWideString(const StringRange& r) {
	return convertUniToWideString(convertUTF8ToUniString(r));
}

static size_t calcUTF32ToUTF8Size(size_t utf32Size, const UniChar* utf32Chars) {
	size_t n = 0;
	for (size_t i = 0; i < utf32Size; ++i) {
		const UniChar c = utf32Chars[i];
		assert(c < 0xD800 || c >= 0xE000); // Surrogate code points inside UTF32 string are not legal!
		assert(c <= 0x10FFFF);				// Enforce Unicode scalar upper bound
		if (c < 0x80) {
			n += 1;
		} else if (c < 0x800) {
			n += 2;
		} else if (c < 0x10000) {
			n += 3;
		} else {
			n += 4;
		}
	}
	return n;
}

static size_t convertUTF32ToUTF8(size_t utf32Size, const UniChar* utf32Chars, char* utf8Chars) {
	size_t outIndex = 0;
	for (size_t i = 0; i < utf32Size; ++i) {
		const UniChar c = utf32Chars[i];
		assert(c < 0xD800 || c >= 0xE000); // Surrogate code points not legal in UTF-32!
		assert(c <= 0x10FFFF);				// Enforce Unicode scalar upper bound
		if (c < 0x80) {
			utf8Chars[outIndex + 0] = static_cast<char>(c);
			outIndex += 1;
		} else if (c < 0x800) {
			utf8Chars[outIndex + 0] = static_cast<char>((c >> 6) | 0xC0);
			utf8Chars[outIndex + 1] = static_cast<char>((c & 0x3F) | 0x80);
			outIndex += 2;
		} else if (c < 0x10000) {
			utf8Chars[outIndex + 0] = static_cast<char>((c >> 12) | 0xE0);
			utf8Chars[outIndex + 1] = static_cast<char>(((c >> 6) & 0x3F) | 0x80);
			utf8Chars[outIndex + 2] = static_cast<char>((c & 0x3F) | 0x80);
			outIndex += 3;
		} else {
			utf8Chars[outIndex + 0] = static_cast<char>((c >> 18) | 0xF0);
			utf8Chars[outIndex + 1] = static_cast<char>(((c >> 12) & 0x3F) | 0x80);
			utf8Chars[outIndex + 2] = static_cast<char>(((c >> 6) & 0x3F) | 0x80);
			utf8Chars[outIndex + 3] = static_cast<char>((c & 0x3F) | 0x80);
			outIndex += 4;
		}
	}
	return outIndex;
}

/*
	Converts UTF-32 to UTF-8. Every UniChar that reaches here came from one of the decoders above or from IMPD
	itself, so the input is already known to be a valid code point.
*/
static String convertUniToUTF8String(const UniString& s) {
	if (s.empty()) {
		return String();
	}
	String result(calcUTF32ToUTF8Size(s.size(), s.data()), 0);
	convertUTF32ToUTF8(s.size(), s.data(), &result[0]);
	return result;
}

/*
	Converts the platform wide string to UTF-8.
*/
static String convertWideToUTF8String(const WideString& s) {
	return convertUniToUTF8String(convertWideToUniString(s));
}

} // namespace IMPD

#endif
