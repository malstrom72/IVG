var quickHashGen = require("../QuickHashGenCore");

var QuickHashGen = quickHashGen.QuickHashGen;
var XorshiftPRNG2x32 = quickHashGen.XorshiftPRNG2x32;

var MAX_COMPLEXITY = 32;
var MAX_SIZE_MULTIPLIER = 8;

var TEMPLATE =
	"#include <iostream>\n" +
	"#include <cstring>\n" +
	"#include <cassert>\n" +
	"\n" +
	"static int lookup(int n /* string length */, const char* s /* zero-terminated string */) {\n" +
	"       static const char* STRINGS[${stringCount}] = {\n" +
	"               ${stringList}\n" +
	"       };\n" +
	"       static const int HASH_TABLE[${tableSize}] = {\n" +
	"               ${tableData}\n" +
	"       };\n" +
	"       const unsigned char* p = (const unsigned char*) s;\n" +
	"       assert(s[n] == '\\0');\n" +
	"       if (n < ${minLength} || n > ${maxLength}) return -1;\n" +
	"       int stringIndex = HASH_TABLE[${hashExpression}];\n" +
	"       return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;\n" +
	"}\n" +
	"\n" +
	"int main()\n" +
	"{\n" +
	"    static const char* STRINGS[${stringCount}] = {\n" +
	"               ${stringList}\n" +
	"       };\n" +
	"    for (int i = 0; i < ${stringCount}; ++i) {\n" +
	"        // std::cout << STRINGS[i] << std::endl;\n" +
	"        int j = lookup(strlen(STRINGS[i]), STRINGS[i]);\n" +
	"        if (j != i) {\n" +
	'            std::cout << std::endl << "BAD!! (" << i << "==" << j << ")" << std::endl;\n' +
	"            return 1;\n" +
	"        }\n" +
	"    }\n" +
	"   \n" +
	'   std::cout << std::endl << "GOOD!!" << std::endl;\n' +
	"   return 0;\n" +
	"}\n";

var seed = process.argv[2] ? parseInt(process.argv[2], 10) : (Math.random() * 1000000 + 1) | 0;
process.stdout.write("// seed: " + seed + "\n");
// Dedicated PRNG for generating the random string set.
var rnd = new XorshiftPRNG2x32(seed);

var wordCount = rnd.nextInt(100) + 1;
var strings = [];
var didStrings = {};
for (var i = 0; i < wordCount; ++i) {
	var wordLength = rnd.nextInt(20);
	var randomWord = "";
	for (var j = 0; j < wordLength; ++j) {
		randomWord += String.fromCharCode(1 + rnd.nextInt(256 - 1));
	}
	if (!(randomWord in didStrings)) {
		didStrings[randomWord] = true;
		strings.push(randomWord);
	}
}

var minSize;
for (minSize = 1; strings.length > minSize; minSize <<= 1);
var maxSize = minSize * MAX_SIZE_MULTIPLIER;

// Seed QuickHashGen's internal PRNG deterministically so that a given seed
// for this test produces identical behaviour across runs.
// Without explicit seeds QuickHashGen would fall back to Math.random(),
// resulting in nondeterministic failures when running tests/fuzzLoop.sh.
var hashSeed0 = rnd.nextInt32();
var hashSeed1 = rnd.nextInt32();
var theHashMaker = new QuickHashGen(strings, minSize, maxSize, true, true, true, false, false, hashSeed0, hashSeed1);

var found = null;
while (found === null) {
	var complexity = theHashMaker.randomInt(MAX_COMPLEXITY) + 1;
	found = theHashMaker.search(complexity, 100000);
}

if (found !== null) {
	// Verify generated JS hash implementations before emitting C++.
	var jsExpr = theHashMaker.generateJSExpression(found);
	var evalFn;
	try {
		evalFn = eval("(function(n,w){ return " + jsExpr + "; })");
	} catch (e) {
		console.error("Eval compile error: " + (e && e.message ? e.message : String(e)));
		process.exit(1);
	}
	var jsFn = theHashMaker.generateJSEvaluator(found);
	// Prepare input arrays with zero padding up to the longest string.
	var maxLen = 0;
	for (var i = 0; i < strings.length; ++i) {
		var L = strings[i].length;
		if (L > maxLen) maxLen = L;
	}
	for (var j = 0; j < strings.length; ++j) {
		var str = strings[j];
		var n = str.length;
		var arr = new Array(maxLen);
		for (var k = 0; k < maxLen; ++k) {
			arr[k] = k < n ? str.charCodeAt(k) : 0;
		}
		var h1 = evalFn(n, arr);
		var h2 = jsFn(n, arr);
		if (h1 !== h2) {
			console.error("JS expression and function mismatch on #" + j);
			process.exit(1);
		}
		var idx = found.table[h1];
		if (idx !== j) {
			console.error("JS lookup mismatch on #" + j);
			process.exit(1);
		}
	}

	process.stdout.write(theHashMaker.generateCOutput(TEMPLATE, found));
	process.exit(0);
} else {
	process.exit(1);
}
