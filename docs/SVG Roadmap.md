# SVG Feature Roadmap

This document outlines a path for expanding `svg2ivg.js` to handle more of the SVG files under `tests/svg`.
Each subsection describes the missing capability, implementation notes, and sample tests from
`externals/resvgTests`.

## Feature Rankings

The lists below prioritize upcoming tasks for `svg2ivg.js`.

### Importance (highest first)
1. CSS Styles and Classes
2. Additional Unit Types
3. `currentColor` and `inherit`
4. `image`
5. Marker Elements
6. Stroke Dash Arrays
7. `clipPathUnits="objectBoundingBox"`

### Difficulty (easiest first)
1. Stroke Dash Arrays
2. Additional Unit Types
3. `currentColor` and `inherit`
4. `image`
5. `clipPathUnits="objectBoundingBox"`
6. Marker Elements
7. CSS Styles and Classes

## Text and Images

### `image`
1. Decode `href` data URIs or external references.
2. Create an IVG image command that embeds raster data with `width` and `height` scaling.
3. Handle positioning attributes (`x`, `y`) and clipping to the viewport.

## Styling and Paint

### Marker Elements
1. Parse `<marker>` definitions and apply `marker-start`, `marker-mid`, and `marker-end` attributes.
2. Honor `markerUnits`, `viewBox`, and `orient`.

	Sample: `externals/resvgTests/painting/marker/marker-on-line.svg`

### Stroke Dash Arrays
1. Allow `parseDashArray` to accept any number of dash segments.
2. Emit all segments in the IVG stroke command.

	Sample: `externals/resvgTests/painting/stroke-dasharray/comma-ws-separator.svg`

### CSS Styles and Classes
1. Parse `<style>` elements and build a style map.
2. Merge rules referenced by `class` attributes before conversion.

	Sample: `externals/resvgTests/structure/style/class-selector.svg`

## Units and Colors

### Additional Unit Types
1. Support `em`, `rem`, `vh`, `vw`, and `ex` in `convertUnits`.

	Samples:
	- `externals/resvgTests/shapes/rect/em-values.svg`
	- `externals/resvgTests/shapes/rect/rem-values.svg`
	- `externals/resvgTests/shapes/rect/vw-and-vh-values.svg`

### `currentColor` and `inherit`
1. Resolve `currentColor` and `inherit` in `convertPaint` based on computed color.

	Samples:
	- `externals/resvgTests/painting/fill/currentColor.svg`
	- `externals/resvgTests/painting/color/inherit.svg`

## Clipping

### `clipPathUnits="objectBoundingBox"`
1. Compute bounding boxes for geometry before applying `clip-path`.
2. Scale and offset clip-path contents when `clipPathUnits="objectBoundingBox"` is used.

	Sample: `externals/resvgTests/masking/clipPath/clip-path-with-transform.svg`

## Out of Scope

### Filter Effects
IVG has no filter command today, so `<filter>` elements such as `feGaussianBlur` or `feColorMatrix` cannot be
supported until the IVG format adds filter instructions.

