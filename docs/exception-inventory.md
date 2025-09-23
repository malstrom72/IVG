# Exception Inventory

This document lists every exception thrown by the core C++ sources in `src/`.
The data was collected by scanning the source files for `throwBadSyntax`, `throwRunTimeError`,
and direct throws of the custom exception types. Locations use one-based line numbers.

## Custom exception types

* `IMPD::SyntaxException` – raised through `Interpreter::throwBadSyntax` when parsing detects invalid syntax.

* `IMPD::RunTimeException` – raised through `Interpreter::throwRunTimeError` when evaluation hits run-time constraints or invalid values.

* `IMPD::AbortedException` – indicates execution was stopped intentionally, such as due to a STOP instruction or executor abort.

* `IMPD::FormatException` – signals unsupported or malformed `format` directives.



## Exception message style analysis

The captured message texts fall into three broad styles:

1. **Raw literals** – simple "Message" strings that match the diagnostic the user ultimately sees.
2. **Concatenated expressions** – `String("Message: ") + value` constructs, sometimes chaining multiple additions.
3. **Helper rethrows** – partial expressions such as `const String& how) { throw SyntaxException(how` that delegate to helper overloads without providing their own literal.

Across these styles we saw several inconsistencies:

* **Capitalisation:** Most literals start with an upper-case letter, yet a handful (`"line-angle requires…"`, `"path instruction limit exceeded"`, etc.) remain lower-case.
* **Delimiter usage:** Some diagnostics append a colon and space before dynamic data (for example, `"Missing font: " + …`), others omit the space (`"Duplicate glyph definition …"`), and some avoid punctuation entirely (`"Invalid instruction"`).
* **Quoting and grammar:** Several messages quote argument names with single quotes while similar cases skip quoting, and certain sentences end abruptly (`"Missing }"`, `"Math error"`) compared with more descriptive peers such as `"Math error (sqrt of negative)"`.
* **Expression form:** Mixing literal strings with concatenated `String(...)` expressions hampers localisation and consistency checks, and the helper overload entries pollute the inventory with non-user-facing snippets.

### Instruction case conventions and message tone

