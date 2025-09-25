# IVG Keyword Case Sensitivity Report

This report catalogs every place in the IVG sources where textual keywords or identifiers are parsed, noting whether the comparison logic is case sensitive. The investigation covered `src/IMPD.cpp`, `src/IMPD.h`, `src/IVG.cpp`, and `src/IVG.h` to ensure every keyword path was reviewed. The `Interpreter::toLower` helper only normalizes ASCII `A`–`Z`, so non-ASCII letters retain their original casing even when a feature is described as "case-insensitive."【F:src/IMPD.cpp†L762-L769】【F:src/IMPD.h†L187-L191】

## Summary Table

| Feature | Example Keywords / Values | Case Sensitivity | Implementation Notes |
| --- | --- | --- | --- |
| Instruction names (built-in `stop`, `format`, etc.) | `_debug`, `include`, `trace` | Case-insensitive | Statements are lowered before dispatch, so instruction handlers always receive lowercase tokens.【F:src/IMPD.cpp†L584-L593】【F:src/IMPD.cpp†L288-L301】 |
| IVG executor instructions | `rect`, `fill`, `mask` | Case-insensitive | The main IVG executor and sub-executors (paths, transforms, fonts) read instructions that were already normalized by the interpreter, so hash lookups succeed regardless of source casing.【F:src/IMPD.cpp†L584-L593】【F:src/IVG.cpp†L112-120】【F:src/IVG.cpp†L600-L704】【F:src/IVG.cpp†L2224-L2259】 |
| Transformation commands inside `transform` blocks | `matrix`, `rotate`, `scale` | Case-insensitive | The transformation executor compares against lowercase strings because the interpreter supplies lowered instruction identifiers.【F:src/IMPD.cpp†L584-L593】【F:src/IVG.cpp†L600-L668】 |
| Path sub-instructions | `move-to`, `arc-to`, `line-angle`, etc. | Case-insensitive | Path instructions are resolved through a hash lookup on lowered tokens, so any ASCII casing is accepted.【F:src/IMPD.cpp†L584-L593】【F:src/IVG.cpp†L552-L820】 |
| Font block instructions | `metrics`, `glyph`, `kern` | Case-insensitive | Font parsing relies on lowered instruction names supplied by the interpreter before hashing.【F:src/IMPD.cpp†L584-L593】【F:src/IVG.cpp†L2224-L2259】 |
| Argument labels | `width:`, `linegap:`, `pattern:` | Case-insensitive | Labels are lowered when parsed and stored in the argument container, so callers can fetch them with canonical lowercase names regardless of the original casing.【F:src/IMPD.cpp†L155-L215】 |
| Format identifiers | `IVG-1`, `IVGFONT-1` | Case-insensitive | `format` arguments and their `uses`/`requires` lists are lowered before executor callbacks, allowing flexible casing in scripts.【F:src/IMPD.cpp†L1265-L1281】【F:src/IVG.cpp†L1220-L1234】【F:src/IVG.cpp†L2228-L2236】 |
| Color specifications | Named colors (`silver`), `rgb(...)`, `hsv(...)` | Case-insensitive | Color parsing lowers the function prefix and color-name tokens prior to lookup, so any ASCII casing works for color keywords.【F:src/IVG.cpp†L448-L505】 |
| Gradient type & stop helpers | `linear`, `radial`, stop colors | Case-insensitive | Gradient type strings and stop color values are lowered before validation; color names inherit the color parser’s insensitivity.【F:src/IVG.cpp†L912-L967】 |
| Stroke style keywords | `caps=Round`, `joints=MITER`, `dash=none` | Case-insensitive | Stroke value strings are lowered (`caps`, `joints`, and special value `none`), ensuring ASCII case does not matter.【F:src/IVG.cpp†L1170-L1214】 |
| Path-specific value keywords | Arc `turn=cw/ccw`, ellipse `type=pie/chord`, `anchor=center` | Case-insensitive | These options are normalized with `toLower` before comparison, so uppercase or mixed-case inputs behave identically.【F:src/IVG.cpp†L747-L769】【F:src/IVG.cpp†L1660-L1700】【F:src/IVG.cpp†L1728-L1746】 |
| Fill rules | `rule=even-odd`, `rule=non-zero` | Case-insensitive | `fill` rule strings are lowered before matching, so casing is ignored.【F:src/IVG.cpp†L1798-L1808】 |
| Image alignment keywords | `align=Top`, `align=LEFT center` | Case-insensitive | Alignment entries are lowered before the keyword hash lookup, tolerating any ASCII casing for horizontal and vertical alignments.【F:src/IVG.cpp†L1439-L1463】 |
| Mask control keywords | `mask invert`, `mask reset` | Case-insensitive | When a mask instruction takes a direct keyword argument, the interpreter lowers it before comparison, so casing does not matter.【F:src/IVG.cpp†L1906-L1939】 |
| Boolean values passed to `toBool` (`yes` / `no`) | `relative=yes`, `stretch=No` | Case-sensitive | `Interpreter::toBool` performs direct equality checks against the lowercase literals `yes` and `no`, so any other casing triggers a runtime error. This affects every place `toBool` is called (paint `relative`, arc `large`, ellipse `type` options, image `stretch`, mask `inverse`/`inverted`, loop modifiers, etc.).【F:src/IMPD.cpp†L756-L759】【F:src/IVG.cpp†L758-L769】【F:src/IVG.cpp†L1170-L1214】【F:src/IVG.cpp†L1480-L1484】【F:src/IVG.cpp†L1930-L1939】 |
| Math function names | `sin`, `round`, `len`, `def` | Case-sensitive | Function lookup hashes the raw token and compares with `strcmp`, so functions must be written in lowercase exactly as listed; variants like `SIN` are rejected.【F:src/IMPD.cpp†L271-L285】【F:src/IMPD.cpp†L1048-L1087】 |

## Additional Notes

* Because `toLower` only normalizes ASCII capital letters, non-ASCII characters (e.g., accented letters) remain unchanged; keywords containing such characters should therefore be written with the canonical lowercase spelling to avoid mismatch.【F:src/IMPD.cpp†L762-L769】【F:src/IMPD.h†L187-L191】
* Any feature that ultimately calls `Interpreter::toBool` inherits its strict expectation of `"yes"` or `"no"`. When adding new boolean-like options, consider whether this constraint is desirable or if normalization should be applied first.
* The consistent use of lowercase hash tables in the executor code means user content may prefer lowercase for readability, but the engine accepts any ASCII casing everywhere noted as case-insensitive.
