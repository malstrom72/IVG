# Mask behavior update implementation plan

## Requested behavior
- Mask segments should always multiply with the active mask instead of replacing it. `mask a; mask b; mask c inverted:yes` must evaluate to `a * b * ~c` while respecting the mask inherited from outer contexts.
- `mask invert` must flip the current mask relative to the mask that was active when the surrounding context started: `newMask = contextBaseline * ~currentMask`.
- `mask reset` must restore the mask to the value that was active when the surrounding context started.

## Implementation steps
- [ ] Adjust `MaskMakerCanvas::finish` (and its single caller in `MASK_INSTRUCTION`) so mask blocks always retrieve the raw segment coverage; do not apply inversion inside `finish` because the caller must decide how to combine the segment with any outer mask.
- [ ] Update `MASK_INSTRUCTION` in `src/IVG.cpp` to recognize the `mask invert` and `mask reset` forms before trying to interpret the block argument, so the interpreter can execute the new commands without creating a mask block.
- [ ] When executing a `mask { ... }` block, snapshot `currentContext->accessState().mask` before running the block, run the block in a `MaskMakerCanvas`, obtain the segment raster, and then:
	- multiply the segment with the inherited mask when no inversion is requested (`outerMask == nullptr` should hand back the segment directly);
	- otherwise build `outerMask * ~segmentMask`, cloning `outerMask` on demand so the inherited raster is never mutated.
- [ ] Extend `Context` with helpers to expose and restore the mask captured at context creation (for example `const RLERaster<Mask8>* getInitialMask() const` and `void restoreInitialMask()`), and use them so `mask invert` and `mask reset` can respect the context baseline without duplicating logic.
- [ ] Implement `mask invert` so it clones the context-baseline mask (or synthesizes an "all visible" raster when no baseline mask exists), multiplies it with the inverse of the current mask, and replaces the active mask with the result.
- [ ] Implement `mask reset` so it restores `currentContext->accessState().mask` to the baseline mask captured when the context began, dropping any overrides produced by nested mask operations.
- [ ] Document the new always-multiplying semantics plus the `mask invert` and `mask reset` commands in `docs/IVG Documentation.md`, including examples that cover nested mask chains.
- [ ] Expand regression coverage in `tests/` so nested mask chains exercise the new semantics, and add dedicated tests for `mask invert` and `mask reset` before running `timeout 600 ./build.sh`.
