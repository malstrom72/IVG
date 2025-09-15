# Plan for Adding "define mask" Support

## Milestone 1: Introduce storage for named masks
- [ ] **Add mask container to executor**
	- In `src/IVG.h` around the existing `PathMap`/`PatternMap` typedefs (~line 383), insert  `typedef std::map<IMPD::WideString, Inheritable<NuXPixels::RLERaster<NuXPixels::Mask8> > > MaskMap;`
	- Declare `MaskMap definedMasks;` with the other member maps and expose `public: const MaskMap& getDefinedMasks() const { return definedMasks; }`
	- Ensure the header includes `NuXPixels/RLERaster.h` if it is not already present.
- [ ] Run `timeout 600 ./build.sh`

## Milestone 2: Extend `define` instruction to handle masks
- [ ] **Teach `executeDefine` about masks**
	- In `src/IVG.cpp` function `IVGExecutor::executeDefine` (~line 1283), add an `else if (typeLower == "mask")` branch after the pattern case.
	- Parse arguments:
		`const WideString name = impd.unescapeToWide(args.fetchRequired(1, true));`
		`const String& definition = args.fetchRequired(2, false);`
		`bool inverted = impd.fetchOptionalBool("inverted", false);`
		`args.throwIfAnyUnfetched();`
	- Guard against duplicate names with a runtime error if `definedMasks.find(name) != definedMasks.end()`.
	- Create the mask raster using the same setup as `MASK_INSTRUCTION`:
		instantiate `MaskMakerCanvas` and `Context`, reset state (`pen`, `fill`, `textStyle`, `evenOddFillRule`),
		run `runInNewContext(impd, maskContext, definition);`
		store result `definedMasks[name] = maskMaker.finish(inverted);`
- [ ] Run `timeout 600 ./build.sh`

## Milestone 3: Support named masks in `MASK_INSTRUCTION`
- [ ] **Reference previously defined masks**
        - In `src/IVG.cpp` case `MASK_INSTRUCTION` (~line 1890), replace first argument handling to check whether it starts with '['. If it does, retain current block behaviour.
        - Otherwise treat the argument as a mask name:
                `WideString name = impd.unescapeToWide(firstArg);`
                lookup `definedMasks`; throw runtime error if missing;
                parse optional `inverted` flag;
                assign mask with pipelining: `currentContext->accessState().mask = (inverted ? ~(*it->second) : *it->second);`
- [ ] **Add `mask reset` command**
       - In the same `MASK_INSTRUCTION` case, handle the `reset` argument to clear `currentContext->accessState().mask` and reject `inverted`.
- [ ] Run `timeout 600 ./build.sh`

## Milestone 4: Document `define mask` feature
- [ ] **Update user documentation**
	- In `docs/IVG Documentation.md` after the `define pattern` section, add a `define mask` subsection describing syntax `define mask <name> [inverted] { ... }`, referencing via `mask <name>` and `mask <name> inverted`, plus a short example.
	- Regenerate `docs/IVG Documentation.html` by running `bash tools/updateDocumentation.sh`.
- [ ] Run `timeout 600 ./build.sh`

## Milestone 5: Add regression tests for reusable masks
- [ ] **Create regression test**
	- Add `tests/ivg/defineMaskTest.ivg` that defines a mask (e.g. `starMask`), applies it normally and with `inverted`.
	- Add expected output images/baselines so the test harness can verify results.
- [ ] Run `timeout 600 ./build.sh`
