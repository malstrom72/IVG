"use strict";
// ===== Output template (shared via core) =====
// DEBUG gate for eval-related UI controls. Default off for production.
var DEBUG = false;
function buildTemplate(zeroTerminated) {
	return makeCTemplate({
		zeroTerminated: !!zeroTerminated,
		functionName: "<<findSomething>>",
		header: "/* Built with QuickHashGen */\n",
		includeSeedComment: true,
		includeAssert: true,
	});
}
// ===== DOM wiring & state =====
var HTML_ELEMENTS = [
	"editor",
	"solutionsCount",
	"complexity",
	"cost",
	"tableSize",
	"testedCount",
	"requireZeroTermination",
	"allowMultiplications",
	"allowLength",
	"evalTest",
	"forceEval",
	"hashes",
	"startStop",
	"testStatus",
];
var elements = {};
for (var i = 0; i < HTML_ELEMENTS.length; ++i) elements[HTML_ELEMENTS[i]] = document.getElementById(HTML_ELEMENTS[i]);
// Hide eval-related controls in non-debug mode
try {
	if (!DEBUG) {
		if (elements.evalTest) {
			if (elements.evalTest.parentElement) elements.evalTest.parentElement.style.display = "none";
			elements.evalTest.checked = false;
			elements.evalTest.disabled = true;
		}
		if (elements.forceEval) {
			if (elements.forceEval.parentElement) elements.forceEval.parentElement.style.display = "none";
			elements.forceEval.checked = false;
			elements.forceEval.disabled = true;
		}
		if (elements.testStatus) {
			// Hide the self-tests/mode status line entirely when not debugging
			elements.testStatus.style.display = "none";
			elements.testStatus.textContent = "";
		}
	}
} catch (err) {
	console.error("Failed to apply DEBUG gating to controls", err);
}
// Stop on edits/toggles
var isRunning = false;
if (elements.editor)
	elements.editor.addEventListener("input", function () {
		if (isRunning) {
			isRunning = false;
			elements.startStop.textContent = "Start";
		}
	});
if (elements.allowMultiplications)
	elements.allowMultiplications.addEventListener("change", function () {
		if (isRunning) {
			isRunning = false;
			elements.startStop.textContent = "Start";
		}
	});
if (elements.allowLength)
	elements.allowLength.addEventListener("change", function () {
		if (isRunning) {
			isRunning = false;
			elements.startStop.textContent = "Start";
		}
	});
if (elements.requireZeroTermination)
	elements.requireZeroTermination.addEventListener("change", function () {
		currentTemplate = buildTemplate(elements.requireZeroTermination.checked);
		// Apply signature/assert/return rewrite so the editor stays in sync with the toggle
		try {
			var code = elements.editor.value || "";
			if (code) {
				elements.editor.value = rewriteZeroTerminationMode(code, elements.requireZeroTermination.checked);
				lastInputText = elements.editor.value;
			}
		} catch (err) {
			console.error("Failed to rewrite zero-termination mode", err);
		}
		if (isRunning) {
			isRunning = false;
			elements.startStop.textContent = "Start";
		}
	});
if (elements.forceEval)
	elements.forceEval.addEventListener("change", function () {
		ENGINE_USE_EVAL = !!(DEBUG && EVAL_ALLOWED && elements.forceEval.checked);
		if (isRunning) {
			isRunning = false;
			elements.startStop.textContent = "Start";
		}
		updateModeLabel();
	});
var currentTemplate = buildTemplate(true);
var theHashMaker = null,
	lastInputText = elements.editor.value,
	solutionsCounter = 0,
	strings = [],
	minSize,
	maxSize,
	best = null,
	currentSeed = 0;
