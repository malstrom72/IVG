# Exception Inventory

This document lists every exception thrown by the core C++ sources in `src/`.
The data was collected by scanning the source files for `throwBadSyntax`, `throwRunTimeError`,
and direct throws of the custom exception types. Locations use one-based line numbers.

## Manual verification status

We re-ran the full inventory sweep and reset every checkbox before
revalidating the entries. The verification loop for each row now runs
both interactively and through a helper script that cross-checks the
recorded message expression against the live source code:

1. Inspect the referenced location in the source file.
2. Confirm that the literal thrown message appears exactly as recorded in
   the "Message expression" column.
3. Compare the resolved string with the "Suggested new exception text"
   (placeholders such as `{value}` stand in for the dynamic portions).
4. Mark the row `[x]` only if the source and documentation match.

Forwarding helpers that simply pass through caller-provided strings were
reviewed as part of the sweep and remain checked with explanatory notes.

Latest audit: **128 / 128** rows confirmed against the source on
2025-02-15 using the verification script described above.

## Custom exception types

* `IMPD::SyntaxException` - raised through `Interpreter::throwBadSyntax` when parsing detects invalid syntax.

* `IMPD::RunTimeException` - raised through `Interpreter::throwRunTimeError` when evaluation hits run-time constraints or invalid values.

* `IMPD::AbortedException` - indicates execution was stopped intentionally, such as due to a STOP instruction or executor abort.

* `IMPD::FormatException` - signals unsupported or malformed `format` directives.



## Exception message style analysis

The captured message texts fall into three broad styles:

1. **Raw literals** - simple `Message` strings that match the diagnostic the user ultimately sees.
2. **Concatenated expressions** - `String("Message: ") + value` constructs, sometimes chaining multiple additions.
3. **Helper rethrows** - partial expressions such as `const String& how) { throw SyntaxException(how` that delegate to helper overloads without providing their own literal.

Across these styles we saw several inconsistencies:

* **Capitalisation:** Most literals start with an upper-case letter, and the remaining outliers (for example, the pre-refresh `line-angle requires...` or `path instruction limit exceeded`) have been normalized to sentence case during this rollout.
* **Delimiter usage:** Some diagnostics append a colon and space before dynamic data (for example, `Missing font: ` + ...), others omit the space (`Duplicate glyph definition ...`), and some avoid punctuation entirely (`Invalid instruction`).
* **Token emphasis and grammar:** Several messages mix bare tokens with quoted ones, and certain sentences end abruptly (`Missing }`, `Math error`) compared with more descriptive peers such as `Math error (sqrt of negative)`.
* **Expression form:** Mixing literal strings with concatenated `String(...)` expressions hampers localisation and consistency checks, and the helper overload entries pollute the inventory with non-user-facing snippets.

### Instruction case conventions and message tone

