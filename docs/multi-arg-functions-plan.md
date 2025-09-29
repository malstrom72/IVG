# Multi-argument function implementation plan

The guiding principle for every change described below is **minimalism**: extend the existing evaluator and registries only where absolutely necessary so the runtime stays small and easy to audit.

## Original interpreter behavior
- ImpD registers primitive math functions through a fixed lookup table (`findFunction`) that is generated with QuickHashGen. Only 20 entries existed initially: 17 unary math functions, the `pi` constant, and the `len` and `def` helpers. The hash rejected identifiers longer than five characters, so the table could not match names such as `distance`.【F:src/IMPD.cpp†L259-L285】
- Each math entry was implemented by a `double (*)(double)` pointer in the `MATH_FUNCTION_POINTERS` array, and callers expected a single `double` argument. The evaluator invoked the pointer directly after parsing one parenthesized argument expression and performed generic overflow checking via `errno` and `isFinite`.【F:src/IMPD.cpp†L267-L287】【F:src/IMPD.cpp†L1120-L1162】
- The public documentation mirrored this unary-only capability: every primitive function listed in the language guide took a single argument in radians (for the trig operators), and multi-argument helpers were not described.【F:docs/ImpD Documentation.md†L207-L234】

## Limitations that block multi-argument functions
- Because `findFunction` assumes identifiers are at most five characters long and maps directly into the unary function table, any new name longer than that (e.g. `distance`) is discarded before evaluation begins. Extending the hash requires regenerating the lookup tables with the existing `externals/QuickHashGen` utility and adjusting `MATH_FUNCTION_COUNT`/related bookkeeping.【F:src/IMPD.cpp†L259-L285】【F:src/IMPD.h†L60-L120】
- `evaluateOuter` treats a recognized function call as `name(expression)` where `expression` is parsed by a recursive call to `evaluateInner`. There is no parsing for commas inside the parentheses, so `name(arg1, arg2)` would currently stop at the comma and fail to advance the iterator. `evaluateInner`’s operator loop also lacks any case that would consume `,`, reinforcing the assumption that only one argument exists.【F:src/IMPD.cpp†L1010-L1034】【F:src/IMPD.cpp†L1120-L1162】

## Minimal multi-argument rollout checklist
- [x] **Keep the function registry lean.**
  - [x] Replace the single `MATH_FUNCTION_POINTERS` array with a compact descriptor that records an arity and either a unary or binary pointer; no variadic entries are kept.
  - [x] Re-run QuickHashGen with the updated name list so longer identifiers like `atan2` and `hypot` resolve correctly without widening the hash more than necessary.
  - [x] Publish stable enum indexes for the built-ins so call sites never reach into the table with raw integers.
- [x] **Parse comma-separated arguments.**
  - [x] Teach `evaluateOuter` to collect up to two arguments in a tiny stack buffer, returning early if a closing parenthesis arrives with nothing parsed.
  - [x] Let `evaluateInner` break on commas via a dedicated `COMMA` precedence so nested calls reuse the existing recursion without extra branching.
- [x] **Evaluate the multi-argument helpers.**
  - [x] Wire `atan2(y, x)` and `hypot(x, y)` through thin wrappers that clear `errno`, invoke the STL helper, and reuse the existing overflow guards.
  - [x] Remove the experimental four-argument path (`angle`, `distance`) so the dispatcher stays binary-only.
- [x] **Validation and documentation.**
  - [x] Expand the regression tests to cover good and bad usages of the binary helpers, including comma mishandling.
  - [x] Trim the language guide to mention only the surviving built-ins and call out that `atan2`/`hypot` are the sole multi-argument functions.

## Open questions and risks
- Adding more helpers in the future will require revisiting arity limits. Document the reasoning for keeping only unary and binary built-ins so any expansion happens intentionally.【F:docs/ImpD Documentation.md†L207-L236】【F:src/IMPD.cpp†L267-L320】
- Multi-argument evaluation introduces new failure modes (missing commas, empty arguments, unterminated parentheses). Error messages should remain consistent with existing `throwBadSyntax` usage so script authors receive actionable diagnostics.【F:src/IMPD.cpp†L1010-L1162】
- QuickHashGen changes affect code generation. Capture the exact generator inputs (e.g. update `tools/` scripts or checked-in metadata) so the hash table can be reproduced deterministically during future maintenance.【F:src/IMPD.cpp†L270-L285】
