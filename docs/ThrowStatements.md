# Throwing Sites

This document summarizes strings that can be thrown by the IVG project and explains
why they occur.

## IMPD

### Syntax errors

- `Duplicate label: <label>`: a label appears more than once.
- `Missing indexed argument <n>`: argument at position `<n>` was not provided.
- `Missing argument: <label>`: required argument with name `<label>` is missing.
- `Missing argument(s)`: no arguments were fetched.
- `Unrecognized labels or too many arguments`: extra or unknown arguments remain.
- `Missing */`, `Missing ]`, `Missing }`, `Missing "`: parser reached end of input without a closing token.
- `Label cannot be empty`: a label string had zero length.
- `Syntax error`: generic parsing failure.
- `Expected :`, `Expected =`, `Missing )`: required delimiter not found.
- `Unexpected end`: input ended while parsing an expression.

### Runtime errors

- `Math error (log of 0 or less)`, `Math error (log10 of 0 or less)`,
	`Math error (sqrt of negative)`: invalid arguments to math functions.
- `Division by zero`, `Modulo by zero`: attempt to divide by zero.
- `Number overflow`: result is not a finite number.
- `Invalid integer: <value>` or `Invalid number: <value>`: numeric text could not be parsed.
- `Invalid boolean (should be 'yes' or 'no'): <value>`: unsupported boolean text.
- `Variable <name> does not exist` or `Could not set variable <name>`: variable lookup or assignment failed.
- `Recursion limit reached`, `Statements limit reached`: interpreter safety limits exceeded.
- `Aborted`: executor aborted progress.
- `Cannot return in global frame`: return used outside a function context.
- `Unsupported data format`: a `format` instruction requested an unknown data format.
- `Could not include file: <path>`: include instruction failed to load a file.

## IVG

- `Duplicate font definition: <name>`, `Duplicate image definition: <name>`,
	`Duplicate path definition: <name>`: objects must be uniquely defined.
- `resolution out of range [0.0001..inf): <value>`: invalid resolution value.
- `Invalid define instruction type: <type>`: unknown type passed to a `define` instruction.
- `Unrecognized alignment: <value>` or duplicate alignment tokens: invalid alignment directives.
- `Negative clip width`, `Negative clip height`, `Negative rectangle width`,
	`Negative rectangle height`, `Negative rounded corner radius`,
	`Negative ellipse radius`, `Negative star radius`: geometry values must be non-negative.
- `star points out of range [1..10000]: <value>`: star shape uses an invalid number of points.
- `Need to set font before writing`, `Missing font: <name>`: text operations without a valid font.
- `Unrecognized fill rule: <value>` or `Undefined path: <name>`: painting references unknown values.
- `Relative paint is not allowed with wipe`: invalid combination of paint options.
- `aa-gamma out of range (0..100): <value>`,
	`curve-quality out of range (0..100): <value>`,
	`pattern-resolution out of range (0..100): <value>`: tuning parameters outside their allowed ranges.
- `Undeclared bounds`, `Multiple bounds declarations`,
	`bounds width out of range [1..32767]: <value>`,
	`bounds height out of range [1..32767]: <value>`: invalid canvas bounds.
- Font definition errors: `Duplicate metrics instruction`, `Invalid metrics instruction`,
	`Invalid glyph character (length is not 1)`, `Missing metrics before glyph instruction`,
	`Negative glyph advance`, `Duplicate glyph definition`, `Duplicate kerning pair`.

## NuXPixels

The low-level helpers in `externals/NuX` use `throw()` exception specifications but do not
produce descriptive messages. Callers must avoid invalid parameters such as division by
zero to prevent undefined behavior.