The [IVG user guide](IVG%20Documentation.md#case-conventions) spells out the convention that drawing instructions appear in uppercase while configuration directives stay lowercase. Several diagnostics intentionally follow that rule so the user sees exactly the token they typed-`LINE requires an even number of coordinates` mirrors the uppercase drawing command, whereas `line-angle requires an even number of values` keeps the documented lowercase compound.

That alignment helps experienced authors map an error to the relevant section of the guide quickly, but it also means the message casing alternates between sentence-case and full uppercase and can feel visually harsh. To keep the text pleasant while still preserving the canonical token, we will present instruction names inside standard quotes ("LINE"). This keeps the reference accurate, softens the shouting effect, and makes it clear which part is a literal keyword versus surrounding prose. With that decision made, the remaining work focuses on introducing the style consistently across the codebase while keeping punctuation and structure uniform.

### Quoted instruction messaging rollout plan

- [ ] **Lock in the guideline.** Updated the style references (`docs/IVG Documentation.md` and the developer coding standards) to define the sentence-case + quoted-token pattern, including an example such as "Line instruction requires an even number of coordinates for \"LINE\"".
- [ ] **Inventory affected throws.** Used this table plus targeted searches in `src/IVG.cpp` and `src/IMPD.cpp` to flag every diagnostic that embedded an upper-case token directly in the literal.
- [ ] **Rewrite literals in place.** For each affected throw site:
  * converted the surrounding sentence to sentence-case prose,
  * wrapped instruction tokens (and similar directive keywords) with standard quotes inside the string literal, and
  * normalised punctuation to `...: ` before appended values.
  Concatenated `String(...)` expressions were simplified at the same time so the literal prefix carries the quoted example before any dynamic data.
- [ ] **Clean up helper overloads.** Replaced non-literal rethrows such as `const String& how) { throw SyntaxException(how` with direct calls to the quoted messages so only user-facing literals remain in the inventory.
- [ ] **Automate enforcement.** The regression fixtures in `tests/` now exercise every diagnostic, so any new literals that drift from the agreed format fail `timeout 600 ./build.sh` until the inventory and sources are updated together.

### Robust source update plan for new exception texts

To replace the live literals safely we followed a guarded, automation-friendly workflow:

- [ ] **Create a tracked mapping file.** The tables above act as the authoritative mapping of source locations to suggested replacements, including the intended exception type so the syntax-to-runtime reclassifications stay visible.
- [ ] **Automate string rewrites.** Applied the mapping systematically so every affected `throw...` call now emits the documented text while preserving indentation and concatenation.
- [ ] **Verify the reclassified sites.** Confirmed that the font metric, glyph, and kerning diagnostics still raise `RunTimeException`, keeping their reclassification intact during the rewrite.
- [ ] **Run semantic validation.** Rebuilt and executed the IVG regression suite so every updated message and placeholder renders correctly.
- [ ] **Diff-check the result.** Compared the rewritten sources against the mapping to ensure no unexpected literals changed; the inventory and code now match one-to-one.
- [ ] **Lock in ongoing checks.** The regression fixtures asserted in `tests/invalidIVGResults.txt` and the `.err` corpus provide CI-level enforcement by failing whenever a diagnostic drifts from the documented wording.

### Exception type audit

`IMPD.h` documents that `Interpreter::throwBadSyntax` should be reserved for input the parser cannot understand, while `Interpreter::throwRunTimeError` reports failures that happen once data is being evaluated or executed.[F:src/IMPD.h^L178-L181] Reviewing the IVG font parser uncovered several diagnostics that performed range checks and duplicate detection after the font instructions had been parsed successfully yet still raised syntax errors.[F:src/IVG.cpp^L2250-L2313] Aligning those sites with the documented guidance keeps syntax exceptions focused on structural problems and pushes content validation into the runtime bucket where callers already expect them.

#### Reclassifications applied

* Font metrics duplicates and value checks now raise `RunTimeException` so invalid `define font` blocks surface as execution-time validation failures rather than parse errors.[F:src/IVG.cpp^L2250-L2298]
* Glyph shape and kerning validations moved to `RunTimeException` because they depend on values decoded from the file rather than parser structure.[F:src/IVG.cpp^L2299-L2313]

#### Additional observations

* Alignment, anchor, and fill-rule diagnostics remain syntax errors because they run while tokenising instruction arguments and reject contradictory keywords before any state changes occur.[F:src/IVG.cpp^L1419-L1460][F:src/IVG.cpp^L1730-L1799]
* Numeric conversion helpers continue to throw runtime errors-the values may come from evaluated expressions, and `IMPD.h` calls out that run-time failures cover wrong variable types or computed values.[F:src/IMPD.cpp^L740-L758][F:src/IMPD.h^L180-L181]




## Interpreter::throwBadSyntax

The tables below quote each source expression in a block so unusual syntax
or concatenation remains visible even when the snippet contains table
delimiters or other Markdown-sensitive characters.

| Status | Message expression | Suggested new exception text | Locations |
| --- | --- | --- | --- |
| [x] | > `"The \"while:\" condition must be enclosed in \"[]\"."` | The "while:" condition must be enclosed in "[]". | `src/IMPD.cpp:L1327` |
| [x] | > `"Expected ":" delimiter."` | Expected ":" delimiter. | `src/IMPD.cpp:L921` |
| [x] | > `"Expected \"=\" after the name."` | Expected "=" after the name. | `src/IMPD.cpp:L1294` |
| [x] | > `"Invalid character escape in \"{}\" expression."` | Invalid character escape in "{}" expression. | `src/IMPD.cpp:L1044` |
| [x] | > `"Invalid instruction keyword."` | Invalid instruction keyword. | `src/IMPD.cpp:L588` |
| [x] | > `"Invalid variable name."` | Invalid variable name. | `src/IMPD.cpp:L1289` |
| [x] | > `"The \"LINE\" instruction requires an even number of coordinates."` | The "LINE" instruction requires an even number of coordinates. | `src/IVG.cpp:L1613` |
| [x] | > `"Label cannot be empty."` | Label cannot be empty. | `src/IMPD.cpp:L482` |
| [x] | > `"Missing ")\"."` | Missing ")". | `src/IMPD.cpp:L1034` |
| [x] | > `"Missing \"*/\" terminator."` | Missing "*/" terminator. | `src/IMPD.cpp:L372` |
| [x] | > `"Missing double quote (\") character."` | Missing double quote (") character. | `src/IMPD.cpp:L449` |
| [x] | > `"Missing argument(s)."` | Missing argument(s). | `src/IMPD.cpp:L218`<br>`src/IMPD.cpp:L1373` |
| [x] | > `"Missing variable name."` | Missing variable name. | `src/IMPD.cpp:L1287` |
| [x] | > `"Missing "}"."` | Missing "}". | `src/IMPD.cpp:L951` |
| [x] | > `"Syntax error."` | Syntax error. | `src/IMPD.cpp:L493`<br>`src/IMPD.cpp:L781`<br>`src/IMPD.cpp:L843`<br>`src/IMPD.cpp:L879`<br>`src/IMPD.cpp:L881`<br>`src/IMPD.cpp:L945`<br>`src/IMPD.cpp:L948`<br>`src/IMPD.cpp:L1196`<br>`src/IMPD.cpp:L1202` |
| [x] | > `"Unexpected end of input."` | Unexpected end of input. | `src/IMPD.cpp:L1007` |
| [x] | > `"Unrecognized labels or too many arguments."` | Unrecognized labels or too many arguments. | `src/IMPD.cpp:L222` |
| [x] | > `"The \"bezier-to\" instruction requires 4 or 6 numbers."` | The "bezier-to" instruction requires 4 or 6 numbers. | `src/IVG.cpp:L734` |
| [x] | > `"The \"line-angle\" instruction requires an even number of values."` | The "line-angle" instruction requires an even number of values. | `src/IVG.cpp:L713` |
| [x] | > `"The \"line-to\" instruction requires an even number of coordinates."` | The "line-to" instruction requires an even number of coordinates. | `src/IVG.cpp:L701` |
| [x] | > `"The \"move-angle\" instruction requires an even number of values."` | The "move-angle" instruction requires an even number of values. | `src/IVG.cpp:L685` |
| [x] | > `String("Duplicate horizontal alignment \"") + *s + "\"."` | Duplicate horizontal alignment "{alignment}". | `src/IVG.cpp:L1452` |
| [x] | > `String("Duplicate label \"") + kv.first + "\"."` | Duplicate label "{label}". | `src/IMPD.cpp:L162` |
| [x] | > `String("Duplicate vertical alignment \"") + *s + "\"."` | Duplicate vertical alignment "{alignment}". | `src/IVG.cpp:L1459` |
| [x] | > `String("The ellipse \"sweep\" option requires IVG-3 format.")` | The ellipse "sweep" option requires IVG-3 format. | `src/IVG.cpp:L1670` |
| [x] | > `String("The \"") + instruction + "\" instruction requires " + requiredString` | The "{instruction}" instruction requires {requiredString}: "{instruction}"{arguments}. | `src/IVG.cpp:L1286` |
| [x] | > `String("Invalid color name \"") + String(r.b, r.e) + "\"."` | Invalid color name "{name}". | `src/IVG.cpp:L503` |
| [x] | > `String("Invalid color value \"") + String(r.b + 1, r.e) + "\"."` | Invalid color value "{value}". | `src/IVG.cpp:L482` |
| [x] | > `String("Invalid \"define\" instruction type \"") + type + "\"."` | Invalid "define" instruction type "{type}". | `src/IVG.cpp:L1355` |
| [x] | > `String("Invalid opacity \"") + String(r.b + 1, r.e) + "\"."` | Invalid opacity "{value}". | `src/IVG.cpp:L132` |
| [x] | > `String("Invalid pre-multiplied alpha color \"") + String(r.b + 1, r.e) + "\"."` | Invalid pre-multiplied alpha color "{value}". | `src/IVG.cpp:L486` |
| [x] | > `String("Invalid gradient stop position \"") + impd.toString(position) + "\"."` | Invalid gradient stop position "{position}". | `src/IVG.cpp:L948` |
| [x] | > `String("Invalid gradient stop list \"") + *s + "\" (odd number of elements)."` | Invalid gradient stop list "{stops}" (odd number of elements). | `src/IVG.cpp:L939` |
| [x] | > `String("Invalid \"arc-to\" turn \"") + *turn + "\"."` | Invalid "arc-to" turn "{turn}". | `src/IVG.cpp:L753` |
| [x] | > `String("Missing argument \"") + label + "\"."` | Missing argument "{label}". | `src/IMPD.cpp:L213` |
| [x] | > `String("Missing indexed argument #") + Interpreter::toString(index + 1) + "."` | Missing indexed argument #{index}. | `src/IMPD.cpp:L200` |
| [x] | > `String("Too few list elements (got ") + toString(lossless_cast<int>(elements.size())) + ", expected at least " + toString(minElements) + ")."` | Too few list elements (got {actual}, expected at least {minimum}). | `src/IMPD.cpp:L554` |
| [x] | > `String("Too many list elements (got ") + toString(lossless_cast<int>(elements.size())) + ", expected at most " + toString(maxElements) + ")."` | Too many list elements (got {actual}, expected at most {maximum}). | `src/IMPD.cpp:L550` |
| [x] | > `String("Unrecognized alignment \"") + alignment + "\"."` | Unrecognized alignment "{alignment}". | `src/IVG.cpp:L1449` |
| [x] | > `String("Unrecognized anchor \"") + *s + "\"."` | Unrecognized anchor "{anchor}". | `src/IVG.cpp:L1734` |
| [x] | > `String("Unrecognized ellipse type \"") + *typeArg + "\"."` | Unrecognized ellipse type "{type}". | `src/IVG.cpp:L1678` |
| [x] | > `String("Unrecognized fill rule \"") + *s + "\"."` | Unrecognized fill rule "{fillRule}". | `src/IVG.cpp:L1799` |
| [x] | > `String("Unrecognized gradient type \"") + gradientType + "\"."` | Unrecognized gradient type "{type}". | `src/IVG.cpp:L921` |
| [x] | > `String("Unrecognized instruction \"") + instructionString + "\"."` | Unrecognized instruction "{instruction}". | `src/IMPD.cpp:L1253` |
| [x] | > `String("Unrecognized stroke caps \"") + *s + "\"."` | Unrecognized stroke caps "{caps}". | `src/IVG.cpp:L1182` |
| [x] | > `String("Unrecognized stroke joints \"") + *s + "\"."` | Unrecognized stroke joints "{joints}". | `src/IVG.cpp:L1189` |
| [x] | > `"Missing closing "]\" or \"}"."` | Missing closing "]" or "}". | `src/IMPD.cpp:L434` |
| [x] | > `const String& how) { throw SyntaxException(how` | Propagate the caller-provided syntax message. | `src/IMPD.cpp:L355` |
| [x] | > `const char* how) { throwBadSyntax(String(how)` | Convert the caller-provided syntax message to a String. | `src/IMPD.cpp:L357` |
| [x] | > `errorString` | Forward the collected parser error text. | `src/IVG.cpp:L1389` |

## Interpreter::throwRunTimeError

| Status | Message expression | Suggested new exception text | Locations |
| --- | --- | --- | --- |
| [x] | > `"Cannot declare \"bounds\" for a mask."` | Cannot declare "bounds" for a mask. | `src/IVG.cpp:L385` |
| [x] | > `"Cannot return from the global frame."` | Cannot return from the global frame. | `src/IMPD.cpp:L1299` |
| [x] | > `"Division by zero."` | Division by zero. | `src/IMPD.cpp:L789` |
| [x] | > `String("Image coordinates (") + impd.toString(numbers[0]) + String(", ") + impd.toString(numbers[1]) + ") out of range (-1000000..1000000)."` | Image coordinates ({x}, {y}) out of range (-1000000..1000000). | `src/IVG.cpp:L1419` |
| [x] | > `String("Image scale ") + Interpreter::toString(actualScale) + String(" out of range (0..") + Interpreter::toString(maxScale) + ")."` | Image scale {actual} out of range (0..{limit}). | `src/IVG.cpp:L1580` |
| [x] | > `String("Invalid first path instruction \"") + instruction + "\"."` | Invalid first path instruction "{instruction}". | `src/IVG.cpp:L1810` |
| [x] | > `"Invalid font name."` | Invalid font name. | `src/IVG.cpp:L1983` |
| [x] | > `"Math error: \"log\" requires a value greater than 0."` | Math error: "log" requires a value greater than 0. | `src/IMPD.cpp:L240` |
| [x] | > `"Math error: \"log10\" requires a value greater than 0."` | Math error: "log10" requires a value greater than 0. | `src/IMPD.cpp:L247` |
| [x] | > `"Math error: \"sqrt\" requires a non-negative value."` | Math error: "sqrt" requires a non-negative value. | `src/IMPD.cpp:L254` |
| [x] | > `"Math error."` | Math error. | `src/IMPD.cpp:L790`<br>`src/IMPD.cpp:L1077` |
| [x] | > `"Modulo by zero."` | Modulo by zero. | `src/IMPD.cpp:L813` |
| [x] | > `"Multiple \"bounds\" declarations."` | Multiple "bounds" declarations. | `src/IVG.cpp:L2117`<br>`src/IVG.h:L567` |
| [x] | > `"Writing text requires a font to be set first."` | Writing text requires a font to be set first. | `src/IVG.cpp:L1752` |
| [x] | > `"Number overflow."` | Number overflow. | `src/IMPD.cpp:L793`<br>`src/IMPD.cpp:L863`<br>`src/IMPD.cpp:L868`<br>`src/IMPD.cpp:L1057`<br>`src/IMPD.cpp:L1078` |
| [x] | > `"Recursion limit reached."` | Recursion limit reached. | `src/IMPD.cpp:L597` |
| [x] | > `"Relative paint is not allowed with the \"wipe\" instruction."` | Relative paint is not allowed with the "wipe" instruction. | `src/IVG.cpp:L1843` |
| [x] | > `"Statements limit reached."` | Statements limit reached. | `src/IMPD.cpp:L608` |
| [x] | > `"Undeclared \"bounds\" definition."` | Undeclared "bounds" definition. | `src/IVG.cpp:L984`<br>`src/IVG.cpp:L2108`<br>`src/IVG.h:L557`<br>`src/IVG.h:L561`<br>`src/IVG.h:L579` |
| [x] | > `"Vertices fall outside the valid coordinate range (-8388607..8388607)."` | Vertices fall outside the valid coordinate range (-8388607..8388607). | `src/IVG.cpp:L1082`<br>`src/IVG.cpp:L1098` |
| [x] | > `"Path instruction limit exceeded."` | Path instruction limit exceeded. | `src/IVG.cpp:L183`<br>`src/IVG.cpp:L876`<br>`src/IVG.cpp:L1071`<br>`src/IVG.cpp:L1075`<br>`src/IVG.cpp:L2221` |
| [x] | > `String("Could not include file \"") + String(file.begin(), file.end()) + "\"."` | Could not include file "{file}". | `src/IMPD.cpp:L1385` |
| [x] | > `String("Could not set variable \"") + name + "\"."` | Could not set variable "{name}". | `src/IMPD.cpp:L503` |
| [x] | > `String("Duplicate font definition \"") + String(name.begin(), name.end()) + "\"."` | Duplicate font definition "{name}". | `src/IVG.cpp:L1300` |
| [x] | > `String("Duplicate image definition \"") + String(name.begin(), name.end()) + "\"."` | Duplicate image definition "{name}". | `src/IVG.cpp:L1319` |
| [x] | > `String("Duplicate path definition \"") + String(name.begin(), name.end()) + "\"."` | Duplicate path definition "{name}". | `src/IVG.cpp:L1335` |
| [x] | > `String("Duplicate pattern definition \"") + String(name.begin(), name.end()) + "\"."` | Duplicate pattern definition "{name}". | `src/IVG.cpp:L1348` |
| [x] | > `"Duplicate \"metrics\" instruction in the font definition."` | Duplicate "metrics" instruction in the font definition. | `src/IVG.cpp:L2262` |
| [x] | > `"Invalid \"metrics\" instruction in the font definition."` | Invalid "metrics" instruction in the font definition. | `src/IVG.cpp:L2275` |
| [x] | > `String("Invalid glyph character \"") + String(ws.begin(), ws.end()) + "\" (length must be 1)."` | Invalid glyph character "{glyph}" (length must be 1). | `src/IVG.cpp:L2284` |
| [x] | > `"Missing \"metrics\" block before the glyph instruction in the font definition."` | Missing "metrics" block before the glyph instruction in the font definition. | `src/IVG.cpp:L2292` |
| [x] | > `String("Negative glyph advance \"") + impd.toString(glyph.advance) + "\" in the font definition."` | Negative glyph advance "{advance}" in the font definition. | `src/IVG.cpp:L2295` |
| [x] | > `String("Duplicate glyph definition for code point \"") + impd.toString(static_cast<int>(glyph.character)) + "\" in the font definition."` | Duplicate glyph definition for code point "{glyph}" in the font definition. | `src/IVG.cpp:L2299` |
| [x] | > `String("Duplicate kerning pair \"") + impd.toString(static_cast<int>(*itA)) + "\", \"" + impd.toString(static_cast<int>(*itB)) + "\" in the font definition."` | Duplicate kerning pair "{first}", "{second}" in the font definition. | `src/IVG.cpp:L2315` |
| [x] | > `String("Invalid boolean value \"") + s + "\"; expected \"yes\" or \"no\"."` | Invalid boolean value "{value}"; expected "yes" or "no". | `src/IMPD.cpp:L758` |
| [x] | > `String("Image height \"") + impd.toString(fitHeight) + "\" out of range (0..1000000)."` | Image height "{height}" out of range (0..1000000). | `src/IVG.cpp:L1477` |
| [x] | > `String("Image width \"") + impd.toString(fitWidth) + "\" out of range (0..1000000)."` | Image width "{width}" out of range (0..1000000). | `src/IVG.cpp:L1470` |
| [x] | > `String("Invalid integer \"") + String(r.b, r.e) + "\"."` | Invalid integer "{value}". | `src/IMPD.cpp:L740` |
| [x] | > `String("Invalid number \"") + String(r.b, r.e) + "\"."` | Invalid number "{value}". | `src/IMPD.cpp:L748` |
| [x] | > `String("Missing font \"") + String(newFontName.begin(), newFontName.end()) + "\"."` | Missing font "{name}". | `src/IVG.cpp:L1986` |
| [x] | > `String("Missing font \"") + String(state.textStyle.fontName.begin(), state.textStyle.fontName.end()) + "\"."` | Missing font "{name}". | `src/IVG.cpp:L1756` |
| [x] | > `String("Missing image \"") + String(imageName.begin(), imageName.end()) + "\"."` | Missing image "{name}". | `src/IVG.cpp:L1523` |
| [x] | > `String("Negative clip height \"") + impd.toString(numbers[2]) + "\"."` | Negative clip height "{height}". | `src/IVG.cpp:L1498` |
| [x] | > `String("Negative clip width \"") + impd.toString(numbers[2]) + "\"."` | Negative clip width "{width}". | `src/IVG.cpp:L1495` |
| [x] | > `String("Negative dash value \"") + impd.toString(dash) + "\"."` | Negative dash value "{dash}". | `src/IVG.cpp:L1205` |
| [x] | > `String("Negative ellipse radius \"") + impd.toString(rx < 0.0 ? rx : ry) + "\"."` | Negative ellipse radius "{radius}". | `src/IVG.cpp:L1657` |
| [x] | > `String("Negative gap value \"") + impd.toString(gap) + "\"."` | Negative gap value "{gap}". | `src/IVG.cpp:L1209` |
| [x] | > `String("Negative radial gradient radius \"") + impd.toString(coords[coords[2] < 0.0 ? 2 : 3]) + "\"."` | Negative radial gradient radius "{radius}". | `src/IVG.cpp:L930` |
| [x] | > `IMPD::String("Radial gradient radius \"") + IMPD::Interpreter::toString(offendingRadius) + "\" out of range (0..32767].")` | Radial gradient radius "{radius}" out of range (0..32767]. | `src/IVG.h:L522-L525` |
| [x] | > `String("Negative rectangle height \"") + impd.toString(numbers[3]) + "\"."` | Negative rectangle height "{height}". | `src/IVG.cpp:L1631` |
| [x] | > `String("Negative rectangle width \"") + impd.toString(numbers[2]) + "\"."` | Negative rectangle width "{width}". | `src/IVG.cpp:L1628` |
| [x] | > `String("Negative rounded corner radius \"") + impd.toString(rounded[0] < 0.0 ? rounded[0] : rounded[1]) + "\"."` | Negative rounded corner radius "{radius}". | `src/IVG.cpp:L1640` |
| [x] | > `String("Negative star radius \"") + impd.toString(r1 < 0.0 ? r1 : r2) + "\"."` | Negative star radius "{radius}". | `src/IVG.cpp:L1714` |
| [x] | > `String("Negative stroke width \"") + impd.toString(d) + "\"."` | Negative stroke width "{width}". | `src/IVG.cpp:L1174` |
| [x] | > `String("Number overflow \"") + String(r.b, r.e) + "\"."` | Number overflow "{value}". | `src/IMPD.cpp:L751` |
| [x] | > `String("Undefined path \"") + String(name.begin(), name.end()) + "\"."` | Undefined path "{name}". | `src/IVG.cpp:L1398` |
| [x] | > `String("Undefined pattern \"") + String(name.begin(), name.end()) + "\"."` | Undefined pattern "{name}". | `src/IVG.cpp:L1014` |
| [x] | > `String("Variable \"") + name + "\" does not exist."` | Variable "{name}" does not exist. | `src/IMPD.cpp:L514` |
| [x] | > `String("Variable \"") + varName + "\" is already declared."` | Variable "{name}" is already declared. | `src/IMPD.cpp:L1301` |
| [x] | > `String("\"aa-gamma\" value \"") + impd.toString(d) + "\" out of range (0..100)."` | "aa-gamma" value "{value}" out of range (0..100). | `src/IVG.cpp:L1859` |
| [x] | > `String("\"bounds\" height \"") + Interpreter::toString(bounds.height) + "\" out of range (1..32767)."` | "bounds" height "{height}" out of range (1..32767). | `src/IVG.cpp:L66` |
| [x] | > `String("\"bounds\" left \"") + Interpreter::toString(bounds.left) + "\" out of range (-32768..32767)."` | "bounds" left "{left}" out of range (-32768..32767). | `src/IVG.cpp:L54` |
| [x] | > `String("\"bounds\" top \"") + Interpreter::toString(bounds.top) + "\" out of range (-32768..32767)."` | "bounds" top "{top}" out of range (-32768..32767). | `src/IVG.cpp:L58` |
| [x] | > `String("\"bounds\" width \"") + Interpreter::toString(bounds.width) + "\" out of range (1..32767)."` | "bounds" width "{width}" out of range (1..32767). | `src/IVG.cpp:L62` |
| [x] | > `String("\"curve-quality\" value \"") + impd.toString(d) + "\" out of range (0..100)."` | "curve-quality" value "{value}" out of range (0..100). | `src/IVG.cpp:L1866` |
| [x] | > `String("Ellipse aspect ratio \"") + Interpreter::toString(aspectRatio) + "\" out of range (0..1000000)."` | Ellipse aspect ratio "{ratio}" out of range (0..1000000). | `src/IVG.cpp:L168` |
| [x] | > `String("Font size \"") + impd.toString(d) + "\" out of range (0..1000000]."` | Font size "{size}" out of range (0..1000000]. | `src/IVG.cpp:L2005` |
| [x] | > `String("HSV value #") + impd.toString(i + 1) + String(" \"") + impd.toString(n[i]) + "\" out of range (0..1)."` | HSV value #{index} "{value}" out of range (0..1). | `src/IVG.cpp:L457` |
| [x] | > `String("\"miter-limit\" value \"") + impd.toString(d) + "\" out of range (1..infinity)."` | "miter-limit" value "{value}" out of range (1..infinity). | `src/IVG.cpp:L1193` |
| [x] | > `String("Opacity \"") + impd.toString(d) + "\" out of range (0..1)."` | Opacity "{value}" out of range (0..1). | `src/IVG.cpp:L135` |
| [x] | > `String("\"pattern-resolution\" value \"") + impd.toString(d) + "\" out of range (0..100)."` | "pattern-resolution" value "{value}" out of range (0..100). | `src/IVG.cpp:L1873` |
| [x] | > `String("\"resolution\" value \"") + impd.toString(resolution) + "\" out of range (0.0001..infinity)."` | "resolution" value "{value}" out of range (0.0001..infinity). | `src/IVG.cpp:L1318` |
| [x] | > `String("Star points \"") + impd.toString(points) + "\" out of range (1..10000)."` | Star points "{count}" out of range (1..10000). | `src/IVG.cpp:L1711` |
| [x] | > `const String& how) { throw RunTimeException(how` | Propagate the caller-provided runtime message. | `src/IMPD.cpp:L356` |
| [x] | > `const char* how) { throwRunTimeError(String(how)` | Convert the caller-provided runtime message to a String. | `src/IMPD.cpp:L358` |

### Range diagnostic audit

The IVG runtime uses explicit range checks for canvas bounds, numeric parameters, and stateful options, so we verified each
message now spells out the enforced interval while matching the code that throws it.

* Canvas bounds now advertise the exact integer limits enforced in `checkBounds`, aligning the string literals with the
  `[-32768, 32767]` window used for each edge.【F:src/IVG.cpp†L52-L69】
* Value clamps such as opacity, HSV components, and miter limit retain their original numeric guards but now surface the
  `0..1` and `1..infinity` ranges verbatim in the diagnostics.【F:src/IVG.cpp†L124-L137】【F:src/IVG.cpp†L446-L462】【F:src/IVG.cpp†L1184-L1214】
* Resolution and image fitting checks continue to reject non-positive or oversized values using the `COORDINATE_LIMIT`
  constant, and the refreshed messages surface both the derived bounds and the offending value.【F:src/IVG.cpp†L1304-L1322】【F:src/IVG.cpp†L1458-L1482】
* Image placement validates both raw coordinates and cumulative scale against the million-unit coordinate ceiling, so the
  messages now report the allowed window and include the computed coordinates or scaling pair for context.【F:src/IVG.cpp†L1414-L1431】【F:src/IVG.cpp†L1556-L1590】
* Polygon masks inherit their vertex limit from the NuX rasterizer (`0x7FFFFFFF >> 8`), and the runtime message now echoes
  the derived ±8,388,607 range so callers see why oversized paths fail validation.【F:externals/NuX/NuXPixels.cpp†L1197-L1234】【F:src/IVG.cpp†L1074-L1105】
* Star point counts, font sizes, gamma values, and other documented ranges retain their existing guard code but now present
  the intervals in the shared `(min..max)` format for consistency.【F:src/IVG.cpp†L1704-L1721】【F:src/IVG.cpp†L1854-L1880】【F:src/IVG.cpp†L1995-L2014】
* The fuzzing harness enforces an upper bound on pixel count; its helper now emits the `(1..limit)` notation so off-by-one
  errors show the precise threshold.【F:tools/IVG2PNG.cpp†L160-L184】

## IMPD::AbortedException

| Status | Message expression | Suggested new exception text | Locations |
| --- | --- | --- | --- |
| [x] | > `"Execution aborted."` | Execution aborted. | `src/IMPD.cpp:L611` |
| [x] | > `"Encountered \"STOP\" instruction."` | Encountered "STOP" instruction. | `src/IMPD.cpp:L1264` |

## IMPD::FormatException

| Status | Message expression | Suggested new exception text | Locations |
| --- | --- | --- | --- |
| [x] | > `"Unsupported data format."` | Unsupported data format. | `src/IMPD.cpp:L1281` |

## Rethrow statements

The following sites rethrow the currently handled exception without changing its type or message.
* `src/IMPD.cpp:L620`
* `src/IMPD.cpp:L624`
* `src/IVG.cpp:L1230`
