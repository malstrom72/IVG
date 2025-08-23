# PolygonMask Rasterizer

`PolygonMask` converts vector paths into a row-oriented coverage mask using a scanline algorithm. It keeps active edges per row, writes **coverage slope deltas** at integer x boundaries, and then integrates once per row to produce mask pixels.

## Pseudo-code

```
for each row y:
    advance/activate segments
    write boundary deltas
    integrate deltas → coverage
    apply fill rule → pixels
```

## Core idea: delta coverage (slope-delta buffer)

- We do **not** store per-pixel coverage directly.  
- `coverageDelta[i]` stores the **change in coverage slope** at boundary `i`.  
- A left-to-right pass performs two implicit integrations:
	1) accumulate slope: `slope += coverageDelta[i]`
	2) accumulate area (coverage): `cover += slope`
- This makes each edge write O(1) work per **boundary** it crosses, and the resolve pass O(width).

## Data & coordinate system

- Fixed point:
	- `FRACT_BITS`: subpixel fraction bits for x/y.
	- `COVERAGE_BITS`: fixed-point precision for coverage.
- Per edge segment (`Segment`):
	- Vertical extent `[topY, bottomY)` in fixed point.
	- Current x at `currentY`: `x` (fixed 32.32).
	- Horizontal slope per unit y: `dx`.
	- Per-column coverage step for this row: `coverageByX` (signed).
	- Row span in columns: `leftEdge`..`rightEdge` (**rightEdge is one-past**).
- Lists:
	- `segsVertically`: by `topY` (activation order).
	- `segsHorizontally`: by `leftEdge` (x order for the current row).

- Row buffer:
	- `coverageDelta`: length `min(bounds.width + 1, MAX_RENDER_LENGTH + 1)` with guard element at index `length`.

## Row pipeline (per call to `render(x, y, length, output)`)

1) **Clip** the requested `[x, x+length)` span and vertical row `y` to `bounds`. Emit transparent for clipped parts.

2) **Advance** to row `y`:
	- If `y < row`: `rewind()` (restart).
	- If `y > row`: step previously engaged segments forward in y using `dx`; update `currentY` and `x`.

3) **Activate** segments whose `topY < rowFixed + FRACT_ONE`. Merge them into `segsHorizontally` keeping x order (in-place merge).

4) **Per-segment deposit into `coverageDelta`** (delta-coverage method):

	- Compute how much of the segment participates in this row:
		- Full row: `dy = FRACT_ONE`; partial first/last row: `dy < FRACT_ONE`.
		- `dx_row = dy * dx` (fixed-point).
		- `remaining`: signed total row area for this segment:
			- full row: `±(1 << (COVERAGE_BITS + FRACT_BITS))`
			- partial row: `±(1 << COVERAGE_BITS) * dy`

	- Map endpoints to the scanline:
		- `leftX = high32(x)`, `rightX = high32(x + dx_row)`; ensure `leftX <= rightX`.
		- Columns: `leftCol = (leftX >> FRACT_BITS) - x0`, `rightCol = (rightX >> FRACT_BITS) - x0`.
		- Subpixels: `leftSub = leftX & FRACT_MASK`, `rightSub = rightX & FRACT_MASK`.

	- Cases:

		- **Entirely right of buffer**: set `leftEdge = rightEdge = length`, no writes.

		- **Entirely left of buffer**: set edges to 0 and dump all signed area at boundary 0:
			```
			coverageDelta[0] += remaining
			```

		- **Single column** (`leftCol == rightCol`): split the signed area between boundaries `col` and `col+1` using subpixel weights (trapezoid):
			```
			coverage = (2*FRACT_ONE - leftSub - rightSub) * remaining >> (FRACT_BITS+1)
			coverageDelta[col+0] += coverage
			coverageDelta[col+1] += remaining - coverage
			seg->leftEdge  = col
			seg->rightEdge = col + 1
			```

		- **Multiple columns**:

			- **Left edge**:
				- If `leftCol < 0` (enters from clip-left): accumulate all covered area up to boundary 0:
					```
					covered  = (min(rightCol, 0) - leftCol) * coverageByX
					covered -= (leftSub * coverageByX) >> FRACT_BITS
					coverageDelta[0] += covered
					leftCol = 0
					seg->leftEdge = 0
					```
				- Else (left edge inside buffer): deposit left partial column with trapezoid split:
					```
					lx       = FRACT_ONE - leftSub
					covered  = (lx * coverageByX) >> FRACT_BITS
					coverage = (lx * covered) >> (FRACT_BITS+1)
					coverageDelta[leftCol+0] += coverage
					coverageDelta[leftCol+1] += covered - coverage
					seg->leftEdge = leftCol
					++leftCol
					```

		- **Interior columns** (uniform slope across pixels):
			```
			colCount = rightCol - leftCol
			if (colCount > 0) {
				coverageDelta[leftCol + 0] += (coverageByX >> 1)
				end = min(leftCol + colCount, length)
				for (int col = leftCol + 1; col < end; ++col) {
					coverageDelta[col] += coverageByX
				}
				coverageDelta[end] += coverageByX - (coverageByX >> 1)
			}
			```
			This is the trapezoidal ½, 1, …, 1, ½ pattern in **slope-delta** form.

			- **Right edge** (if inside buffer): spend the remaining area in the right partial column:
				```
				remaining -= covered + colCount * coverageByX
				coverage = (2*FRACT_ONE - rightSub) * remaining >> (FRACT_BITS+1)
				coverageDelta[rightCol+0] += coverage
				coverageDelta[rightCol+1] += remaining - coverage
				seg->rightEdge = rightCol + 1  // one-past
				```
			- Else if the right edge is beyond the buffer:
				```
				seg->rightEdge = length
				```

	- Retire segments whose `bottomY <= rowFixed`: mark with sentinel and swap out of `segsVertically`:
		```
		seg->leftEdge = -0x7FFFFFFF  // INT_MIN sentinel
		```

