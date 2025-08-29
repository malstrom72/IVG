# QuickHashGen

QuickHashGen generates tiny, collision-free hash functions and lookup tables for fixed sets of
strings. It is useful when you need a fast switch-on-strings in C/C++ or any environment where a
small perfect hash is required.

## Why QuickHashGen?

- Eliminates run-time hashing by precomputing a perfect hash.
- Produces compact lookup tables and simple hash expressions.
- Supports zero-terminated or non-zero-terminated strings.

## Command line usage

A Node-based CLI is provided:

```
node QuickHashGenCLI.node.js [options] [input-file]
```

Options:

- `-h`, `--help`: display usage information.

- `--tests N`: number of expressions to try (default `100000`). A larger value increases the search
  space and the odds of discovering a lower-cost hash at the cost of longer runtime.

- `--no-multiplications`: disallow multiplication instructions in the generated hash expression.
  Useful for targets where multiplies are expensive or unavailable.

- `--no-length`: prevent use of the string length variable `n` in the hash expression. This keeps
  the hash based strictly on character data so strings of differing lengths don't simply hash to
  their length. The generated lookup still receives `n` for bounds checking.

- `--no-zero-termination`: generate a lookup function that does not require the input strings to be
  zero-terminated. The resulting C template uses `strncmp` and expects the caller to supply the
  string length.

- `--eval-test`: after a candidate expression is found, evaluate it on all input strings using the
  selected engine to verify that it maps each string to the expected index (primarily for
  testing).

- `--force-eval`: use the `eval` engine instead of the default `Function` constructor when searching
  and testing (primarily for testing). Mirrors the HTML interface’s "Use eval engine" checkbox and
  can influence performance depending on the environment.

- `--bench`: run a simple benchmark comparing the `Function` constructor and `eval` engines after a
  solution is found.

- `--seed N`: seed all internal randomness with a single 32-bit value for fully deterministic
  output.


### Input formats

Strings are read from a file or standard input. Each line may be:

- plain text, representing a single string per line, or
- one or more C-style quoted strings separated by whitespace or commas.

Quoted strings support standard C escape sequences such as `\n` or `\xFF`, allowing spaces and
binary data.

Example generating C code:

```
printf "move\nattack\ndefend\nuse\nopen\nclose\ntake\ndrop\ntalk\ntrade\ncraft\nrepair\nequip\nunequip\ninventory\nquit\n" \
	| node QuickHashGenCLI.node.js --seed 1 > actions.c
```

The generated `lookup` function returns the index of a matching string or `-1` if the string is absent.

### Example: Adventure Actions (16)

As a concrete example, the 16 adventure actions can be hashed like so (using a fixed seed for reproducibility):

```bash
printf "move\nattack\ndefend\nuse\nopen\nclose\ntake\ndrop\ntalk\ntrade\ncraft\nrepair\nequip\nunequip\ninventory\nquit\n" \
	| node QuickHashGenCLI.node.js --seed 1 > actions.c
```

The resulting `actions.c` contains the generated lookup routine:

```c
/* Built with QuickHashGen CLI */
// Seed: 1
static int lookup(int n /* string length */, const char* s /* string (zero terminated) */) {
	static const char* STRINGS[16] = {
		"move", "attack", "defend", "use", "open", "close", "take", "drop", "talk",
		"trade", "craft", "repair", "equip", "unequip", "inventory", "quit"
	};
	static const int HASH_TABLE[32] = {
		-1, 15, -1, 2, 10, 14, -1, -1, -1, 12, -1, -1, -1, 11, 0, -1,
		7, 5, 3, 9, -1, 6, -1, -1, -1, 4, -1, 8, 1, -1, -1, 13
	};
	const unsigned char* p = (const unsigned char*) s;
	if (n < 3 || n > 9) return -1;
	int stringIndex = HASH_TABLE[(p[3] - (n - p[0])) & 31u];
	return (stringIndex >= 0 && strcmp(s, STRINGS[stringIndex]) == 0) ? stringIndex : -1;
}
```

A C++ program can then switch on the returned index. For the 16 adventure actions above, you can
dispatch actions like this:

