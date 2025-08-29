"use strict";
// Core hashing algorithm extracted for reuse in browser and CLI tools.

// Enable debug assertions by setting NODE_ENV=development.
var DEBUG = typeof process !== "undefined" && process && process.env && process.env.NODE_ENV === "development";

var assert;
if (DEBUG) {
	var AssertionError = function (message) {
		this.name = "AssertionError";
		this.message = message || "";
	};
	AssertionError.prototype = Error.prototype;
	assert = function (condition, message) {
		if (!condition) {
			if (typeof console !== "undefined" && typeof console.assert === "function") {
				console.assert(condition, message);
			}
			throw new AssertionError(message);
		}
	};
} else {
	assert = function () {};
}

var C_ESCAPE_CHARS = "\\\"\'abfnrtv?";

var C_ESCAPE_CODES = "\\\"\'\x07\b\f\n\r\t\v?";

if (DEBUG) assert(C_ESCAPE_CODES.length === C_ESCAPE_CHARS.length, "C_ESCAPE_CODES.length === C_ESCAPE_CHARS.length");

function XorshiftPRNG2x32(seed0, seed1) {
	if (typeof seed0 === "undefined") seed0 = 123456789;
	if (typeof seed1 === "undefined") seed1 = 362436069;

	var px = seed0;
	var py = seed1;

	function next() {
		var t = px ^ (px << 10);
		px = py;
		py = py ^ (py >>> 13) ^ t ^ (t >>> 10);
	}

	this.nextInt32 = function () {
		next();
		return py >>> 0;
	};

	this.nextFloat = function () {
		next();
		return (py >>> 0) * 2.3283064365386962890625e-10 + (px >>> 0) * 5.42101086242752217003726400434970855712890625e-20;
	};

	this.nextInt = function (max) {
		next();
		return (py >>> 0) % max;
	};

	this.clone = function () {
		return new XorshiftPRNG2x32(px, py);
	};
}

// Returns pair: [ string, length ]
function parseCString(s) {
	if (DEBUG) assert(s[0] === '\"' || s[0] === "'", "s[0] === '\"' || s[0] === '\''");

	var o = "";
	var i = 1;
	var b = 1;
	var endChar = s[0];

	while (i < s.length && s[i] !== endChar && s[i] !== "\r" && s[i] !== "\n") {
		if (s[i] === "\\") {
			if (i + 1 >= s.length) throw new Error("Unterminated C escape sequence");
			o += s.substring(b, i);
			var c = s[i + 1];
			i += 2;
			var index = C_ESCAPE_CHARS.indexOf(c);
			if (index >= 0) o += C_ESCAPE_CODES[index];
			else {
				b = i;
				switch (c) {
					case "\r":
						if (i < s.length && s[i] === "\n") ++i;
						break;
					case "\n":
						break;

					case "u":
						while (i < s.length && "0123456789abcdefABCDEF".indexOf(s[i]) >= 0 && i < b + 4) ++i;
						if (i !== b + 4) throw new Error("Illegal C escape sequence");
						o += String.fromCharCode(parseInt(s.substring(b, i), 16));
						break;

					case "x":
						while (i < s.length && "0123456789abcdefABCDEF".indexOf(s[i]) >= 0) ++i;
						if (i === b) throw new Error("Illegal C escape sequence");
						o += String.fromCharCode(parseInt(s.substring(b, i), 16));
						break;

					default:
						b = --i;
						while (i < s.length && "01234567".indexOf(s[i]) >= 0 && i < b + 3) ++i;
						if (i === b) throw new Error("Illegal C escape sequence");
						o += String.fromCharCode(parseInt(s.substring(b, i), 8));
						break;
				}
			}
			b = i;
		} else {
			++i;
		}
	}
	if (s[i] !== endChar) throw new Error("Unterminated C string");
	return [o + s.substring(b, i), i + 1];
}

function toHex(i, length) {
	if (DEBUG) assert(1 <= length && length <= 16, "1 <= length && length <= 16");
	var s = i.toString(16);
	return ("0000000000000000".slice(0, length - s.length) + s).slice(-length);
}

