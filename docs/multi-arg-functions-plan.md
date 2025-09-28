# Multi-argument function implementation plan

The guiding principle for every change described below is **minimalism**: extend the existing evaluator and registries only where absolutely necessary so the runtime stays small and easy to audit.

## Original interpreter behavior
- ImpD registers primitive math functions through a fixed lookup table (`findFunction`) that is generated with QuickHashGen. Only 20 entries existed initially: 17 unary math functions, the `pi` constant, and the `len` and `def` helpers. The hash rejected identifiers longer than five characters, so the table could not match names such as `distance`.【F:src/IMPD.cpp†L259-L285】
- Each math entry was implemented by a `double (*)(double)` pointer in the `MATH_FUNCTION_POINTERS` array, and callers expected a single `double` argument. The evaluator invoked the pointer directly after parsing one parenthesized argument expression and performed generic overflow checking via `errno` and `isFinite`.【F:src/IMPD.cpp†L259-L280】【F:src/IMPD.cpp†L1073-L1100】
- The public documentation mirrored this unary-only capability: every primitive function listed in the language guide took a single argument in radians (for the trig operators), and multi-argument helpers were not described.【F:docs/ImpD Documentation.md†L207-L234】

## Limitations that block multi-argument functions
- Because `findFunction` assumes identifiers are at most five characters long and maps directly into the unary function table, any new name longer than that (e.g. `distance`) is discarded before evaluation begins. Extending the hash requires regenerating the lookup tables with the existing `externals/QuickHashGen` utility and adjusting `MATH_FUNCTION_COUNT`/related bookkeeping.【F:src/IMPD.cpp†L259-L285】【F:src/IMPD.h†L60-L120】
- `evaluateOuter` treats a recognized function call as `name(expression)` where `expression` is parsed by a recursive call to `evaluateInner`. There is no parsing for commas inside the parentheses, so `name(arg1, arg2)` would currently stop at the comma and fail to advance the iterator. `evaluateInner`’s operator loop also lacks any case that would consume `,`, reinforcing the assumption that only one argument exists.【F:src/IMPD.cpp†L979-L1005】【F:src/IMPD.cpp†L1068-L1105】

## Minimal multi-argument rollout checklist
- [x] **Extend the function registry.**
  - [x] Replace the single `MATH_FUNCTION_POINTERS` array with a tiny descriptor table:
    ```cpp
    struct MathFunction
    {
            int arity;                 // 1, 2, or 4 for the new helpers
            MathDispatch dispatch;     // tagged union of function pointers
    };
    ```
    Keep the struct in `IMPD.cpp` so no headers grow. The tagged union can be a `double (*unary)(double)` plus `double (*binary)(double, double)` and a bespoke `double (*variadic)(const double*, int)` for the 4-argument utilities. That avoids heap allocations and keeps lookup O(1).
  - [x] Re-run QuickHashGen with the expanded name list to regenerate the `FUNCTION_HASH_TABLE` so identifiers longer than five characters (`angle`, `distance`) map correctly. Record the generator command in a comment next to the table (mirroring the existing approach) so the minimal workflow stays reproducible.【F:src/IMPD.cpp†L259-L285】
  - [x] Add an `enum` or `constexpr` indexes for `atan2`, `hypot`, `angle`, and `distance` so call sites never rely on raw integers. Update `MATH_FUNCTION_COUNT` and any loops that assume a 1:1 relationship between the hash index and a unary pointer.【F:src/IMPD.h†L60-L120】
- [x] **Parse comma-separated arguments.**
  - [x] In `evaluateOuter`, after consuming the identifier and `(`, loop with a local counter:
    1. Call `evaluateInner` to compute each argument.
    2. Push the result into a fixed-size stack array (max 4 doubles) so no allocations are needed.
    3. If the next token is `,`, advance `p` and continue; if it is `)`, stop; otherwise forward the iterator to `throwBadSyntax`.
- [x] In `evaluateInner`, teach the main operator loop that a comma terminates the current subexpression by returning to the caller without consuming more characters. This allows the existing recursion to remain untouched while guaranteeing the iterator always advances past separators.【F:src/IMPD.cpp†L979-L1105】
- [x] **Implement per-function evaluation.**
  - [x] `atan2(y, x)` and `hypot(x, y)` should call `std::atan2`/`std::hypot`. Wrap them in thin static helpers that take two doubles and apply the existing error template: `errno = 0; result = fn(a, b); if (errno || !isFinite(result)) throwMathError();` This mirrors the unary flow without duplicating boilerplate.【F:src/IMPD.cpp†L259-L280】【F:src/IMPD.cpp†L1073-L1100】
  - [x] `angle(fromX, fromY, toX, toY)` can share a `computeDelta(from, to)` helper that produces the x/y deltas. Feed those into `std::atan2(dy, dx)` and, if stakeholders confirm degrees, multiply by `180.0 / PI` before returning. Use the same math-error guard pattern.
  - [x] `distance(fromX, fromY, toX, toY)` should reuse the deltas and call `std::hypot` (which already performs a stable Euclidean norm) to ensure large inputs stay well-conditioned.
  - [x] Store these helpers in `IMPD.cpp` alongside the existing math wrappers so no new translation units are introduced.
- [x] **Validation and documentation.**
  - [x] Add regression tests under `tests/` that cover normal, negative, and degenerate coordinates, plus malformed invocations (`angle(1,)`) to confirm `throwBadSyntax` remains informative.
  - [x] Update `docs/ImpD Documentation.md` to describe the new helpers, explicitly stating whether `angle` returns degrees or radians so scripting authors do not guess. Mirror the minimal prose already used for existing functions.【F:docs/ImpD Documentation.md†L207-L234】

## Open questions and risks
- The language currently treats trig inputs/outputs as radians, yet vector-based drawing commands expect degrees. Clarify the desired unit for `angle` before implementation to avoid silent mismatches between expression results and path instructions.【F:docs/IVG Documentation.md†L336-L345】【F:docs/ImpD Documentation.md†L207-L234】
- Multi-argument evaluation introduces new failure modes (missing commas, empty arguments, unterminated parentheses). Error messages should remain consistent with existing `throwBadSyntax` usage so script authors receive actionable diagnostics.【F:src/IMPD.cpp†L1033-L1105】
- QuickHashGen changes affect code generation. Capture the exact generator inputs (e.g. update `tools/` scripts or checked-in metadata) so the hash table can be reproduced deterministically during future maintenance.【F:src/IMPD.cpp†L270-L285】