```cpp
#include <string>
extern int lookup(int n, const char* s);

enum Command {
  MOVE, ATTACK, DEFEND, USE, OPEN, CLOSE, TAKE, DROP, TALK, TRADE, CRAFT, REPAIR, EQUIP, UNEQUIP, INVENTORY, QUIT
};

void handle_command(const std::string& cmd) {
	switch (static_cast<Command>(lookup((int)cmd.size(), cmd.c_str()))) {
		case MOVE:       do_move(); break;
		case ATTACK:     do_attack(); break;
		case DEFEND:     do_defend(); break;
		case USE:        do_use(); break;
		case OPEN:       do_open(); break;
		case CLOSE:      do_close(); break;
		case TAKE:       do_take(); break;
		case DROP:       do_drop(); break;
		case TALK:       do_talk(); break;
		case TRADE:      do_trade(); break;
		case CRAFT:      do_craft(); break;
		case REPAIR:     do_repair(); break;
		case EQUIP:      do_equip(); break;
		case UNEQUIP:    do_unequip(); break;
		case INVENTORY:  show_inventory(); break;
		case QUIT:       do_quit(); break;
		default:         unknown_cmd(); break;
	}
}
```

## Programmatic use

The `QuickHashGenCore.js` module exposes the algorithm for use within Node or the browser:

```javascript
const qh = require("./QuickHashGenCore");
const strings = ["red", "green", "blue"];

let minSize = 1;
while (strings.length > minSize) minSize <<= 1;
const maxSize = minSize * 8;

const gen = new qh.QuickHashGen(
	strings,
	minSize,
	maxSize,
	true /* zero-terminated */,
	true /* allow multiplications */,
	true /* allow length */,
	false /* use eval engine */,
	false /* eval self-test */,
);
const best = gen.search(10, 1000);
const cExpr = gen.generateCExpression(best);
console.log(cExpr);
```

### Shared Helpers

QuickHashGen exposes a few helper functions used by both the CLI and the web UI.
These make it easier to integrate the engine and keep behavior consistent:

- `makeCTemplate(options)`:
	- Builds a C template string for `generateCOutput`.
	- Options: `{ zeroTerminated, functionName, header?, includeAssert?, includeSeedComment? }`.

- `computeTableBounds(strings)`:
	- Returns `{ minSize, maxSize }` where `minSize` is the next power of two ≥ `strings.length` and
	  `maxSize = minSize * 8`.

- `iterationsFor(stringsLength, base=200)`:
	- Computes a per-iteration budget `Math.max(1, Math.floor(base / stringsLength))` for smoother
	  progress.

- `scheduleStep(qh, best, rng, stringsLength, remainingTests?)`:
	- Chooses a complexity and iteration budget, then performs one search step on a `QuickHashGen`
	  instance.
	- `rng` must provide `nextInt(max)`.

- Seed helpers:
	- `parseSeedComment(text)` parses `// Seed: N` from a C snippet.
	- `formatSeedComment(seed)` returns the normalized seed comment string.

- C code patching helpers:
	- `updateCCodeWithSolution(code, qh, found, templateOptions)` updates an existing C snippet
	  in-place (table size, initializer, hash expr, seed comment), or generates a new one if none
	  is present.
	- `findInitializerRange(code, declStart)` and `findMatchingSquare(code, openIndex)` are exposed
	  for custom patching.
	- `updateCCodeMetadata(code, strings?)` updates `STRINGS[...]` length and the `if (n < .. ||
	  n > ..)` guard based on a list of strings; if `strings` is omitted, it tries to extract
	  strings from the code.

These helpers are exported from `QuickHashGenCore.js` and are used internally by both the CLI and
the browser UI to keep outputs aligned.

### Core API

`QuickHashGenCore.js` exports a handful of helpers and the `QuickHashGen` class for programmatic
integration:

