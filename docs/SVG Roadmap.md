# SVG Feature Roadmap

This document outlines a path for expanding `svg2ivg.js` to handle more of the SVG files under `tests/svg`.
Each subsection describes the missing capability and proposes implementation steps.

## Geometry Elements


## Text and Images

### `text`
1. Read text content and attributes such as `x`, `y`, and `font-size`.
2. Map characters to drawing commands (e.g. using built-in fonts or an external glyph source).
3. Emit IVG text or converted paths positioned at the specified coordinates.

### `image`
1. Decode `href` data URIs or external references.
2. Create an IVG image command that embeds raster data with `width` and `height` scaling.
3. Handle positioning attributes (`x`, `y`) and clipping to the viewport.

## Definitions and Reuse

### `defs` / `use`
1. While parsing, store elements with `id` inside a definitions table.
2. Implement a `use` converter that clones a referenced element, applies `x`/`y` offsets, and processes it normally.
3. Support nested `use` instances and inherited presentation attributes.

### Gradients
1. Parse `<linearGradient>` and `<radialGradient>` definitions, recording their stops and coordinates.
2. When a `fill` or `stroke` uses `url(#id)`, look up the gradient and emit the corresponding IVG gradient commands.
3. Handle gradient units, spread methods, and transformation attributes.

