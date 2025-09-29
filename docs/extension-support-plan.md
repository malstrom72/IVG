# Extension Metadata Support Plan

## Background

* `Interpreter::runInstruction` currently special-cases `meta` by returning early even when the active executor does not recognize the tag, so no validation occurs and unrecognized metas always pass silently.【F:src/IMPD.cpp†L1244-L1250】
* During `FORMAT`, the interpreter canonicalizes the identifier and collects `uses:` / `requires:` arguments, but it discards them after the call to `Executor::format`, leaving no persistent record for later `meta` statements or nested interpreters.【F:src/IMPD.cpp†L1258-L1276】
* Inline assets (for example embedded fonts) reuse the existing interpreter while swapping executors, so each embedded document must explicitly receive fresh per-document state to avoid leaking metadata between scopes.【F:src/IMPD.cpp†L341-L352】【F:src/IVG.cpp†L972-L1004】【F:src/IVG.cpp†L1716-L1852】

## Minimal goals

- [x] Introduce a shared `FormatInfo` that stores the active format identifier and the declared `uses:` entries for the current document scope.
- [x] Allow exactly one successful `format` instruction per document while keeping the executor responsible for validating identifiers and dependencies.
- [x] Require every `meta <id>` to be covered by a matching `uses:` declaration (with implicit or explicit version selection) while still letting executors ignore tags they do not implement.
- [x] Continue to let executors process `requires:` entries (other than the interpreter's `impd-1` fast path) without adding extra state to the interpreter.
- [x] Ensure embedded documents allocate a new `FormatInfo` while call stacks and macros keep sharing the parent scope.

## Implementation plan

### 1. Add `FormatInfo` and thread it through interpreters

- [x] Declare a tiny helper type in `IMPD.h`:
  - [x] Add `std::string formatId;` as the lower-cased identifier of the accepted format, keeping it empty until `format` succeeds.
  - [x] Add `std::set<std::string> uses;` so each entry stores the lower-cased `<id>-<version>` token supplied by `uses:`.
- [x] Store a `FormatInfo&` inside `Interpreter`, extending the constructors so callers must pass a reference when creating a new interpreter (root callers create the object; nested interpreters decide whether to reuse or reset it).
- [x] Keep relying on the interpreter's direct reference whenever it needs to read or update the scope so no additional accessors are required inside `Interpreter`.

### 2. Ensure every document scope owns the right instance

- [x] Audit interpreter construction sites:
  - [x] Ensure embedded-document loaders (fonts and other inline assets) allocate a fresh `FormatInfo` before instantiating their interpreter so the new document starts blank.【F:src/IVG.cpp†L972-L1004】【F:src/IVG.cpp†L1716-L1852】
  - [x] Keep call frames (`CALL`, `FOR`, etc.) reusing the parent's reference to preserve shared macros, variables, and metadata.
- [x] Add short comments where the new constructor argument is wired to clarify why a new object is created or shared in each location.

### 3. Rework `FORMAT` handling around the shared state

- [x] In the interpreter's `FORMAT` case, check `formatInfo.formatId` and throw `throwBadSyntax("Duplicate format instruction")` before invoking the executor when it is not empty.
- [x] After parsing the arguments:
  - [x] Lower-case the format identifier and every entry in the `uses:` and `requires:` lists (reusing the existing `transform` calls).
  - [x] Preserve the `uses:` tokens exactly as supplied (`snapshot-1`, `snapshot-2`, ...), avoiding additional parsing.
  - [x] Strip `impd-1` from the `requires:` vector just like today so executors only see dependencies they must validate.
- [x] Populate the shared state around the `format` dispatch by lower-casing the identifier, clearing any previous `FormatInfo`, and inserting each normalized `uses:` token so executors and later `meta` checks see the recorded declarations without changing the existing `Executor::format` signature.
- [x] Leave executors with the validation responsibilities they already own for identifiers, dependencies, and requirements while consulting the passed `FormatInfo` read-only.

### 4. Validate `meta` using the recorded declarations

- [x] Replace the early return in `Interpreter::runInstruction` with dedicated handling when `instructionString == "meta"`:
  - [x] Parse the meta identifier and optional argument list the same way the interpreter already parses other instructions.
  - [x] Lower-case the identifier, splitting an explicit `<id>-<version>` token at the last dash to extract the version and otherwise treating the entire token as the id.
  - [x] Look up matching entries in `getFormatInfo().uses` by scanning for tokens that start with `<id>-`, calling `throwBadSyntax` when none exist so the format must declare the meta via `uses:`.
  - [x] If the meta specified a version, ensure the exact `<id>-<version>` token exists; otherwise iterate over matching tokens, convert the substring after the dash to an integer with `strtol`, track the highest numeric value, and substitute the corresponding `<id>-<version>` pair.
- [x] Forward the resolved `<id>-<version>` token plus the argument range to `executor.execute`, treating a `false` return as a silent ignore and otherwise continuing as usual.

### 5. Keep nested document behaviour intuitive

- [x] When a new interpreter is created for an embedded document, pass both the new executor and a freshly zeroed `FormatInfo` so the executor can immediately confirm the scope is clean via `formatInfo.formatId`.
- [x] When reusing an interpreter for nested calls, forward the existing `FormatInfo&` so macros and subroutines can issue metas that depend on the parent document's declarations.

### 6. Tests and documentation

- [x] Extend regression coverage to include:
  - [x] Successful handling of multiple versions declared via `uses:[snapshot-1 snapshot-2]` alongside both `meta snapshot` (implicit latest version) and `meta snapshot-1` (explicit) calls.
  - [x] Failing cases for duplicate `format`, metas without corresponding `uses:` declarations, and metas that request versions not declared in `uses:`.
  - [x] A scenario where an executor declines a recognized meta version (returning `false`) to confirm the interpreter ignores it without error.
- [x] Update the IMPD documentation to describe the single-format rule, the `uses:` requirement for versioned metas, and the silent-ignore policy for unrecognized tags.【F:docs/ImpD Documentation.md†L330-L360】
- [x] Run `timeout 600 ./build.sh` after implementation to confirm both build configurations still succeed.
