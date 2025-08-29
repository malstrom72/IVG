const assert = require("assert");
const fs = require("fs");
const path = require("path");
const core = require("../QuickHashGenCore");

// computeTableBounds / iterationsFor
{
	const strings = ["a", "b", "c"]; // length 3 -> nextPow2 = 4, max = 32
	const { minSize, maxSize } = core.computeTableBounds(strings);
	assert.strictEqual(minSize, 4);
	assert.strictEqual(maxSize, 32);
	assert.strictEqual(core.iterationsFor(strings.length, 200), Math.max(1, Math.floor(200 / 3)));
}

// seed helpers
{
	const txt = "/* Built with QuickHashGen */\n// Seed: 123\nint x;\n";
	assert.strictEqual(core.parseSeedComment(txt), 123);
	assert.strictEqual(core.formatSeedComment(456), "// Seed: 456");
}

// bracket/initializer helpers
{
	const code = "static const int HASH_TABLE[4] = { 1, 2, { 3, 4 }, 5 };";
	const ds = code.indexOf("HASH_TABLE[");
	const rng = core.findInitializerRange(code, ds);
	assert(rng && code[rng.open] === "{" && code[rng.close] === "}");
	const line = "int stringIndex = HASH_TABLE[(a[b[2]] & 31) ^ 7];";
	const open = line.indexOf("[");
	const close = core.findMatchingSquare(line, open);
	assert.strictEqual(close, line.lastIndexOf("]") - 0);
}

// updateCCodeWithSolution round-trip
{
	const inputPath = path.join(__dirname, "input1.txt");
	const strings = core.parseQuickHashGenInput(fs.readFileSync(inputPath, "utf8"));
	const { minSize, maxSize } = core.computeTableBounds(strings);
	const seed = 1;
	const qh = new core.QuickHashGen(strings, minSize, maxSize, true, true, true, false, false, seed);
	const rng = new core.XorshiftPRNG2x32(seed);
	let best = null;
	const tests = 1000;
	while (qh.getTestedCount() < tests) {
		const found = core.scheduleStep(qh, best, rng, strings.length, tests - qh.getTestedCount());
		if (found && (best === null || found.cost < best.cost || (found.cost === best.cost && found.table.length < best.table.length))) {
			best = found;
			if (best.complexity === 1) break;
		}
	}
	assert(best, "no solution found for helper round-trip test");
	const tpl = core.makeCTemplate({
		zeroTerminated: true,
		functionName: "lookup",
		header: "/* Built with QuickHashGen CLI */\n",
		includeSeedComment: true,
		includeAssert: false,
	});
	const full = qh.generateCOutput(tpl, best);
	const patched = core.updateCCodeWithSolution(full, qh, best, {
		zeroTerminated: true,
		functionName: "lookup",
		header: "/* Built with QuickHashGen CLI */\n",
		includeSeedComment: true,
		includeAssert: false,
	});
	assert.strictEqual(patched, full);
}

console.log("helpers tests passed");

// updateCCodeMetadata should update STRINGS size and n-guard
{
	const code0 =
		"/* Built with QuickHashGen */\n" +
		"// Seed: 1\n" +
		"static int lookup(int n, const char* s /* string (zero terminated) */) {\n" +
		'\tstatic const char* STRINGS[2] = { "a", "bbb" };\n' +
		"\tstatic const int HASH_TABLE[4] = { 0, 1, 2, 3 };\n" +
		"\tconst unsigned char* p = (const unsigned char*) s;\n" +
		"\tif (n < 0 || n > 0) return -1;\n" +
		"\tint stringIndex = HASH_TABLE[(n + p[0]) & 3];\n" +
		"\treturn (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;\n" +
		"}\n";
	const strings2 = ["a", "bbb", "cc"]; // minLen=1, maxLen=3, count=3
	const updated = core.updateCCodeMetadata(code0, strings2);
	// STRINGS size updated
	assert(updated.includes("STRINGS[3]"));
	// Guard updated to new min/max
	assert(updated.includes("if (n < 1 || n > 3)"));
}
