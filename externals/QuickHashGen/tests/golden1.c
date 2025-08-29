/* Built with QuickHashGen CLI */
// Seed: 1
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[3] = {
		"red", "green", "blue"
	};
	static const int HASH_TABLE[4] = {
		2, 1, -1, 0
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 3 || n > 5) return -1;
	int stringIndex = HASH_TABLE[(n) & 3u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