function escapeCString(s) {
	var o = '"';
	var b = 0;
	var forbid = "";
	for (var i = 0; i < s.length; ++i) {
		var c = s[i];
		var v = c.charCodeAt(0);
		if (v < 32 || v >= 127 || c === '\"' || c === "\\" || forbid.indexOf(c) >= 0) {
			forbid = "";
			o += s.substring(b, i);
			var index = C_ESCAPE_CODES.indexOf(c);
			if (index >= 0) {
				o += "\\" + C_ESCAPE_CHARS[index];
			} else if (v === 0) {
				o += "\\0";
				forbid = "01234567";
			} else if (v <= 0xff) {
				o += "\\x" + toHex(v, 2);
				forbid = "0123456789abcdefABCDEF";
			} else {
				o += "\\u" + toHex(v, 4);
			}
			b = i + 1;
		} else {
			forbid = "";
		}
	}
	o += s.substring(b, i) + '"';
	//	process.stderr.write(s + " -> " + o + " -> " + parseCString(o)[0] + "\n");
	if (DEBUG) assert(parseCString(o)[0] === s, "parseCString(o)[0] === s");
	return o;
}

if (DEBUG) {
	// Quick-and-dirty tests
	var p = parseCString('"ab\\0cdef\\\\gjio\\n\\\r\nx\\x45\\u0045\\u0123\\?\\053\\1012end"slack');
	assert(p[0] === "ab\0cdef\\gjio\nxEE\u0123?+A2end" && p[1] === 52);
	assert(escapeCString("hej \n \x22''\\ \x04 \u2414 \0 \r du") === '"hej \\n \\"\'\'\\\\ \\x04 \\u2414 \\0 \\r du"');
	assert(escapeCString("\x050018efgef") === '"\\x05\\x30\\x30\\x31\\x38\\x65\\x66gef"'); // \x in C++ is greedy (stupid)
	assert(escapeCString("\u050018efgef") === '"\\u050018efgef"'); // \u isn't greedy
}

function stringListToC(strings, maxCols, pre) {
	var s = "";
	var l = "";
	var n = strings.length;
	for (var i = 0; i < n; ++i) {
		var w = escapeCString(strings[i]);
		if (i < n - 1) {
			w += ", ";
		}
		if (l.length + w.length > maxCols && l !== "") {
			s += l + "\n" + pre;
			l = "";
		}
		l += w;
	}
	return s + l;
}

var RADIX_PREFIXES = { 8: "0", 10: "", 16: "0x" };

function numberListToC(numbers, elementsPerLine, radix, pre) {
	if (DEBUG) assert(radix === 0 || radix in RADIX_PREFIXES, "radix === 0 || radix in RADIX_PREFIXES");
	var s = "";
	var n = numbers.length;
	for (var i = 0; i < n; ++i) {
		s += radix === 0 ? numbers[i] : RADIX_PREFIXES[radix] + numbers[i].toString(radix);
		if (i < n - 1) {
			s += ", ";
			if (i % elementsPerLine === elementsPerLine - 1) {
				s += "\n" + pre;
			}
		}
	}
	return s;
}

if (!("imul" in Math)) {
	// From https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Math/imul
	Math.imul = function (a, b) {
		var ah = (a >>> 16) & 0xffff;
		var al = a & 0xffff;
		var bh = (b >>> 16) & 0xffff;
		var bl = b & 0xffff;
		// the shift by 0 fixes the sign on the high part
		// the final |0 converts the unsigned value into a signed value
		return (al * bl + (((ah * bl + al * bh) << 16) >>> 0)) | 0;
	};
}