var ENGINE_USE_EVAL = false;
var EVAL_ALLOWED = false;
// ===== Controls =====
function toggleRun() {
	if (isRunning) {
		isRunning = false;
		elements.startStop.textContent = "Start";
		return;
	}
	isRunning = true;
	elements.startStop.textContent = "Stop";
	resetSearch(); // re-parse editor (C++ or raw list)
	updateCodeMetadata(); // sync STRINGS length and min/max guard
}
window.toggleRun = toggleRun; // expose for inline onclick
function parseStringsFromEditor(text) {
	var p = text.indexOf("STRINGS");
	if (p >= 0) {
		p = text.indexOf("{", p);
		if (p < 0) throw new Error("No '{' after STRINGS.");
		var i = p + 1,
			depth = 1,
			body = "";
		while (i < text.length && depth > 0) {
			var ch = text[i++];
			if (ch === "{") depth++;
			else if (ch === "}") depth--;
			if (depth > 0) body += ch;
		}
		var out = [],
			j = 0;
		while (j < body.length) {
			while (j < body.length && body[j] !== '"' && body[j] !== "'") j++;
			if (j >= body.length) break;
			var parsed = parseCString(body.slice(j));
			out.push(parsed[0]);
			j += parsed[1];
		}
		if (out.length === 0) throw new Error("Found STRINGS but no string literals.");
		return out;
	}
	return parseQuickHashGenInput(text);
}
// findInitializerRange and findMatchingSquare are provided by the core.