The [IVG user guide](IVG%20Documentation.md#case-conventions) spells out the convention that drawing instructions appear in uppercase while configuration directives stay lowercase. Several diagnostics intentionally follow that rule so the user sees exactly the token they typed—`"LINE requires an even number of coordinates"` mirrors the uppercase drawing command, whereas `"line-angle requires an even number of values"` keeps the documented lowercase compound.

That alignment helps experienced authors map an error to the relevant section of the guide quickly, but it also means the message casing alternates between sentence-case and full uppercase and can feel visually harsh. To keep the text pleasant while still preserving the canonical token, we will present instruction names as inline code (`` `LINE` ``). This keeps the reference accurate, softens the shouting effect, and makes it clear which part is a literal keyword versus surrounding prose. With that decision made, the remaining work focuses on introducing the style consistently across the codebase while keeping punctuation and structure uniform.

### Inline-code messaging rollout plan

1. **Lock in the guideline.** Update the style references (`docs/IVG Documentation.md` and the developer coding standards) to define the sentence-case + inline-code pattern, including an example such as `"Line instruction requires an even number of coordinates for \\`LINE\\`"`.
2. **Inventory affected throws.** Use this table plus a quick search for `requires`/`instruction` in `src/IVG.cpp` and `src/IMPD.cpp` to flag every diagnostic that currently embeds an upper-case token directly in the literal.
3. **Rewrite literals in place.** For each affected throw site:
	* convert the surrounding sentence to sentence-case prose,
	* wrap the instruction token (and similar directive keywords) in backticks inside the string literal, and
	* normalise punctuation to "…: " before appended values.
Concatenated `String(...)` expressions should be simplified at the same time so the literal prefix carries the inline code example before any dynamic data.
4. **Clean up helper overloads.** Replace non-literal rethrows such as `const String& how) { throw SyntaxException(how` with direct calls to the new inline-coded messages or adjust the extraction script so only user-facing literals remain in the inventory.
5. **Automate enforcement.** Extend the extraction script (or add a linter) to flag new diagnostics that include bare all-caps tokens, lack inline-code quoting, or omit the standard punctuation so future additions follow the agreed format without manual review.



## Interpreter::throwBadSyntax

The tables below quote each source expression in a block so unusual syntax
or concatenation remains visible even when the snippet contains table
delimiters or other Markdown-sensitive characters.

| Message expression | Suggested new exception text | Locations |
| --- | --- | --- |
| > `"'while:' condition has to be enclosed in [ ]"` | The `while:` condition must be enclosed in `[]`. | `src/IMPD.cpp:L1327` |
| > `"Duplicate metrics instruction in font definition"` | Duplicate `metrics` instruction in the font definition. | `src/IVG.cpp:L2262` |
| > `"Expected :"` | Expected `:` delimiter. | `src/IMPD.cpp:L921` |
| > `"Expected ="` | Expected `=` after the name. | `src/IMPD.cpp:L1294` |
| > `"Invalid character escape code inside { } expression"` | Invalid character escape in `{}` expression. | `src/IMPD.cpp:L1044` |
| > `"Invalid instruction"` | Invalid instruction keyword. | `src/IMPD.cpp:L588` |
| > `"Invalid metrics instruction in font definition"` | Invalid `metrics` instruction in the font definition. | `src/IVG.cpp:L2275` |
| > `"Invalid variable name"` | Invalid variable name. | `src/IMPD.cpp:L1289` |
| > `"LINE requires an even number of coordinates"` | The `LINE` instruction requires an even number of coordinates. | `src/IVG.cpp:L1613` |
| > `"Label cannot be empty"` | Label cannot be empty. | `src/IMPD.cpp:L482` |
| > `"Missing )"` | Missing `)`. | `src/IMPD.cpp:L1034` |
| > `"Missing */"` | Missing `*/` terminator. | `src/IMPD.cpp:L372` |
| > `"Missing \""` | Missing `"` quote. | `src/IMPD.cpp:L449` |
| > `"Missing argument(s)"` | Missing argument(s). | `src/IMPD.cpp:L218`<br>`src/IMPD.cpp:L1373` |
| > `"Missing metrics before glyph instruction in font definition"` | Missing `metrics` block before the glyph instruction in the font definition. | `src/IVG.cpp:L2292` |
| > `"Missing variable name"` | Missing variable name. | `src/IMPD.cpp:L1287` |
| > `"Missing }"` | Missing `}`. | `src/IMPD.cpp:L951` |
| > `"Syntax error"` | Syntax error. | `src/IMPD.cpp:L493`<br>`src/IMPD.cpp:L781`<br>`src/IMPD.cpp:L843`<br>`src/IMPD.cpp:L879`<br>`src/IMPD.cpp:L881`<br>`src/IMPD.cpp:L945`<br>`src/IMPD.cpp:L948`<br>`src/IMPD.cpp:L1196`<br>`src/IMPD.cpp:L1202` |
| > `"Unexpected end"` | Unexpected end of input. | `src/IMPD.cpp:L1007` |
| > `"Unrecognized labels or too many arguments"` | Unrecognized labels or too many arguments. | `src/IMPD.cpp:L222` |
| > `"bezier-to requires 4 or 6 numbers"` | The `bezier-to` instruction requires 4 or 6 numbers. | `src/IVG.cpp:L734` |
| > `"line-angle requires an even number of values"` | The `line-angle` instruction requires an even number of values. | `src/IVG.cpp:L713` |
| > `"line-to requires an even number of coordinates"` | The `line-to` instruction requires an even number of coordinates. | `src/IVG.cpp:L701` |
| > `"move-angle requires an even number of values"` | The `move-angle` instruction requires an even number of values. | `src/IVG.cpp:L685` |
| > `String("Duplicate glyph definition in font definition (unicode: ") + impd.toString(static_cast<int>(glyph.character)) + ")"` | Duplicate glyph definition for code point `{glyph}` in the font definition. | `src/IVG.cpp:L2299` |
| > `String("Duplicate horizontal alignment: " + *s)` | Duplicate horizontal alignment `{alignment}`. | `src/IVG.cpp:L1452` |
| > `String("Duplicate kerning pair in font definition: ") + impd.toString(static_cast<int>(*itA)) + "," + impd.toString(static_cast<int>(*itB))` | Duplicate kerning pair `{first}`, `{second}` in the font definition. | `src/IVG.cpp:L2315` |
| > `String("Duplicate label: ") + kv.first` | Duplicate label `{label}`. | `src/IMPD.cpp:L162` |
| > `String("Duplicate vertical alignment: " + *s)` | Duplicate vertical alignment `{alignment}`. | `src/IVG.cpp:L1459` |
| > `String("Ellipse sweep requires IVG-3")` | The ellipse `sweep` option requires IVG-3 format. | `src/IVG.cpp:L1670` |
| > `String("Instruction requires ") + requiredString + ": " + instruction + (!arguments.empty() ? String(" ") + arguments : String())` | The `{instruction}` instruction requires {requiredString}: `{instruction}`{arguments}. | `src/IVG.cpp:L1286` |
| > `String("Invalid color name: ") + String(r.b, r.e)` | Invalid color name `{name}`. | `src/IVG.cpp:L503` |
| > `String("Invalid color: ") + String(r.b + 1, r.e)` | Invalid color value `{value}`. | `src/IVG.cpp:L482` |
| > `String("Invalid define instruction type: ") + type` | Invalid `define` instruction type `{type}`. | `src/IVG.cpp:L1355` |
| > `String("Invalid glyph character (length is not 1): ") + String(ws.begin(), ws.end())` | Invalid glyph character `{glyph}` (length must be 1). | `src/IVG.cpp:L2284` |
| > `String("Invalid opacity: ") + String(r.b + 1, r.e)` | Invalid opacity `{value}`. | `src/IVG.cpp:L132` |
| > `String("Invalid pre-multiplied alpha color: ") + String(r.b + 1, r.e)` | Invalid pre-multiplied alpha color `{value}`. | `src/IVG.cpp:L486` |
| > `String("Invalid stops for gradient (invalid position: ") + impd.toString(position) + ")"` | Invalid gradient stop position `{position}`. | `src/IVG.cpp:L948` |
| > `String("Invalid stops for gradient (odd number of elements): ") + *s` | Invalid gradient stop list `{stops}` (odd number of elements). | `src/IVG.cpp:L939` |
| > `String("Invalid turn for arc-to: ") + *turn` | Invalid `arc-to` turn `{turn}`. | `src/IVG.cpp:L753` |
| > `String("Missing argument: ") + label` | Missing argument `{label}`. | `src/IMPD.cpp:L213` |
| > `String("Missing indexed argument ") + Interpreter::toString(index + 1)` | Missing indexed argument #{index}. | `src/IMPD.cpp:L200` |
| > `String("Negative glyph advance in font definition: ") + impd.toString(glyph.advance)` | Negative glyph advance `{advance}` in the font definition. | `src/IVG.cpp:L2295` |
| > `String("Too few list elements (got " + toString(lossless_cast<int>(elements.size())) + ", expected at least ") + toString(minElements) + ")"` | Too few list elements (got {actual}, expected at least {minimum}). | `src/IMPD.cpp:L554` |
| > `String("Too many list elements (got " + toString(lossless_cast<int>(elements.size())) + ", expected at most ") + toString(maxElements) + ")"` | Too many list elements (got {actual}, expected at most {maximum}). | `src/IMPD.cpp:L550` |
| > `String("Unrecognized alignment: ") + alignment` | Unrecognized alignment `{alignment}`. | `src/IVG.cpp:L1449` |
| > `String("Unrecognized anchor: ") + *s` | Unrecognized anchor `{anchor}`. | `src/IVG.cpp:L1734` |
| > `String("Unrecognized ellipse type: ") + *typeArg` | Unrecognized ellipse type `{type}`. | `src/IVG.cpp:L1678` |
| > `String("Unrecognized fill rule: ") + *s` | Unrecognized fill rule `{fillRule}`. | `src/IVG.cpp:L1799` |
| > `String("Unrecognized gradient type: ") + gradientType` | Unrecognized gradient type `{type}`. | `src/IVG.cpp:L921` |
| > `String("Unrecognized instruction: ") + instructionString` | Unrecognized instruction `{instruction}`. | `src/IMPD.cpp:L1253` |
| > `String("Unrecognized stroke caps: ") + *s` | Unrecognized stroke caps `{caps}`. | `src/IVG.cpp:L1182` |
| > `String("Unrecognized stroke joints: ") + *s` | Unrecognized stroke joints `{joints}`. | `src/IVG.cpp:L1189` |
| > `c == '[' ? "Missing ]" : "Missing }"` | Missing closing `]` or `}`. | `src/IMPD.cpp:L434` |
| > `const String& how) { throw SyntaxException(how` | Propagate the caller-provided syntax message. | `src/IMPD.cpp:L355` |
| > `const char* how) { throwBadSyntax(String(how)` | Convert the caller-provided syntax message to a `String`. | `src/IMPD.cpp:L357` |
| > `errorString` | Forward the collected parser error text. | `src/IVG.cpp:L1389` |

## Interpreter::throwRunTimeError

| Message expression | Suggested new exception text | Locations |
| --- | --- | --- |
| > `"Bounds cannot be declared for mask"` | Cannot declare `bounds` for a mask. | `src/IVG.cpp:L385` |
| > `"Cannot return in global frame"` | Cannot return from the global frame. | `src/IMPD.cpp:L1299` |
| > `"Division by zero"` | Division by zero. | `src/IMPD.cpp:L789` |
| > `"Image coordinates out of range"` | Image coordinates out of range. | `src/IVG.cpp:L1419` |
| > `"Image scale out of range"` | Image scale out of range. | `src/IVG.cpp:L1580` |
| > `"Invalid first path instruction: " + instruction` | Invalid first path instruction `{instruction}`. | `src/IVG.cpp:L1810` |
| > `"Invalid font name"` | Invalid font name. | `src/IVG.cpp:L1983` |
| > `"Math error (log of 0 or less)"` | Math error: `log` requires a value greater than 0. | `src/IMPD.cpp:L240` |
| > `"Math error (log10 of 0 or less)"` | Math error: `log10` requires a value greater than 0. | `src/IMPD.cpp:L247` |
| > `"Math error (sqrt of negative)"` | Math error: `sqrt` requires a non-negative value. | `src/IMPD.cpp:L254` |
| > `"Math error"` | Math error. | `src/IMPD.cpp:L790`<br>`src/IMPD.cpp:L1077` |
| > `"Modulo by zero"` | Modulo by zero. | `src/IMPD.cpp:L813` |
| > `"Multiple bounds declarations"` | Multiple `bounds` declarations. | `src/IVG.cpp:L2105` |
| > `"Need to set font before writing"` | Writing text requires a font to be set first. | `src/IVG.cpp:L1752` |
| > `"Number overflow"` | Number overflow. | `src/IMPD.cpp:L793`<br>`src/IMPD.cpp:L863`<br>`src/IMPD.cpp:L868`<br>`src/IMPD.cpp:L1057`<br>`src/IMPD.cpp:L1078` |
| > `"Recursion limit reached"` | Recursion limit reached. | `src/IMPD.cpp:L597` |
| > `"Relative paint is not allowed with wipe"` | Relative paint is not allowed with the `wipe` instruction. | `src/IVG.cpp:L1843` |
| > `"Statements limit reached"` | Statements limit reached. | `src/IMPD.cpp:L608` |
| > `"Undeclared bounds"` | Undeclared `bounds` definition. | `src/IVG.cpp:L984`<br>`src/IVG.cpp:L2096` |
| > `"Vertices outside valid coordinate range"` | Vertices fall outside the valid coordinate range. | `src/IVG.cpp:L1082`<br>`src/IVG.cpp:L1098` |
| > `"path instruction limit exceeded"` | Path instruction limit exceeded. | `src/IVG.cpp:L183`<br>`src/IVG.cpp:L876`<br>`src/IVG.cpp:L1071`<br>`src/IVG.cpp:L1075`<br>`src/IVG.cpp:L2209` |
| > `String("Could not include file: ") + String(file.begin(), file.end())` | Could not include file `{file}`. | `src/IMPD.cpp:L1385` |
| > `String("Could not set variable ") + name` | Could not set variable `{name}`. | `src/IMPD.cpp:L503` |
| > `String("Duplicate font definition: ") + String(name.begin(), name.end())` | Duplicate font definition `{name}`. | `src/IVG.cpp:L1300` |
| > `String("Duplicate image definition: ") + String(name.begin(), name.end())` | Duplicate image definition `{name}`. | `src/IVG.cpp:L1319` |
| > `String("Duplicate path definition: ") + String(name.begin(), name.end())` | Duplicate path definition `{name}`. | `src/IVG.cpp:L1335` |
| > `String("Duplicate pattern definition: ") + String(name.begin(), name.end())` | Duplicate pattern definition `{name}`. | `src/IVG.cpp:L1348` |
| > `String("Invalid boolean (should be 'yes' or 'no'): ") + s` | Invalid boolean `{value}` (use `yes` or `no`). | `src/IMPD.cpp:L758` |
| > `String("Invalid image height: ") + impd.toString(fitHeight)` | Invalid image height `{height}`. | `src/IVG.cpp:L1477` |
| > `String("Invalid image width: ") + impd.toString(fitWidth)` | Invalid image width `{width}`. | `src/IVG.cpp:L1470` |
| > `String("Invalid integer: ") + String(r.b, r.e)` | Invalid integer `{value}`. | `src/IMPD.cpp:L740` |
| > `String("Invalid number: ") + String(r.b, r.e)` | Invalid number `{value}`. | `src/IMPD.cpp:L748` |
| > `String("Missing font: ") + String(newFontName.begin(), newFontName.end())` | Missing font `{name}`. | `src/IVG.cpp:L1986` |
| > `String("Missing font: ") + String(state.textStyle.fontName.begin(), state.textStyle.fontName.end())` | Missing font `{name}`. | `src/IVG.cpp:L1756` |
| > `String("Missing image: ") + String(imageName.begin(), imageName.end())` | Missing image `{name}`. | `src/IVG.cpp:L1523` |
| > `String("Negative clip height: ") + impd.toString(numbers[2])` | Negative clip height `{height}`. | `src/IVG.cpp:L1498` |
| > `String("Negative clip width: ") + impd.toString(numbers[2])` | Negative clip width `{width}`. | `src/IVG.cpp:L1495` |
| > `String("Negative dash value: ") + impd.toString(dash)` | Negative dash value `{dash}`. | `src/IVG.cpp:L1205` |
| > `String("Negative ellipse radius: ") + impd.toString(rx < 0.0 ? rx : ry)` | Negative ellipse radius `{radius}`. | `src/IVG.cpp:L1657` |
| > `String("Negative gap value: ") + impd.toString(gap)` | Negative gap value `{gap}`. | `src/IVG.cpp:L1209` |
| > `String("Negative radial gradient radius: ") + impd.toString(coords[coords[2] < 0.0 ? 2 : 3])` | Negative radial gradient radius `{radius}`. | `src/IVG.cpp:L930` |
| > `String("Negative rectangle height: ") + impd.toString(numbers[3])` | Negative rectangle height `{height}`. | `src/IVG.cpp:L1631` |
| > `String("Negative rectangle width: ") + impd.toString(numbers[2])` | Negative rectangle width `{width}`. | `src/IVG.cpp:L1628` |
| > `String("Negative rounded corner radius: ") + impd.toString(rounded[0] < 0.0 ? rounded[0] : rounded[1])` | Negative rounded corner radius `{radius}`. | `src/IVG.cpp:L1640` |
| > `String("Negative star radius: ") + impd.toString(r1 < 0.0 ? r1 : r2)` | Negative star radius `{radius}`. | `src/IVG.cpp:L1714` |
| > `String("Negative stroke width: ") + impd.toString(d)` | Negative stroke width `{width}`. | `src/IVG.cpp:L1174` |
| > `String("Number overflow: ") + String(r.b, r.e)` | Number overflow `{value}`. | `src/IMPD.cpp:L751` |
| > `String("Undefined path: ") + String(name.begin(), name.end())` | Undefined path `{name}`. | `src/IVG.cpp:L1398` |
| > `String("Undefined pattern: ") + String(name.begin(), name.end())` | Undefined pattern `{name}`. | `src/IVG.cpp:L1014` |
| > `String("Variable ") + name + " does not exist"` | Variable `{name}` does not exist. | `src/IMPD.cpp:L514` |
| > `String("Variable ") + varName + " already declared"` | Variable `{name}` is already declared. | `src/IMPD.cpp:L1301` |
| > `String("aa-gamma out of range (0..100): ") + impd.toString(d)` | `aa-gamma` value `{value}` out of range (0..100). | `src/IVG.cpp:L1859` |
| > `String("bounds height out of range [1..32767]: ") + Interpreter::toString(bounds.height)` | `bounds` height `{height}` out of range [1..32767]. | `src/IVG.cpp:L66` |
| > `String("bounds left out of range [-32768..32767]: ") + Interpreter::toString(bounds.left)` | `bounds` left `{left}` out of range [-32768..32767]. | `src/IVG.cpp:L54` |
| > `String("bounds top out of range [-32768..32767]: ") + Interpreter::toString(bounds.top)` | `bounds` top `{top}` out of range [-32768..32767]. | `src/IVG.cpp:L58` |
| > `String("bounds width out of range [1..32767]: ") + Interpreter::toString(bounds.width)` | `bounds` width `{width}` out of range [1..32767]. | `src/IVG.cpp:L62` |
| > `String("curve-quality out of range (0..100): ") + impd.toString(d)` | `curve-quality` value `{value}` out of range (0..100). | `src/IVG.cpp:L1866` |
| > `String("ellipse aspect ratio out of range: ") + Interpreter::toString(aspectRatio)` | Ellipse aspect ratio `{ratio}` out of range. | `src/IVG.cpp:L168` |
| > `String("font size out of range (0..1000000]: ") + impd.toString(d)` | Font size `{size}` out of range (0..1000000]. | `src/IVG.cpp:L2005` |
| > `String("hsv value number ") + impd.toString(i + 1) + " out of range [0..1]: " + impd.toString(n[i])` | HSV value #{index} `{value}` out of range [0..1]. | `src/IVG.cpp:L457` |
| > `String("miter-limit out of range [1..inf): ") + impd.toString(d)` | `miter-limit` value `{value}` out of range [1..∞). | `src/IVG.cpp:L1193` |
| > `String("opacity out of range [0..1]: ") + impd.toString(d)` | Opacity `{value}` out of range [0..1]. | `src/IVG.cpp:L135` |
| > `String("pattern-resolution out of range (0..100): ") + impd.toString(d)` | `pattern-resolution` value `{value}` out of range (0..100). | `src/IVG.cpp:L1873` |
| > `String("resolution out of range [0.0001..inf): ") + impd.toString(resolution)` | Resolution `{value}` out of range [0.0001..∞). | `src/IVG.cpp:L1314` |
| > `String("star points out of range [1..10000]: ") + impd.toString(points)` | Star points `{count}` out of range [1..10000]. | `src/IVG.cpp:L1711` |
| > `const String& how) { throw RunTimeException(how` | Propagate the caller-provided runtime message. | `src/IMPD.cpp:L356` |
| > `const char* how) { throwRunTimeError(String(how)` | Convert the caller-provided runtime message to a `String`. | `src/IMPD.cpp:L358` |

## IMPD::AbortedException

| Message expression | Suggested new exception text | Locations |
| --- | --- | --- |
| > `"Aborted"` | Execution aborted. | `src/IMPD.cpp:L609` |
| > `"Encountered STOP instruction"` | Encountered `STOP` instruction. | `src/IMPD.cpp:L1262` |

## IMPD::FormatException

| Message expression | Suggested new exception text | Locations |
| --- | --- | --- |
| > `"Unsupported data format"` | Unsupported data format. | `src/IMPD.cpp:L1281` |

## Rethrow statements

The following sites rethrow the currently handled exception without changing its type or message.
* `src/IMPD.cpp:L620`
* `src/IMPD.cpp:L624`
* `src/IVG.cpp:L1230`
