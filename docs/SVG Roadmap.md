# SVG Feature Roadmap

This document outlines a path for expanding `svg2ivg.js` to handle more of the SVG files under `tests/svg`.
Each subsection describes the missing capability, implementation notes, and sample tests from
`externals/resvgTests`.

## Feature Rankings

The lists below prioritize upcoming tasks for `svg2ivg.js`.

### Importance (highest first)
1. CSS Styles and Classes
2. Mask Elements

### Difficulty (easiest first)
1. Mask Elements
2. CSS Styles and Classes

## Styling and Paint

### CSS Styles and Classes
1. Parse `<style>` elements and build a style map.
2. Merge rules referenced by `class` attributes before conversion.

Sample: `externals/resvgTests/structure/style/class-selector.svg`

## Masking

### Mask Elements
1. Parse `<mask>` definitions and record them in a lookup table.
2. Apply masks when elements reference the `mask` attribute.

Sample: `externals/resvgTests/masking/mask/mask-on-group.svg`

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

## Completed Features

- Additional Unit Types (`em`, `rem`, `vh`, `vw`, `vmin`, `vmax`, `ex`)
- `currentColor` and `inherit`
- `clipPathUnits="objectBoundingBox"`
- Marker Elements

