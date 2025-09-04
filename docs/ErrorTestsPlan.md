# Error Test Improvement Plan

## Summary
- The project's documentation lists a broad set of possible exceptions for IMPD and IVG components.
- Current invalid IMPD tests already exercise duplicate labels, missing/extra arguments, missing delimiters, recursion limits, division by zero, and math errors.
- Invalid IVG tests cover define-type errors, font-definition issues, alignment problems, unsupported data formats, and missing fonts.
- Comparing documentation with the tests shows coverage for 22/43 exception types:
	- IMPD syntax: 6/10
	- IMPD runtime: 7/11
	- IVG-specific: 9/22
- Missing coverage includes messages such as "Missing argument(s)," "Unrecognized labels or too many arguments," "Number overflow," "Invalid boolean," "Duplicate font/image/path definition," "resolution out of range," and negative geometry checks.

## Additional tests to add

### IMPD
- No-argument call to trigger "Missing argument(s)"
- Instruction with extra/unknown labels ("Unrecognized labels or too many arguments")
- Cases for missing closing quote, expected colon/equal, missing parenthesis, or unfinished expressions ("Unexpected end")
- Expressions generating numeric overflow
- Booleans other than yes/no
- Failed variable assignment ("Could not set variable")
- Executor that aborts progress
- `INCLUDE` referencing a non-existent file

### IVG
- Duplicate `define font/image/path` blocks
- `define` statements with invalid resolution
- Negative clip, rectangle, rounded-corner, ellipse, and star parameters
- Star with point count outside `[1..10000]`
- Referencing a non-existent font ("Missing font: name")
- Using an undefined path
- Relative paint with `wipe`
- Out-of-range `aa-gamma`, `curve-quality`, or `pattern-resolution` settings
- Drawing without bounds, re-declaring bounds, or bounds outside `[1..32767]`
- Invalid metrics instruction or negative glyph advance
- Repeated alignment tokens in image placement
