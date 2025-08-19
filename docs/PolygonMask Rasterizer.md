# PolygonMask Rasterizer

`PolygonMask` converts vector paths into a row oriented coverage mask.  The implementation follows a classic scanline algorithm tuned for incremental rendering.

## Design philosophy

* **Edge segmentation** – Each path edge is clipped and represented as a vertical segment with sub‑pixel precision.
* **Active edge lists** – Segments are kept in two arrays: one sorted by starting `y` for activation and another sorted by `x` while scanning.
* **Coverage accumulation** – As each row is rasterized, segments contribute signed coverage deltas that are integrated into a span buffer.
* **Fill rules** – The accumulated coverage is interpreted according to either the non‑zero or even‑odd rule to produce mask pixels.
* **Sequential rendering with rewind** – Rows are expected in order but the state can be reset with `rewind()` when random access is needed.

## Pseudo‑code

```text
build segments from path
sort segments by top y
for each requested row y:
    activate segments whose top <= y < bottom
    sort active segments by x
    for each segment pair:
        accumulate coverage across the row
    integrate coverage deltas
    apply fill rule to emit mask pixels
```

The real implementation uses fixed‑point math and optimizes the merging and integration steps to minimize work per row.
