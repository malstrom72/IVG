# resvg Test Compatibility

This document lists which `externals/resvgTests` files the `tools/svg2ivg/svg2ivg.js` converter can handle.

## Should work
- shapes/circle/simple-case.svg
- shapes/line/simple-case.svg
- shapes/polygon/simple-case.svg
- shapes/polyline/simple-case.svg
- shapes/rect/simple-case.svg
- shapes/path/A.svg
- painting/fill/rgb-color.svg
- painting/fill-rule/evenodd.svg
- painting/stroke/none.svg
- painting/stroke-linecap/round.svg
- painting/stroke-opacity/50percent.svg
- painting/fill-opacity/50percent.svg
- paint-servers/linearGradient/default-attributes.svg
- paint-servers/radialGradient/default-attributes.svg
- paint-servers/stop/hsla-color.svg
- paint-servers/stop-opacity/simple-case.svg
- structure/defs/simple-case.svg
- structure/g/deeply-nested-groups.svg
- structure/use/simple-case.svg
- text/font-size/simple-case.svg
- text/text-anchor/middle-on-text.svg

## Should not work
- paint-servers/pattern/simple-case.svg
- painting/stroke/pattern.svg
- painting/stroke-dasharray/ws-separator.svg
- painting/stroke-dashoffset/default.svg
- masking/clipPath/clip-path-with-transform.svg
- masking/mask/simple-case.svg
- structure/a/on-shape.svg
- structure/image/with-transform.svg
- structure/style/type-selector.svg
- structure/symbol/simple-case.svg
- text/font-weight/bold.svg
- text/font-style/italic.svg
- text/textPath/startOffset=30.svg
- filters/feGaussianBlur/simple-case.svg
- filters/feDropShadow/with-offset.svg
- painting/visibility/hidden-on-shape.svg
- painting/stroke-linejoin/arcs.svg

## Might work
- text/tspan/tspan-bbox-1.svg – ivg2svg drops nested `tspan` text and falls back to a smaller default font size
- structure/style-attribute/simple-case.svg
- painting/stroke-miterlimit/valid-value.svg

## Should work with fixes
- pattern fills and strokes
- clipping paths via masks
- mask elements
- image elements
- symbol definitions
- link (`a`) elements treated as groups
- CSS style elements and style attributes
- tspan and nested text nodes
- stroke dash arrays and dash offsets

## IVG capabilities relevant to unsupported tests
- Patterns: IVG has a `pattern` paint type
- Masks: IVG includes a `mask` instruction
- Images: IVG provides an `IMAGE` instruction
- Stroke dashes: IVG pens accept `dash` and `dash-offset`
- Groups: `context`/`define` can model `symbol` or `a`
- Text: multiple `TEXT` instructions can represent `tspan`
- Filters: IVG has no filter primitives, so filters cannot be mapped
- Text along a path requires new IVG features
