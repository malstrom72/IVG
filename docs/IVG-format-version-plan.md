# Plan: Strict IVG Format Version Handling

- [ ] Track the IVG format version
	- Add a `formatVersion` field and accessor in `IVGExecutor` so the interpreter knows whether the file is IVG-1/2/3.
	- Update `format` to set this field when the header is parsed; currently it only validates the identifier and drops the information
	- When no `FORMAT` header appears, default the `formatVersion` to a sentinel meaning "any" so every instruction remains valid
	- Run `timeout 600 ./build.sh` to ensure compilation still succeeds.
- [ ] Reject version-specific instructions
	- In `IVGExecutor::execute`, gate features based on `formatVersion`; skip these checks if no format was declared:
		- IVG-1: allow only instructions defined in IVG-1 and reject everything introduced later, including `DEFINE`, `FONT`, `TEXT`, `IMAGE`, `PATH`, `LINE`, and `POLYGON`.
		- IVG-2: reject IVG-3-only instructions (`PATH`, `LINE`, `POLYGON`).
	- Emit `Interpreter::throwBadSyntax` with a helpful message when an instruction is disallowed.
	- Run `timeout 600 ./build.sh` and verify regression tests.
- [ ] Restrict syntax of geometry instructions
	- **Ellipse:** require comma-separated numbers for IVG-1/2 and space-separated pairs for IVG-3 by checking whether the second argument is present (`secondArg` in the current implementation) before parsing
	- **Star:** similarly, check for the presence of `arg2`; IVG-1/2 must use the single CSV list and IVG-3 must use the new pair syntax
	- **Gradients:** modify `GradientSpec` to accept the format version and enforce that IVG-3 uses two coordinate pairs separated by spaces while IVG-1/2 require the CSV form handled by the absence of `arg2`
	- Ensure `parsePaintOfType` passes the format version when creating a `GradientSpec`.
	- Run `timeout 600 ./build.sh` once more and confirm all tests pass.
- [ ] Add or update regression tests
	- Use a test-driven workflow: write failing tests first, then implement the checks to make them pass.
	- For each format version (including no format), add tests for every instruction that should succeed and for every instruction that should fail (e.g., `TEXT` in IVG-1, `LINE` in IVG-2, `PATH` when no format is specified, etc.).
	- Add positive and negative tests verifying the required syntax for `ELLIPSE`, `STAR`, and gradients across versions.
	- Run `timeout 600 ./build.sh` to run the full test suite.

## Testing
- ⚠️ `timeout 600 ./build.sh` (to be run after each stage above; not executed yet)

## Notes
- When implementing, maintain tab indentation and other style rules from `AGENTS.md`.
- Ensure any new `.sh`/`.cmd` scripts created for tests follow repository portability guidelines.
