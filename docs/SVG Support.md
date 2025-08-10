# SVG Support in svg2ivg

## Supported SVG Features

### Elements

-   `svg` root element with `width`, `height` (`px`, `cm`, `mm`, `in`, `pt`, `pc`) and `viewBox` for basic scaling/offset.
-   `g` groups, emitted as `context` blocks to propagate presentation attributes.
-   Shapes: `path` (`d`), `circle` (`cx`, `cy`, `r`), `ellipse` (`cx`, `cy`, `rx`, `ry`), `line` (`x1`, `y1`, `x2`, `y2` as a path), `rect` (`x`, `y`, `width`, `height`, optional `rx`/`ry` rounded corners), `polygon` (`points`), and `polyline` (`points`).
-   Gradient fills and strokes via `linearGradient` and `radialGradient` definitions referenced with `url(#id)`.

### Presentation Attributes

-   `stroke`, `stroke-width`, `stroke-linejoin`, `stroke-linecap`, `stroke-miterlimit`, `fill`.
-   `stroke-linejoin` values: `bevel`, `round`, `miter`, `miter-clip`, `arcs`.
-   `stroke-linecap` values: `butt`, `round`, `square`.

### Transforms

-   `transform` attribute with `translate`, `scale`, `rotate`, `skewX`, `skewY`, and `matrix` operations.

### Colors

-   Hex color literals `#rrggbb` or `#rgb`.
-   Named colors from the CSS/SVG color list (e.g. `lightblue`, `darkgreen`).
-   `none` to disable stroke or fill.

## Unsupported or Partial Features

-   Additional SVG elements (`text`, `defs`, `use`, `image`, `clipPath`, `mask`, etc.).
-   `preserveAspectRatio` handling.
-   Percentage units (e.g. `width="50%"`).
-   Presentation attributes such as `stroke-dasharray`, `stroke-dashoffset`, `stroke-opacity`, `fill-opacity`, or `style`/`class` based styling.
-   Color functions like `rgb()`, `rgba()`, `hsl()`, or pattern fills.
-   Gradient transforms (`gradientTransform`).
-   Global or per-element `opacity`.
-   Any `viewBox` behavior beyond a simple uniform scale and top-left offset.
-   Error recovery for missing attributes—many attributes treated as required even though the SVG spec provides defaults.

This list reflects the current state of `tools/svg2ivg/svg2ivg.pika` and may change as the converter evolves.