function detectEvalAllowed() {
	try {
		var f = eval("(function(n,w){return (n|0)+(w[0]|0);})");
		return f(1, [2]) === 3;
	} catch (err) {
		console.error("eval not allowed", err);
		return false;
	}
}
function stopAndReport(header, details) {
	try {
		isRunning = false;
		if (elements.startStop) elements.startStop.textContent = "Start";
	} catch (err) {
		console.error("Failed to update run state", err);
	}
	var msg = "[FAIL] " + header;
	var body = "";
	try {
		body = "\n" + JSON.stringify(details, null, 2);
	} catch (err) {
		console.error("Failed to stringify details", err);
		body = "\n" + String(details);
	}
	try {
		var prev = elements.hashes.textContent || "";
		elements.hashes.textContent = (prev ? prev + "\n" : "") + msg + body;
		console.error("QuickHashGen failure:", header, details);
	} catch (err) {
		console.error("Failed to report failure", err);
	}
}
function updateModeLabel() {
	if (!DEBUG) return;
	try {
		var base = elements.testStatus.textContent || "";
		var core = base.split(" | ")[0];
		var mode = ENGINE_USE_EVAL ? "eval" : EVAL_ALLOWED ? "csp-safe (manual)" : "csp-safe (CSP)";
		var text = core + (core ? " | " : "") + "Mode: " + mode;
		if (best && best.evalInfo) {
			text += " | Eval: OK (" + best.evalInfo.checked + " checked)";
		}
		elements.testStatus.textContent = text;
	} catch (err) {
		console.error("Failed to update mode label", err);
	}
}
function applyBestToEditor(found) {
	var code = elements.editor.value || "";
	var updated = updateCCodeWithSolution(code, theHashMaker, found, {
		zeroTerminated: elements.requireZeroTermination.checked,
		functionName: "<<findSomething>>",
		header: "/* Built with QuickHashGen */\n",
		includeSeedComment: true,
		includeAssert: true,
	});
	elements.editor.value = updated;
	lastInputText = updated;
}
function resetSearch() {
	theHashMaker = null;
	solutionsCounter = 0;
	lastInputText = elements.editor.value;
	best = null;
	EVAL_ALLOWED = detectEvalAllowed();
	if (elements.forceEval) {
		if (EVAL_ALLOWED) {
			elements.forceEval.disabled = !DEBUG ? true : false;
		} else {
			elements.forceEval.checked = false;
			elements.forceEval.disabled = true;
		}
	}
	ENGINE_USE_EVAL = !!(DEBUG && EVAL_ALLOWED && elements.forceEval && elements.forceEval.checked);
	updateModeLabel();
	try {
		strings = parseStringsFromEditor(lastInputText);
	} catch (err) {
		console.error("Failed to parse strings from editor", err);
		strings = [];
	}
	if (strings.length > 0) {
		var bounds = computeTableBounds(strings);
		minSize = bounds.minSize;
		maxSize = bounds.maxSize;
		var parsedSeed = parseSeedComment(lastInputText);
		currentSeed = typeof parsedSeed === "number" ? parsedSeed >>> 0 : (Math.random() * 0x100000000) >>> 0;
		theHashMaker = new QuickHashGen(
			strings,
			minSize,
			maxSize,
			elements.requireZeroTermination.checked,
			elements.allowMultiplications.checked,
			elements.allowLength.checked,
			DEBUG ? ENGINE_USE_EVAL : false,
			DEBUG && elements.evalTest ? elements.evalTest.checked : false,
			currentSeed,
		);
		elements.hashes.textContent = "";
		elements.testedCount.textContent = "0";
		elements.solutionsCount.textContent = "0";
		elements.complexity.textContent = "?";
		elements.cost.textContent = "?";
		elements.tableSize.textContent = "?";
	}
}
function updateCodeMetadata() {
	try {
		var code = elements.editor.value || "";
		if (code.indexOf("STRINGS") < 0) return;
		var list = Array.isArray(strings) && strings.length ? strings : parseStringsFromEditor(code);
		var updated = updateCCodeMetadata(code, list);
		elements.editor.value = updated;
		lastInputText = updated;
	} catch (err) {
		console.error("Failed to update code metadata", err);
	}
}
function updateOutput() {
	if (best !== null) {
		try {
			applyBestToEditor(best);
		} catch (err) {
			console.error("Failed to apply best solution", err);
		}
		var tableSize = best.table.length;
		var header = "Legend: idx = hash & (tableSize-1) | tableSize=" + tableSize + "\n" + "<string> -> idx=<index> (hash=<raw>)\n";
		var lines = [header];
		for (var i = 0; i < strings.length; ++i) {
			var raw = best.hashes[i];
			var idx = raw & (tableSize - 1);
			lines.push(escapeCString(strings[i]) + " -> idx=" + idx + " (hash=" + raw + ")");
		}
		elements.hashes.textContent = lines.join("\n");
	} else elements.hashes.textContent = "";
	updateModeLabel();
}
function intervalFunction() {
	try {
		if (!isRunning) return;
		if (elements.editor.value !== lastInputText) resetSearch();
		if (theHashMaker !== null) {
			var timeOut = Date.now() + 100;
			while (Date.now() - timeOut < 0) {
				var rng = {
					nextInt: function (m) {
						return theHashMaker.randomInt(m);
					},
				};
				var found = scheduleStep(theHashMaker, best, rng, strings.length, undefined);
				if (found !== null) {
					if (best === null || found.cost < best.cost || (found.cost === best.cost && found.table.length < best.table.length)) {
						best = found;
						updateOutput();
					}
					++solutionsCounter;
				}
			}
			elements.testedCount.textContent = theHashMaker.getTestedCount();
			elements.solutionsCount.textContent = solutionsCounter;
			elements.complexity.textContent = best === null ? "?" : best.complexity;
			elements.cost.textContent = best === null ? "?" : best.cost;
			elements.tableSize.textContent = best === null ? "?" : best.table.length;
		}
	} catch (err) {
		console.error("Error in intervalFunction", err);
		elements.hashes.textContent = String(err);
		theHashMaker = null;
		isRunning = false;
		elements.startStop.textContent = "Start";
	}
}
window.setInterval(intervalFunction, 200);
// ===== Zero-termination mode rewriter =====
function rewriteZeroTerminationMode(code, zeroTerminated) {
	try {
		// 1) Update function arg comment
		code = code.replace(
			/\/\*\s*string\s*\((?:zero terminated|zero termination not required)\)\s*\*\//,
			zeroTerminated ? "/* string (zero terminated) */" : "/* string (zero termination not required) */",
		);
		// 2) Toggle assert (preserve indentation and following line's tab)
		if (zeroTerminated) {
			code = code.replace(
				/\t\/\/\s*zero-termination not expected\s*\n\t\/\/\s*assert\(s\[n\]\s*==\s*'\\0'\);/m,
				"\tassert(s[n] == '\\0');",
			);
		} else {
			code = code.replace(/\tassert\(s\[n\]\s*==\s*'\\0'\);/m, "\t// zero-termination not expected\n\t// assert(s[n] == '\\0');");
		}
		// 3) Replace the entire return statement deterministically without regex pitfalls.
		(function () {
			var anchor = code.indexOf("int stringIndex");
			var searchFrom = anchor >= 0 ? anchor : 0;
			var rStart = code.indexOf("return", searchFrom);
			if (rStart < 0) return; // nothing to rewrite
			var lineStart = code.lastIndexOf("\n", rStart);
			lineStart = lineStart < 0 ? 0 : lineStart + 1;
			var i = lineStart;
			while (i < rStart && (code[i] === "\t" || code[i] === " ")) i++;
			var indent = code.slice(lineStart, i);
			var retEnd = code.indexOf(";", rStart);
			if (retEnd < 0) return;
			var after = retEnd + 1;
			while (after < code.length && (code[after] === "\t" || code[after] === " ")) after++;
			if (after < code.length && code[after] === "\r") after++;
			if (after < code.length && code[after] === "\n") after++;
			var newReturn = zeroTerminated
				? "return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;"
				: "return (stringIndex >= 0 && strncmp(s, STRINGS[stringIndex], n) == 0 && STRINGS[stringIndex][n] == 0) ? stringIndex : -1;";
			code = code.slice(0, lineStart) + indent + newReturn + "\n" + code.slice(after);
		})();
	} catch (err) {
		console.error("Error rewriting zero-termination mode", err);
	}
	return code;
}
// ===== Self tests =====
function runSelfTests() {
	if (!DEBUG) return;
	var ok = 0,
		fail = 0,
		failures = [];
	function T(name, fn) {
		try {
			fn();
			ok++;
		} catch (e) {
			console.error("Test fail:", name, e);
			failures.push({ test: name, error: e && e.stack ? e.stack : String(e) });
			fail++;
		}
	}
	T("escape/parse roundtrip", function () {
		var s = "hej \n \"''\\ \x04 \u2414 \0 \r du";
		var esc = escapeCString(s);
		var back = parseCString(esc)[0];
		if (back !== s) throw new Error("roundtrip");
	});
	T("parse list", function () {
		var arr = parseQuickHashGenInput('a\nb\n"c d"');
		if (arr.length !== 3 || arr[2] !== "c d") throw new Error("bad parse");
	});
	T("parse from C", function () {
		var code = 'static const char* STRINGS[2] = { "aa", "bb" };';
		var arr = parseStringsFromEditor(code);
		if (arr.length !== 2 || arr[1] !== "bb") throw new Error("bad extract");
	});
	T("initializer range", function () {
		var code = "int x; static const int HASH_TABLE[4] = { 1, 2, 3, 4 };";
		var ds = code.indexOf("HASH_TABLE[");
		var r = findInitializerRange(code, ds);
		if (!r || code.slice(r.open, r.close + 1).indexOf("{") !== 0) throw new Error("range");
	});
	T("toggleRun exists", function () {
		if (typeof toggleRun !== "function") throw new Error("missing");
		if (typeof window.toggleRun !== "function") throw new Error("not global");
	});
	T("match HASH_TABLE brackets", function () {
		var line = "int stringIndex = HASH_TABLE[((p[1] + n) & 31) ^ (9 < n ? p[9] : 0)];";
		var i = line.indexOf("[");
		var j = findMatchingSquare(line, i);
		if (j !== line.length - 2) throw new Error("mismatch " + j);
	});
	T("rewrite zero-term off/on", function () {
		var code0 =
			"static int f(int n, const char* s /* string (zero terminated) */) {\n\tconst unsigned char* p = (const unsigned char*) s;\n\tassert(s[n] == '\\0');\n\tif (n < 2) return -1;\n\tint stringIndex = 0;\n\treturn (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;\n}";
		var off = rewriteZeroTerminationMode(code0, false);
		if (off.indexOf("zero termination not required") < 0) throw new Error("sig not switched");
		if (off.indexOf("strncmp(") < 0) throw new Error("return not switched");
		if (off.indexOf("\n\t// zero-termination not expected") < 0) throw new Error("assert not commented");
		if (off.indexOf("\n\tif (n < 2)") < 0) throw new Error("indent after assert lost");
		var on = rewriteZeroTerminationMode(off, true);
		if (on.indexOf("zero terminated") < 0) throw new Error("sig not restored");
		if (on.indexOf("strcmp(") < 0) throw new Error("return not restored");
		if (/\tassert\(s\[n\]\s*==\s*'\\0'\)/.test(on) === false) throw new Error("assert not restored");
	});
	var msg = fail === 0 ? "All tests passed (" + ok + ")" : ok + " passed, " + fail + " failed";
	elements.testStatus.textContent = "Self-tests: " + msg + ".";
	if (fail > 0) {
		stopAndReport("Self-tests failed", { summary: msg, failures: failures });
	}
}
try {
	runSelfTests();
} catch (err) {
	console.error("Self-tests failed", err);
}