function QuickHashGen(
	strings,
	minTableSize,
	maxTableSize,
	zeroTerminated,
	allowMultiplication,
	allowLength,
	useEvalEngine,
	evalTest,
	seed0,
	seed1,
) {
	var stringSet = Object.create(null);

	// Each QuickHashGen instance maintains its own PRNG to avoid global state.
	if (typeof seed0 === "undefined") {
		seed0 = (Math.random() * 0x100000000) >>> 0;
		seed1 = (Math.random() * 0x100000000) >>> 0;
	} else if (typeof seed1 === "undefined") {
		seed1 = 362436069;
	}
	var prng = new XorshiftPRNG2x32(seed0, seed1);
	this.randomInt = function (max) {
		return prng.nextInt(max);
	};
	this.getSeed = function () {
		return seed0;
	};

	var maxLength = 0;
	var minLength = 10000000;
	for (var i = 0; i < strings.length; ++i) {
		var s = strings[i];
		if (Object.prototype.hasOwnProperty.call(stringSet, s)) {
			throw new Error("String " + escapeCString(s) + " appears more than once");
		}
		stringSet[s] = 1;
		var n = s.length;
		minLength = Math.min(n, minLength);
		maxLength = Math.max(n, maxLength);
	}

	var stringChars = new Array(strings.length);
	for (var i = 0; i < strings.length; ++i) {
		var n = strings[i].length;
		var a = new Array(maxLength + 1);
		for (var j = 0; j <= maxLength; ++j) {
			var c = j < n ? strings[i].charCodeAt(j) : 0;
			a[j] = c;
		}
		stringChars[i] = a;
	}

	var safeLength = zeroTerminated ? minLength + 1 : minLength;

	if (typeof useEvalEngine === "undefined") useEvalEngine = false;
	if (typeof evalTest === "undefined") evalTest = false;

	function verifyEval(foundSolution) {
		var r = buildExpression(foundSolution);
		var expr = "(" + r.exprObj.js + ") & " + r.mask;
		var fn;
		try {
			fn = eval("(function(n,w){return " + expr + ";})");
		} catch (e) {
			throw new Error("Eval compile error: " + (e && e.message ? e.message : String(e)));
		}
		var minLen = Infinity;
		for (var i = 0; i < strings.length; ++i) {
			var L = strings[i].length;
			if (L < minLen) minLen = L;
		}
		if (!isFinite(minLen)) return { checked: 0 };
		var padLen = zeroTerminated ? minLen + 1 : minLen;
		var mod = foundSolution.table.length;
		for (var j = 0; j < strings.length; ++j) {
			var str = strings[j];
			var n = str.length;
			var arr = new Array(Math.max(padLen, n));
			for (var k = 0; k < arr.length; ++k) {
				var c = k < n ? str.charCodeAt(k) : 0;
				if (c >= 128) c -= 256;
				arr[k] = c;
			}
			var idx;
			try {
				idx = fn(n, arr) & (mod - 1);
			} catch (e2) {
				throw new Error("Eval runtime error on #" + j + ": " + (e2 && e2.message ? e2.message : String(e2)));
			}
			var idxFunc = foundSolution.hashes[j] & (mod - 1);
			if (idx !== idxFunc || foundSolution.table[idx] !== j) {
				throw new Error("Eval mismatch on #" + j);
			}
		}
		return { checked: strings.length };
	}

	function generateRandomExpression(rnd, complexity, constantMask, cpp, compact) {
		// Unified generator: builds a single AST that yields
		//  - .c  : C/C++ source
		//  - .js : JS source equivalent
		//  - .fn : CSP-safe evaluator
		// RNG decisions are identical regardless of cpp/js to keep PRNG in sync.
		function leaf_n() {
			return {
				prec: 4,
				c: "n",
				js: "n",
				fn: function (n, w) {
					return n | 0;
				},
				cost: 16,
			};
		}
		function leaf_const() {
			var k = (rnd.nextInt32() & constantMask) | 0;
			var s = String(k);
			return {
				prec: 4,
				c: s + "u",
				js: s,
				fn: function () {
					return k;
				},
				cost: 8,
			};
		}
		function leaf_w_in() {
			var idx = rnd.nextInt(safeLength) | 0;
			return {
				prec: 4,
				c: "p[" + idx + "]",
				js: "(w[" + idx + "]|0)",
				fn: function (n, w) {
					return w[idx] | 0;
				},
				cost: 32,
			};
		}
		function leaf_w_out() {
			var span = (maxLength - safeLength) | 0;
			var i = (rnd.nextInt(Math.max(1, span)) + safeLength) | 0;
			return {
				prec: 4,
				c: "(" + i + " < n ? p[" + i + "] : 0)",
				js: "(w[" + i + "]|0)",
				fn: function (n, w) {
					return w[i] | 0;
				},
				cost: 48,
			};
		}
		function needsPar(a, prec) {
			return !compact || a.prec < prec;
		}
		function wrapC(a, prec) {
			return needsPar(a, prec) ? "(" + a.c + ")" : a.c;
		}
		function wrapJ(a, prec) {
			return needsPar(a, prec) ? "(" + a.js + ")" : a.js;
		}
		function bin(a, b, op, prec, evalFn, opCost) {
			var cLeft = wrapC(a, prec);
			var needRightPar = op === "-" && b.prec === prec;
			var cRight = needRightPar ? "(" + b.c + ")" : wrapC(b, prec);
			var jLeft = wrapJ(a, prec);
			var jRight = needRightPar ? "(" + b.js + ")" : wrapJ(b, prec);
			var c = cLeft + " " + op + " " + cRight;
			var j = jLeft + " " + op + " " + jRight;
			return {
				prec: prec,
				c: c,
				js: j,
				fn: function (n, w) {
					return evalFn(a.fn(n, w), b.fn(n, w));
				},
				cost: a.cost + b.cost + opCost,
			};
		}
		// Shifts in the generated expressions need to behave identically
		// in C and JavaScript.  Right shifts in particular must be
		// logical, because the C code operates on unsigned values.
		// Using JavaScript's arithmetic ">>" caused discrepancies
		// between the JS verification and the generated C program for
		// certain inputs.
		function sh(a, dir, shamt, opCost) {
			var j, c;
			if (dir === "<<") {
				j = wrapJ(a, 1) + " << " + shamt;
				c = "(0u + " + wrapC(a, 1) + ") << " + shamt;
				return {
					prec: 1,
					c: c,
					js: j,
					fn: function (n, w) {
						var v = a.fn(n, w) | 0;
						return (v << shamt) | 0;
					},
					cost: a.cost + opCost,
				};
			} else {
				j = wrapJ(a, 1) + " >>> " + shamt;
				c = "(0u + " + wrapC(a, 1) + ") >> " + shamt;
				return {
					prec: 1,
					c: c,
					js: j,
					fn: function (n, w) {
						var v = a.fn(n, w) | 0;
						return (v >>> shamt) | 0;
					},
					cost: a.cost + opCost,
				};
			}
		}
		function mul(a, b, opCost) {
			var c = wrapC(a, 3) + " * " + wrapC(b, 3);
			var j = "Math.imul(" + a.js + ", " + b.js + ")";
			return {
				prec: 3,
				c: c,
				js: j,
				fn: function (n, w) {
					return Math.imul(a.fn(n, w), b.fn(n, w));
				},
				cost: a.cost + b.cost + opCost,
			};
		}
		var OPS = [
			[
				16,
				1,
				1,
				function () {
					return leaf_n();
				},
			],
			[
				8,
				1,
				1,
				function () {
					return leaf_const();
				},
			],
			[
				32,
				1,
				1,
				function () {
					return leaf_w_in();
				},
			],
			[
				48,
				2,
				2,
				function () {
					return leaf_w_out();
				},
			],
			[
				1,
				2,
				Infinity,
				function (c) {
					var a = rndExpr(c - 1, 1);
					var shv = (rnd.nextInt(31) + 1) | 0;
					return sh(a, "<<", shv, 1);
				},
			],
			[
				1,
				2,
				Infinity,
				function (c) {
					var a = rndExpr(c - 1, 1);
					var shv = (rnd.nextInt(31) + 1) | 0;
					return sh(a, ">>", shv, 1);
				},
			],
			[
				2,
				2,
				Infinity,
				function (c) {
					var b = rnd.nextInt(c - 1) + 1;
					var L = rndExpr(b, 2),
						R = rndExpr(c - b, 2);
					return bin(
						L,
						R,
						"+",
						2,
						function (x, y) {
							return (x + y) | 0;
						},
						2,
					);
				},
			],
			[
				2,
				2,
				Infinity,
				function (c) {
					var b = rnd.nextInt(c - 1) + 1;
					var L = rndExpr(b, 2),
						R = rndExpr(c - b, 3);
					return bin(
						L,
						R,
						"-",
						2,
						function (x, y) {
							return (x - y) | 0;
						},
						2,
					);
				},
			],
			[
				1,
				2,
				Infinity,
				function (c) {
					var b = rnd.nextInt(c - 1) + 1;
					var L = rndExpr(b, 0),
						R = rndExpr(c - b, 0);
					return bin(
						L,
						R,
						"^",
						0,
						function (x, y) {
							return (x ^ y) | 0;
						},
						1,
					);
				},
			],
			[
				4,
				2,
				Infinity,
				function (c) {
					var b = rnd.nextInt(c - 1) + 1;
					var L = rndExpr(b, 3),
						R = rndExpr(c - b, 3);
					return mul(L, R, 4);
				},
			],
		];
		var opFrom = allowLength ? 0 : 1;
		var opCount = OPS.length - (allowMultiplication ? 0 : 1) - (allowLength ? 0 : 1);
		function rndExpr(c, prec) {
			if (DEBUG) assert(c > 0, "c>0");
			var op;
			do {
				op = OPS[opFrom + rnd.nextInt(opCount)];
			} while (c < op[1] || c > op[2]);
			return op[3](c);
		}
		var root = rndExpr(complexity, 0);
		return {
			c: root.c,
			js: root.js,
			fn: function (n, w) {
				return root.fn(n, w) | 0;
			},
			cost: root.cost,
		};
	}

	var tried = [];
	var triedCounter = 0;
	var counter = 0;
	var collisions = [];

	this.search = function (complexity, iterations) {
		if (DEBUG) {
			assert(0 < complexity, "0 < complexity");
			assert(
				minTableSize | (0 === minTableSize) && (minTableSize & (minTableSize - 1)) === 0,
				"minTableSize | 0 === minTableSize && (minTableSize & (minTableSize - 1)) === 0",
			);
			assert(
				maxTableSize | (0 === maxTableSize) && (maxTableSize & (maxTableSize - 1)) === 0,
				"maxTableSize | 0 === maxTableSize && (maxTableSize & (maxTableSize - 1)) === 0",
			);
		}
		if (!(complexity in tried)) {
			tried[complexity] = Object.create(null);
		}

		var stringsCount = strings.length;
		var hashes = new Array(stringsCount);

		var rnd = prng;
		for (var i = 0; i < iterations; ++i) {
			var prngCopy = rnd.clone();
			var exprObj = generateRandomExpression(rnd, complexity, maxTableSize - 1, false, false);
			var expr = exprObj.js;
			var exprCost = exprObj.cost;

			// Count every attempted expression towards the global test budget,
			// even if we skip it due to being a duplicate at low complexity.
			// This keeps progress monotonic and avoids stalls when many
			// low-complexity duplicates are generated.
			++triedCounter;

			if (complexity > 4 || !(expr in tried[complexity])) {
				if (complexity <= 4) {
					tried[complexity][expr] = true;
				}
				var func;
				if (useEvalEngine) {
					try {
						func = eval("(function(n, w){return " + expr + ";})");
					} catch (_) {
						continue;
					}
				} else {
					func = exprObj.fn;
				}

				for (var j = 0; j < stringsCount; ++j) {
					hashes[j] = func(strings[j].length, stringChars[j]);
				}

				var tableSize = minTableSize;
				var found = false;
				while (!found && tableSize <= maxTableSize) {
					var j = 0;
					var hash;
					while (j < stringsCount && collisions[(hash = hashes[j] & (tableSize - 1))] !== counter) {
						collisions[hash] = counter;
						++j;
					}
					counter = (counter + 1) | 0;
					if (j === stringsCount) {
						found = true;
					} else {
						tableSize <<= 1;
					}
				}

				if (found) {
					var table = new Array(tableSize);
					for (var j = 0; j < tableSize; ++j) table[j] = -1;
					for (var j = 0; j < stringsCount; ++j) {
						var hash = func(strings[j].length, stringChars[j]) & (tableSize - 1);
						if (DEBUG) assert(table[hash] === -1, "table[hash] === -1");
						table[hash] = j;
					}
					var tableCost = 0;
					for (var t = tableSize; t > 1; t >>= 1) tableCost += 16;
					var result = {
						complexity: complexity,
						cost: exprCost + tableCost,
						prng: prngCopy,
						table: table,
						hashes: hashes,
					};
					if (evalTest) result.evalInfo = verifyEval(result);
					return result;
				}
			}
		}
		return null;
	};

	this.getTestedCount = function () {
		return triedCounter;
	};

	function buildExpression(foundSolution) {
		var exprObj = generateRandomExpression(foundSolution.prng.clone(), foundSolution.complexity, maxTableSize - 1, true, true);
		var mask = foundSolution.table.length - 1;
		return { exprObj: exprObj, mask: mask };
	}

	this.generateCExpression = function (foundSolution) {
		var r = buildExpression(foundSolution);
		return "(" + r.exprObj.c + ") & " + r.mask + "u";
	};

	this.generateJSExpression = function (foundSolution) {
		var r = buildExpression(foundSolution);
		return "(" + r.exprObj.js + ") & " + r.mask;
	};

	this.generateJSEvaluator = function (foundSolution) {
		var r = buildExpression(foundSolution);
		return function (n, w) {
			return r.exprObj.fn(n, w) & r.mask;
		};
	};

	this.generateCOutput = function (template, foundSolution) {
		var cExpression = this.generateCExpression(foundSolution);

		var replaceMap = {
			minLength: function () {
				return minLength;
			},
			maxLength: function () {
				return maxLength;
			},
			stringCount: function () {
				return strings.length;
			},
			stringList: function (pre) {
				return stringListToC(strings, 80, pre);
			},
			tableSize: function () {
				return foundSolution.table.length;
			},
			tableData: function (pre) {
				return numberListToC(foundSolution.table, 16, 0, pre);
			},
			hashExpression: function () {
				return cExpression;
			},
			stringDescription: function () {
				return zeroTerminated ? "zero terminated" : "zero termination not required";
			},
			seed: function () {
				return seed0;
			},
		};

		var output = "";
		var input = template;
		while (input !== "") {
			var index = input.indexOf("${");
			index = index < 0 ? input.length : index;
			output += input.slice(0, index);
			input = input.slice(index);
			if (input !== "") {
				var pre = output.slice(output.lastIndexOf("\n") + 1);
				index = input.indexOf("}");
				index = index < 0 ? input.length : index;
				output += replaceMap[input.slice(2, index)](pre);
				input = input.slice(index + 1);
			}
		}

		return output;
	};

	// Optional helper for shared schedulers (UI/CLI).
	this.getStringsLength = function () {
		return strings.length;
	};
}

