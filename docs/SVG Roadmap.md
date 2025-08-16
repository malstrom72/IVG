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
4. Marker Elements
5. `clipPathUnits="objectBoundingBox"`

### Difficulty (easiest first)
1. Additional Unit Types
2. `currentColor` and `inherit`
3. `clipPathUnits="objectBoundingBox"`
4. Marker Elements
5. CSS Styles and Classes

## Styling and Paint

### Marker Elements
1. Parse `<marker>` definitions and apply `marker-start`, `marker-mid`, and `marker-end` attributes.
2. Honor `markerUnits`, `viewBox`, and `orient`.

Sample: `externals/resvgTests/painting/marker/marker-on-line.svg`

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

### `<image>` Elements
IVG only references external images via `IMAGE`; it cannot embed raster data. `svg2ivg.js` therefore cannot
convert SVG `<image>` tags that use data URIs or inline content.

### Stroke Dash Arrays
IVG supports only a single dash-gap pair, so `stroke-dasharray` lists with more than two values
cannot be represented in the output.

