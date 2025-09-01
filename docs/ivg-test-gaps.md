# IVG Test Coverage Gaps

This note lists IVG features described in the specification but previously missing or sparsely covered in the regression tests.

- **LINE instruction** – `linePenOptions.ivg` adds a polyline with bevel joints and dash offsets.
- **POLYGON instruction** – `polygonPenOptions.ivg` exercises polygon stroking with dash offsets.
- **Pen options** – the new line and polygon tests use `joints:bevel` and `dash-offset` settings.
- **Define path directive** – `pathStrokePenOptions.ivg` reuses a defined path with different stroke styles.
- **External images** – `externalImageTest.ivg` references an external PNG (currently skipped pending loader support).
