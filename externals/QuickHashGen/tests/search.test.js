const assert = require("assert");
const fs = require("fs");
const path = require("path");
const core = require("../QuickHashGenCore");

function nextPow2(n) {
	let s = 1;
	while (n > s) s <<= 1;
	return s;
}

const inputPath = path.join(__dirname, "input1.txt");
const strings = core.parseQuickHashGenInput(fs.readFileSync(inputPath, "utf8"));
const minSize = nextPow2(strings.length);
const maxSize = minSize * 8;
const seed = 1;

function findSolution(zeroTerminated) {
	const qh = new core.QuickHashGen(strings, minSize, maxSize, zeroTerminated, true, true, false, false, seed);
	const complexityRng = new core.XorshiftPRNG2x32(seed);
	let best = null;
	const tests = 1000;
	while (qh.getTestedCount() < tests) {
		const complexity = complexityRng.nextInt(best === null ? 32 : best.complexity) + 1;
		const remaining = tests - qh.getTestedCount();
		const iters = Math.max(1, Math.min(remaining, Math.floor(200 / strings.length)));
		const found = qh.search(complexity, iters);
		if (found && (best === null || found.cost < best.cost || (found.cost === best.cost && found.table.length < best.table.length))) {
			best = found;
			if (best.complexity === 1) break;
		}
	}
	return { qh, best };
}

const expr1 = (() => {
	const { qh, best } = findSolution(true);
	return qh.generateCExpression(best);
})();
const expr2 = (() => {
	const { qh, best } = findSolution(true);
	return qh.generateCExpression(best);
})();
assert.strictEqual(expr1, expr2);

const golden1 = fs.readFileSync(path.join(__dirname, "golden1.c"), "utf8");
const match = golden1.match(/HASH_TABLE\[(.*)\];/);
assert(match, "hash expression not found");
assert.strictEqual(expr1, match[1]);
console.log("search deterministic seeding and generateCExpression test passed");

const dupStrings = ["dup", "dup"];
const minDup = nextPow2(dupStrings.length);
const maxDup = minDup * 8;
let dupThrown = false;
try {
	new core.QuickHashGen(dupStrings, minDup, maxDup, true, true, true, false, false, seed);
} catch (e) {
	dupThrown = true;
}
assert(dupThrown);
console.log("search duplicate strings test passed");

const { best: nzBest } = findSolution(false);
assert(nzBest !== null);
console.log("search zero-termination toggle test passed");