function parseQuickHashGenInput(text) {
	var lines = text.split("\n");
	var strings = [];
	for (var i = 0; i < lines.length; ++i) {
		var s = lines[i].trim();
		if (s.length && (s[0] === '\"' || s[0] === "'")) {
			var o = 0;
			while (o < s.length) {
				var parsed = parseCString(s.slice(o));
				strings.push(parsed[0]);
				o += parsed[1];
				while (" \t\v\n\r".indexOf(s[o]) >= 0) ++o;
				if (s[o] === ",") ++o;
				while (" \t\v\n\r".indexOf(s[o]) >= 0) ++o;
				if (o < s.length && !(s[o] === '\"' || s[o] === "'")) {
					throw new Error("Invalid input");
				}
			}
		} else if (s.length) {
			strings.push(s);
		}
	}
	return strings;
}

// ===== Shared helpers for CLI and Web UI (non-breaking additions) =====

function computeTableBounds(strings) {
	var minSize = 1;
	while (strings.length > minSize) minSize <<= 1;
	var maxSize = minSize * 8;
	return { minSize: minSize, maxSize: maxSize };
}

function iterationsFor(stringsLength, base) {
	if (typeof base === "undefined") base = 200;
	if (!(base > 0)) base = 200;
	var v = Math.floor(base / Math.max(1, stringsLength));
	return v > 0 ? v : 1;
}

