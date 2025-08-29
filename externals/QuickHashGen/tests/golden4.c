/* Built with QuickHashGen CLI */
// Seed: 7
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[5] = {
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 
		"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy", 
		"yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy", 
		"abcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyzabcxyz", 
		"01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567"
	};
	static const int HASH_TABLE[16] = {
		1, 2, -1, -1, -1, -1, -1, 4, -1, 3, -1, -1, -1, -1, -1, 0
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 128 || n > 240) return -1;
	int stringIndex = HASH_TABLE[(((0u + 24u) >> 27 ^ p[127]) + p[51] + 31u) & 15u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
