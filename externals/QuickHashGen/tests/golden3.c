/* Built with QuickHashGen CLI */
// Seed: 10
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[18] = {
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy", "aaaaa", "aaaab", "aaaba", "aabaa", 
		"abaaa", "baaaa", "aaaac", "aaaca", "aacaa", "acaaa", "caaaa", "zzzza", 
		"zzzaz", "zzazz", "zazzz", "azzzz"
	};
	static const int HASH_TABLE[256] = {
		-1, -1, 6, -1, -1, -1, -1, 1, -1, -1, -1, -1, -1, -1, -1, -1, 
		17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, 4, -1, -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 12, -1, 
		-1, -1, -1, -1, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, 13, -1, -1, -1, -1, -1, -1, 5, -1, -1, -1, 16, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, 10, -1, -1, -1, -1, -1, 11, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, -1, -1, -1, 14, -1, -1, -1, 7, -1, -1, -1, -1, -1, -1, -1
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 5 || n > 40) return -1;
	int stringIndex = HASH_TABLE[((n ^ n + p[3] + n ^ 219u + p[4]) * (((0u + (p[0] + ((0u + p[5]) << 25) + 114u + p[1]) * p[2]) >> 3) + (39 < n ? p[39] : 0) - (n + p[4] + (p[0] ^ 66u)))) & 255u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