function makeCTemplate(options) {
	options = options || {};
	var zeroTerminated = !!options.zeroTerminated;
	var functionName = options.functionName || "lookup";
	var header = options.header || "/* Built with QuickHashGen */\n";
	var includeSeed = !!options.includeSeedComment;
	var includeAssert = !!options.includeAssert;
	var seedLine = includeSeed ? "// Seed: ${seed}\n" : "";
	if (zeroTerminated) {
		return (
			header +
			seedLine +
			"static int " +
			functionName +
			"(int n /* string length */, const char* s /* string (zero terminated) */) {\n" +
			"\tstatic const char* STRINGS[${stringCount}] = {\n" +
			"\t\t${stringList}\n" +
			"\t};\n" +
			"\tstatic const int HASH_TABLE[${tableSize}] = {\n" +
			"\t\t${tableData}\n" +
			"\t};\n" +
			"\tconst unsigned char* p = (const unsigned char*) s;\n" +
			(includeAssert ? "\tassert(s[n] == '\\0');\n" : "") +
			"\tif (n < ${minLength} || n > ${maxLength}) return -1;\n" +
			"\tint stringIndex = HASH_TABLE[${hashExpression}];\n" +
			"\treturn (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;\n" +
			"}\n"
		);
	} else {
		return (
			header +
			seedLine +
			"static int " +
			functionName +
			"(int n /* string length */, const char* s /* string (zero termination not required) */) {\n" +
			"\tstatic const char* STRINGS[${stringCount}] = {\n" +
			"\t\t${stringList}\n" +
			"\t};\n" +
			"\tstatic const int HASH_TABLE[${tableSize}] = {\n" +
			"\t\t${tableData}\n" +
			"\t};\n" +
			"\tconst unsigned char* p = (const unsigned char*) s;\n" +
			"\t// zero-termination not expected\n" +
			"\tif (n < ${minLength} || n > ${maxLength}) return -1;\n" +
			"\tint stringIndex = HASH_TABLE[${hashExpression}];\n" +
			"\treturn (stringIndex >= 0 && strncmp(s, STRINGS[stringIndex], n) == 0 && STRINGS[stringIndex][n] == 0) ? stringIndex : -1;\n" +
			"}\n"
		);
	}
}

