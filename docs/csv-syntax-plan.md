# CSV Syntax Restoration Plan

1. **Line and Polygon primitives**
	- Rework `makeLinePath` to accept one comma-separated list and iterate through coordinate pairs.
	- Require an even number of coordinates; each pair represents a vertex.
	- Refactor `makePolygonPath` to consume a single CSV argument with at least three vertex pairs and close the path automatically.

2. **Ellipse and Star shapes**
	- Simplify `makeEllipsePath` so it expects one CSV argument `cx,cy,rx[,ry]`, dropping format-version checks and the optional second argument.
	- Apply the same pattern to `makeStarPath`, parsing all numeric parameters from a single argument and removing `formatVersion` handling.

3. **PATH instruction block**
	- Switch segment commands to parse numbers from one CSV argument:
		- `LINE_TO` – iterate through a list of coordinate pairs; verify an even count.
		- `BEZIER_TO` – accept four or six numbers (quadratic vs. cubic) from one list.
		- `ARC_TO` – read endpoint and radii from one list of three or four numbers; keep flags as labeled args.
		- `ARC_SWEEP` – combine center and sweep degrees into one list of three numbers.
	- Update `PATH_ELLIPSE` and `PATH_STAR` call sites to use the simplified helpers.

4. **Top-level instructions**
	- Remove version-specific argument splitting for `ELLIPSE` and `STAR`; all numeric data comes via one CSV argument.
	- `LINE` and `POLYGON` already delegate to the helpers; the helper changes enforce the CSV-based, even-argument syntax.

5. **Conversion tools**
	- Adjust `svg2ivg.js` converters to emit comma-separated coordinates:
		- Circle/Ellipse converters output `ELLIPSE cx,cy,r` or `ELLIPSE cx,cy,rx,ry`.
		- Line/polyline and polygon converters join points with commas.

6. **Documentation and tests**
	- Update README, docs, and sample/test `.ivg` files to demonstrate the CSV syntax (e.g., `ELLIPSE 150,100,80`, `LINE 50,50,350,50`).
	- Revise any tests that check separator validity to match the new expectations.