5) **Cleanup and x-order**:
	- Remove retired entries from `segsHorizontally` and reinsert the rest by `leftEdge` (stable “push-down”).
	- Update `engagedStart` to first unretired index.

6) **Resolve deltas → pixels** (integration + fill rule):
	- Maintain `coverageAcc` = current **slope-integrated** coverage.
	- Walk the row in two kinds of spans:

		- **Flat region** up to next `leftEdge`:
			```
			coverageAcc += coverageDelta[col]
			fillRule.processCoverage(1, &coverageAcc, &pixel)
			coverageDelta[col] = 0
			output.addSolid(runLength, pixel)
			```

		- **Edge span** from current `leftEdge` to the merged `rightEdge`:
			```
			nx = segsHorizontally[integrateIndex]->rightEdge
			while (integrateIndex + 1 < engagedEnd
					&& nx + 4 >= segsHorizontally[integrateIndex + 1]->leftEdge) {
				++integrateIndex
				nx = max(segsHorizontally[integrateIndex]->rightEdge, nx)
			}
			++integrateIndex
			spanLength = nx - col
			for (int i = 0; i < spanLength; ++i) {
				coverageAcc += coverageDelta[col + i]
				coverageDelta[col + i] = coverageAcc  // reuse as area buffer for this span
			}
			pixels = output.addVariable(spanLength, false)
			fillRule.processCoverage(spanLength, &coverageDelta[col], pixels)
			for (int i = 0; i < spanLength; ++i) {
				coverageDelta[col + i] = 0
			}
			```

	- Clear guard each row:
		```
		coverageDelta[length] = 0
		```

	- Emit right-side clip as transparent if needed.

## Fill rules

- **Non-zero** and **even-odd** rules map the signed integrated coverage to `Mask8::Pixel` via:
	```
	fillRule.processCoverage(count, sourceCoverage, outPixels)
	```

## Clipping

- **Horizontal**: early-out for fully outside; left/right splits emit transparent runs. Left/right entry/exit are handled by which boundary indices receive the deltas (or none).
- **Vertical**: segments are active for rows `[topY, bottomY)`. Partial top/bottom rows scale `dy` and `dx_row`.

## Sentinels & bookkeeping

- Retired segment sentinel:
	```
	seg->leftEdge = -0x7FFFFFFF
	```
- `rightEdge` is **one-past** the last covered column.
- Guard element `coverageDelta[length]` is cleared each row to prevent slope leakage.
- `paintedBounds` is updated in debug builds to assert the rasterizer stayed within `bounds`.

## Complexity & behavior

- Per row:
	- Edge deposits: O(#boundary crossings) across active segments.
	- Resolve pass: O(length).
- Area correctness:
	- For straight edges within a row, cumulative area vs. x is piecewise linear; its derivative is piecewise constant. Storing **slope changes** at boundaries and integrating yields correct area, including subpixel endpoint handling (trapezoidal splits).
- Signed arithmetic supports winding: opposite-direction edges subtract.

## Useful invariants (sanity checks)

- After all segment writes in a row:
	```
	sum_{i=0..length} coverageDelta[i] == 0  // slope returns to baseline
	```
- The integrated coverage over the row equals the sum of each segment’s signed `remaining`, clamped to the clipped window.
- `engagedStart <= engagedEnd`, and active segments in `segsHorizontally[engagedStart..engagedEnd)` are ordered by `leftEdge` after the reorder step.