function parseSeedComment(text) {
	var m = /\/\/\s*Seed:\s*(\d+)/.exec(String(text));
	if (!m) return undefined;
	var v = parseInt(m[1], 10);
	return Number.isFinite(v) ? v >>> 0 : undefined;
}

function formatSeedComment(seed) {
	return "// Seed: " + ((seed >>> 0) >>> 0);
}

function findInitializerRange(code, declStart) {
	var eq = code.indexOf("=", declStart);
	if (eq < 0) return null;
	var open = code.indexOf("{", eq);
	if (open < 0) return null;
	var depth = 1,
		k = open + 1;
	while (k < code.length && depth > 0) {
		var ch = code[k++];
		if (ch === "{") depth++;
		else if (ch === "}") depth--;
	}
	if (depth !== 0) return null;
	return { open: open, close: k - 1 };
}

function findMatchingSquare(code, openIndex) {
	if (openIndex < 0 || code[openIndex] !== "[") return -1;
	var depth = 0;
	for (var i = openIndex; i < code.length; ++i) {
		var ch = code[i];
		if (ch === "[") depth++;
		else if (ch === "]") {
			depth--;
			if (depth === 0) return i;
		}
	}
	return -1;
}

function scheduleStep(qh, best, rng, stringsLength, remainingTests) {
	var maxC = best === null ? 32 : best.complexity;
	var complexity = rng.nextInt(maxC) + 1;
	var iters = iterationsFor(stringsLength, 200);
	if (typeof remainingTests === "number") {
		iters = Math.min(iters, Math.max(1, remainingTests | 0));
	}
	return qh.search(complexity, iters);
}

