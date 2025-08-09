# SVG Support in svg2ivg

## Supported SVG Features

### Elements

-   `svg` root element with `width`, `height` (pixel units only) and `viewBox` for basic scaling/offset.
-   `g` groups, emitted as `context` blocks to propagate presentation attributes.
-   Shapes: `path` (`d`), `circle` (`cx`, `cy`, `r`), `ellipse` (`cx`, `cy`, `rx`, `ry`), `line` (`x1`, `y1`, `x2`, `y2` as a path), and `rect` (`x`, `y`, `width`, `height`, optional `rx`/`ry` rounded corners).

### Presentation Attributes

-   `stroke`, `stroke-width`, `stroke-linejoin`, `stroke-linecap`, `stroke-miterlimit`, `fill`.
-   `stroke-linejoin` values: `bevel`, `round`, `miter`, `miter-clip`, `arcs`.
-   `stroke-linecap` values: `butt`, `round`, `square`.

### Colors

-   Hex color literals `#rrggbb` or `#rgb`.
-   Named colors from the CSS/SVG color list (e.g. `lightblue`, `darkgreen`).
-   `none` to disable stroke or fill.

## Unsupported or Partial Features

-   Additional SVG elements (`polygon`, `polyline`, `text`, `defs`, `use`, `image`, `clipPath`, `mask`, `linearGradient`, etc.).
-   Transform attributes (`transform` on any element) and `preserveAspectRatio` handling.
-   Units other than plain numbers or `px`; percentage values are not handled.
-   Presentation attributes such as `stroke-dasharray`, `stroke-dashoffset`, `stroke-opacity`, `fill-opacity`, or `style`/`class` based styling.
-   Color functions like `rgb()`, `rgba()`, `hsl()`, or gradients/pattern fills.
-   Global or per-element `opacity`.
-   Any `viewBox` behavior beyond a simple uniform scale and top-left offset.
-   Error recovery for missing attributes—many attributes treated as required even though the SVG spec provides defaults.

This list reflects the current state of `tools/svg2ivg/svg2ivg.pika` and may change as the converter evolves.
