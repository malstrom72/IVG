#!/usr/bin/env node
"use strict";

const fs = require("fs");
const core = require("./QuickHashGenCore");

function printUsage() {
	console.error(
		[
			"Usage: node QuickHashGenCLI.node.js [options] [input-file]",
			"Options:",
			"  -h, --help               show usage information",
			"  --tests N                number of expressions to try (default 100000)",
			"  --no-multiplications     disallow multiplications",
			"  --no-length              disallow use of n (string length)",
			"  --no-zero-termination    allow non-zero-terminated strings",
			"  --seed N                 seed the random generator",
			"  --bench                  benchmark solution generation speed",
			"  --eval-test              verify result with eval engine (testing)",
			"  --force-eval             use eval engine (if available) (testing)",
			"",
			"Strings are read from [input-file] or stdin. Each line may be",
			"plain text or one or more C-style quoted strings separated by",
			"commas or whitespace.",
		].join("\n"),
	);
}

let args = process.argv.slice(2);
let opts = {
	tests: 100000,
	allowMultiplications: true,
	allowLength: true,
	requireZeroTermination: true,
	evalTest: false,
	forceEval: false,
	bench: false,
};
let inputFile = null;
for (let i = 0; i < args.length; ++i) {
	const a = args[i];
	if (a === "--help" || a === "-h") {
		printUsage();
		process.exit(0);
	} else if (a === "--tests" && i + 1 < args.length) {
		opts.tests = parseInt(args[++i], 10);
		if (!Number.isFinite(opts.tests) || opts.tests < 0) {
			printUsage();
			process.exit(1);
		}
	} else if (a === "--no-multiplications") {
		opts.allowMultiplications = false;
	} else if (a === "--no-length") {
		opts.allowLength = false;
	} else if (a === "--no-zero-termination") {
		opts.requireZeroTermination = false;
	} else if (a === "--eval-test") {
		opts.evalTest = true;
	} else if (a === "--force-eval") {
		opts.forceEval = true;
	} else if (a === "--bench") {
		opts.bench = true;
	} else if (a === "--seed" && i + 1 < args.length) {
		opts.seed = parseInt(args[++i], 10);
		if (!Number.isFinite(opts.seed) || opts.seed < 0) {
			printUsage();
			process.exit(1);
		}
	} else if (a[0] === "-") {
		printUsage();
		process.exit(1);
	} else {
		inputFile = a;
	}
}

let inputText;
if (inputFile) {
	inputText = fs.readFileSync(inputFile, "utf8");
} else {
	inputText = fs.readFileSync(0, "utf8");
}

let strings = core.parseQuickHashGenInput(inputText);
if (!strings.length) {
	console.error("No strings provided");
	process.exit(1);
}

const bounds = core.computeTableBounds(strings);
let minSize = bounds.minSize;
let maxSize = bounds.maxSize;

let seed = typeof opts.seed === "number" ? opts.seed : (Math.random() * 0x100000000) >>> 0;
let complexityPRNG = new core.XorshiftPRNG2x32(seed);
let qh = new core.QuickHashGen(
	strings,
	minSize,
	maxSize,
	opts.requireZeroTermination,
	opts.allowMultiplications,
	opts.allowLength,
	opts.forceEval,
	opts.evalTest,
	seed,
);
let best = null;
while (qh.getTestedCount() < opts.tests) {
	const remaining = opts.tests - qh.getTestedCount();
	const found = core.scheduleStep(qh, best, complexityPRNG, strings.length, remaining);
	if (found && (best === null || found.cost < best.cost || (found.cost === best.cost && found.table.length < best.table.length))) {
		best = found;
		if (best.complexity === 1) break;
	}
}

if (!best) {
	console.error("No solution found after " + qh.getTestedCount() + " tests");
	process.exit(1);
}

if (opts.bench) {
	function benchGeneration(useEval) {
		let complexityRng = new core.XorshiftPRNG2x32(seed);
		let qhBench = new core.QuickHashGen(
			strings,
			minSize,
			maxSize,
			opts.requireZeroTermination,
			opts.allowMultiplications,
			opts.allowLength,
			useEval,
			false,
			seed,
		);
		let bestBench = null;
		const start = process.hrtime.bigint();
		while (qhBench.getTestedCount() < opts.tests) {
			let complexity = complexityRng.nextInt(bestBench === null ? 32 : bestBench.complexity) + 1;
			let remaining = opts.tests - qhBench.getTestedCount();
			let iters = Math.max(1, Math.min(remaining, Math.floor(200 / strings.length)));
			let found = qhBench.search(complexity, iters);
			if (
				found &&
				(bestBench === null ||
					found.cost < bestBench.cost ||
					(found.cost === bestBench.cost && found.table.length < bestBench.table.length))
			) {
				bestBench = found;
				if (bestBench.complexity === 1) break;
			}
		}
		const elapsed = Number(process.hrtime.bigint() - start) / 1e6;
		return elapsed;
	}
	let tFunc = benchGeneration(false);
	let tEval = benchGeneration(true);
	console.error("Generation benchmark (" + opts.tests + " tests): func=" + tFunc.toFixed(2) + "ms eval=" + tEval.toFixed(2) + "ms");
}

const TEMPLATE = core.makeCTemplate({
	zeroTerminated: opts.requireZeroTermination,
	functionName: "lookup",
	header: "/* Built with QuickHashGen CLI */\n",
	includeSeedComment: true,
	includeAssert: false,
});
const out = qh.generateCOutput(TEMPLATE, best);
process.stdout.write(out);