// Update or generate C code using a found solution. If the provided code
// contains a STRINGS array, this patches the HASH_TABLE size and initializer,
// the hash expression inside HASH_TABLE[...], and refreshes the seed comment.
// If no STRINGS array is present, this generates a full snippet using the
// provided template options via makeCTemplate.
function updateCCodeWithSolution(code, qh, foundSolution, templateOptions) {
	code = String(code || "");
	if (code.indexOf("STRINGS") < 0) {
		// Generate complete output from template
		var tpl = makeCTemplate(
			templateOptions || {
				zeroTerminated: true,
				functionName: "lookup",
				header: "/* Built with QuickHashGen */\n",
				includeSeedComment: true,
				includeAssert: false,
			},
		);
		return qh.generateCOutput(tpl, foundSolution);
	}

	// 1) Update HASH_TABLE size literal if present
	var declStart = code.indexOf("static const int HASH_TABLE[");
	if (declStart < 0) declStart = code.indexOf("HASH_TABLE[");
	if (declStart >= 0) {
		var openIdx = code.indexOf("[", declStart) + 1;
		var closeIdx = code.indexOf("]", openIdx);
		if (openIdx > 0 && closeIdx > openIdx) {
			code = code.slice(0, openIdx) + String(foundSolution.table.length) + code.slice(closeIdx);
		}
	}

	// 2) Replace HASH_TABLE initializer body
	var rng = findInitializerRange(code, declStart);
	if (rng) {
		var header = code.slice(0, rng.open + 1);
		var footer = code.slice(rng.close);
		var tableBody = numberListToC(foundSolution.table, 16, 0, "\t\t");
		code = header + "\n\t\t" + tableBody + "\n\t" + footer;
	}

	// 3) Update the hash expression inside HASH_TABLE[...]
	var useStart = code.indexOf("int stringIndex");
	var startIdx = useStart >= 0 ? code.indexOf("HASH_TABLE[", useStart) : code.lastIndexOf("HASH_TABLE[");
	if (startIdx >= 0) {
		var bOpen = code.indexOf("[", startIdx);
		var bClose = findMatchingSquare(code, bOpen);
		if (bOpen >= 0 && bClose > bOpen) {
			var cExpr = qh.generateCOutput("${hashExpression}", foundSolution).trim();
			code = code.slice(0, bOpen + 1) + cExpr + code.slice(bClose);
		}
	}

	// 4) Update or insert seed comment after the built-with header
	var builtIdx = code.indexOf("/* Built with QuickHashGen");
	if (builtIdx >= 0) {
		var insertPos = code.indexOf("\n", builtIdx);
		insertPos = insertPos < 0 ? code.length : insertPos + 1;
		if (code.substr(insertPos, 8) === "// Seed:") {
			var lineEnd = code.indexOf("\n", insertPos);
			if (lineEnd < 0) lineEnd = code.length;
			code = code.slice(0, insertPos) + formatSeedComment(qh.getSeed()) + code.slice(lineEnd);
		} else {
			code = code.slice(0, insertPos) + formatSeedComment(qh.getSeed()) + "\n" + code.slice(insertPos);
		}
	}

	return code;
}

