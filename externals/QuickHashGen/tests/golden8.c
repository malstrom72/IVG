/* Built with QuickHashGen CLI */
// Seed: 19
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[10] = {
		"!!!", "!!@", "!@!", "@!!", "???", "??!", "?!?", "!??", "###", "##!"
	};
	static const int HASH_TABLE[128] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, 6, -1, -1, -1, -1, -1, 1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 7, 
		-1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 5, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 3, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 
		-1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, -1, -1
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 3 || n > 3) return -1;
	int stringIndex = HASH_TABLE[(84u * (22u + p[2]) * 104u - (p[0] - p[1] - 41u + p[0]) ^ p[0] + (p[1] ^ 7u)) & 127u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
