#ifndef UINT32_CHAR_TRAITS_H
#define UINT32_CHAR_TRAITS_H

#include <cstdint>
#include <cstring>
#include <ios>

#if defined(_LIBCPP_VERSION)
namespace std {
template<> struct char_traits<uint32_t> {
	typedef uint32_t char_type;
	typedef uint32_t int_type;
	typedef streamoff off_type;
	typedef streampos pos_type;
	typedef mbstate_t state_type;
	static void assign(char_type& r, const char_type& c) { r = c; }
	static bool eq(char_type a, char_type b) { return a == b; }
	static bool lt(char_type a, char_type b) { return a < b; }
	static int compare(const char_type* s1, const char_type* s2, size_t n) {
		for (size_t i = 0; i < n; ++i) {
			if (lt(s1[i], s2[i])) return -1;
			if (lt(s2[i], s1[i])) return 1;
		}
		return 0;
	}
	static size_t length(const char_type* s) {
		size_t i = 0;
		while (s[i] != 0) ++i;
		return i;
	}
	static const char_type* find(const char_type* s, size_t n, const char_type& a) {
		for (size_t i = 0; i < n; ++i) {
			if (eq(s[i], a)) return s + i;
		}
		return 0;
	}
	static char_type* move(char_type* dest, const char_type* src, size_t n) {
		return static_cast<char_type*>(memmove(dest, src, n * sizeof(char_type)));
	}
	static char_type* copy(char_type* dest, const char_type* src, size_t n) {
		return static_cast<char_type*>(memcpy(dest, src, n * sizeof(char_type)));
	}
	static char_type* assign(char_type* dest, size_t n, char_type a) {
		for (size_t i = 0; i < n; ++i) dest[i] = a;
		return dest;
	}
	static int_type to_int_type(char_type c) { return c; }
	static char_type to_char_type(int_type c) { return c; }
	static bool eq_int_type(int_type a, int_type b) { return a == b; }
	static int_type eof() { return static_cast<int_type>(-1); }
	static int_type not_eof(int_type c) { return eq_int_type(c, eof()) ? 0 : c; }
};
}
#endif

#endif // UINT32_CHAR_TRAITS_H