// Update STRINGS array length literal and the if-guard bounds in an existing C snippet.
// If `strings` is omitted or empty, this function tries to extract strings from `code`.
// Returns the updated code string (or the original if no changes applied).
function updateCCodeMetadata(code, strings) {
	code = String(code || "");
	if (code.indexOf("STRINGS") < 0) return code;

	// Determine string list
	var list = Array.isArray(strings) && strings.length ? strings : parseQuickHashGenInput(code);
	var count = list.length;
	var minLen = Infinity;
	var maxLen = 0;
	for (var i = 0; i < list.length; ++i) {
		var n = list[i].length;
		if (n < minLen) minLen = n;
		if (n > maxLen) maxLen = n;
	}
	if (!isFinite(minLen)) {
		minLen = 0;
		maxLen = 0;
	}

	// 1) Update STRINGS[...] size literal
	var idx = code.indexOf("STRINGS[");
	if (idx >= 0) {
		idx += 8;
		var close = code.indexOf("]", idx);
		if (close >= 0) code = code.slice(0, idx) + String(count) + code.slice(close);
	}

	// 2) Update the if-guard for n range: if (n < X || n > Y)
	var t0 = code.indexOf("if (n < ");
	if (t0 >= 0) {
		var aStart = t0 + 8;
		var aEnd = aStart;
		while (aEnd < code.length && code.charCodeAt(aEnd) >= 48 && code.charCodeAt(aEnd) <= 57) ++aEnd;
		var orIdx = code.indexOf("|| n > ", aEnd);
		if (orIdx >= 0) {
			var bStart = orIdx + 7;
			var bEnd = bStart;
			while (bEnd < code.length && code.charCodeAt(bEnd) >= 48 && code.charCodeAt(bEnd) <= 57) ++bEnd;
			code = code.slice(0, aStart) + String(minLen) + code.slice(aEnd, bStart) + String(maxLen) + code.slice(bEnd);
		}
	}

	return code;
}

if (typeof module !== "undefined") {
	module.exports = {
		assert: assert,
		XorshiftPRNG2x32: XorshiftPRNG2x32,
		toHex: toHex,
		parseCString: parseCString,
		escapeCString: escapeCString,
		stringListToC: stringListToC,
		numberListToC: numberListToC,
		parseQuickHashGenInput: parseQuickHashGenInput,
		QuickHashGen: QuickHashGen,
		// Shared helpers
		computeTableBounds: computeTableBounds,
		iterationsFor: iterationsFor,
		makeCTemplate: makeCTemplate,
		parseSeedComment: parseSeedComment,
		formatSeedComment: formatSeedComment,
		findInitializerRange: findInitializerRange,
		findMatchingSquare: findMatchingSquare,
		scheduleStep: scheduleStep,
		updateCCodeWithSolution: updateCCodeWithSolution,
		updateCCodeMetadata: updateCCodeMetadata,
	};
}
