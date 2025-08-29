/* Built with QuickHashGen CLI */
// Seed: 13
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[20] = {
		"baaaa", "abaaa", "aabaa", "aaaba", "aaaab", "caaaa", "acaaa", "aacaa", 
		"aaaca", "aaaac", "daaaa", "adaaa", "aadaa", "aaada", "aaaad", "eaaaa", 
		"aeaaa", "aaeaa", "aaaea", "aaaae"
	};
	static const int HASH_TABLE[64] = {
		-1, -1, 14, -1, -1, 13, -1, -1, 1, 16, -1, 17, -1, 8, -1, 19, 
		-1, -1, -1, -1, 7, 3, -1, -1, -1, -1, -1, -1, -1, -1, 11, -1, 
		-1, -1, 0, -1, -1, -1, -1, 5, -1, 4, -1, -1, 10, -1, -1, 12, 
		-1, 15, -1, 6, -1, -1, 9, -1, 2, -1, -1, -1, -1, 18, -1, -1
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 5 || n > 5) return -1;
	int stringIndex = HASH_TABLE[(((0u + (p[0] + 54u + n - 176u) * n * (p[2] - p[4])) >> 3) + n * p[0] + p[2] * p[5] - (44u * n - p[1] - 163u) * ((n * p[3] ^ 83u * 53u) + p[2])) & 63u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
