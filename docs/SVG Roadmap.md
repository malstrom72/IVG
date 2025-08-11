# SVG Feature Roadmap

This document outlines a path for expanding `svg2ivg.js` to handle more of the SVG files under `tests/svg`.
Each subsection describes the missing capability and proposes implementation steps.

## Geometry Elements


## Text and Images

### `image`
1. Decode `href` data URIs or external references.
2. Create an IVG image command that embeds raster data with `width` and `height` scaling.
3. Handle positioning attributes (`x`, `y`) and clipping to the viewport.

### Gradients
1. Parse `<linearGradient>` and `<radialGradient>` definitions, recording their stops and coordinates.
2. When a `fill` or `stroke` uses `url(#id)`, look up the gradient and emit the corresponding IVG gradient commands.
3. Handle gradient units, spread methods, and transformation attributes.