- **`QuickHashGen`**: the core search engine. The constructor accepts `(strings, minTableSize,
    maxTableSize, zeroTerminated, allowMultiplication, allowLength, useEvalEngine=false,
    evalTest=false, seed0?, seed1?)`.

	If `seed0` is omitted the PRNG is seeded randomly; providing `seed0`(and optionally `seed1`)
	yields deterministic output.

	Methods include:

	- `search(complexity, iterations)`: explore random expressions and return the first
	  collision-free solution or `null`.
	- `getTestedCount()`: total number of expressions evaluated so far.
	- `randomInt(max)`: draw a pseudo-random integer in `[0, max)`.
	- `generateCExpression(solution)`: build a C hash expression string.
	- `generateJSExpression(solution)`: build a JavaScript hash expression string.
	- `generateJSEvaluator(solution)`: build a CSP-safe evaluator function.
	- `generateCOutput(template, solution)`: populate a C template using a solution object returned
	  by `search` (internally uses `generateCExpression`).

- **`parseQuickHashGenInput(text)`**: parse newline or C-style quoted strings into an array,
    mirroring CLI input handling.

- **`stringListToC(strings, columns, prefix)`** and **`numberListToC(numbers, columns, base,
    prefix)`**: format arrays as C initializers.

- **`toHex(i, length)`**: format a number as a zero-padded hexadecimal string.

- **`parseCString`** / **`escapeCString`**: convert between C-style quoted strings and raw
    JavaScript strings.

- **`XorshiftPRNG2x32`**: deterministic pseudo-random number generator used during the search.

### Cost model

QuickHashGen tracks a simple runtime cost for each abstract syntax tree (AST) node. The total cost
combines these node costs with a penalty for the hash table size, and the search prefers solutions
with the lowest cost.

Base node costs reflect their relative expense:

- constant value: `8`
- length variable `n`: `16`
- character read within the known string length `p[i]`: `32`
- character read beyond the input length guard `p[i]`: `48`

Binary operators add the cost of their operands plus an operator cost:

- shift (`<<`/`>>>`): `+1`
- addition or subtraction: `+2`
- XOR: `+1`
- multiplication: `+4`

Hash table size also adds to the total cost: each power-of-two increase adds `16`. For example, a
table with `256` entries (`2^8`) contributes `128`.

When multiple expressions hash all strings without collisions, the generator chooses the one with
the lowest total cost, favoring cheaper operations like constants over more expensive character
lookups and large tables.

### Debug mode

The core library includes a lightweight assertion helper and a few quick self-tests that run only
when debug mode is enabled. To enable debugging during development, set the `NODE_ENV` environment
variable to `development` before executing any scripts. For example:

```
NODE_ENV=development node QuickHashGenCLI.node.js --help
```

With `NODE_ENV` unset or set to `production`, these assertions and tests are skipped.

## Fuzz loop (native)

The `tests/fuzzLoop.sh` script continuously fuzzes QuickHashGen by generating random string sets,
emitting C++ via `tests/QuickHashGenTest.node.js`, compiling with `g++`, and executing the
resulting binary. This helps surface edge cases and regressions.

- Requirements: Node.js and a C++ compiler (`g++`) available on PATH.

- Run (POSIX/macOS/Linux):
	```sh
	chmod +x tests/fuzzLoop.sh
	./tests/fuzzLoop.sh
	```

- Behavior: prints `seed <N>` for each iteration, builds to a temporary folder, runs the binary, and
  repeats with the next seed until you stop it (Ctrl-C). Temporary files are cleaned up
  automatically.

- Reproduce a failing case: take the printed seed `N` and run:
	```sh
	node tests/QuickHashGenTest.node.js N > /tmp/test.cpp
	g++ -o /tmp/test /tmp/test.cpp
	/tmp/test
	```

Note: `tests/QuickHashGenTest.node.js` seeds the generator deterministically from `N`, so the same
seed reproduces the same dataset and result.

## Browser interface

`QuickHashGen.html` offers an interactive front end. The single text area acts as both input and
output: type one string per line, paste a list of quoted strings, or drop in previously generated C
code. The app parses the contents, updates the `STRINGS` array and length guards, and regenerates
the hash when you press **Start**. Editing the text or toggling any option automatically pauses the
search so you can resume with new parameters.

Checkboxes mirror the CLI flags, letting you disallow multiplications, omit the length variable,
toggle zero-termination handling, and (when in debug mode) optionally run an evaluation check or
switch to the `eval` engine.

## License

Released under the [BSD 2-Clause License](LICENSE).
