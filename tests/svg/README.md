# svg2ivg Test Cases

## Supported SVGs
- `circle.svg`: basic circle
- `rect.svg`: rectangle outline
- `ellipse.svg`: filled ellipse
- `line.svg`: single line segment
- `path.svg`: closed triangle path
- `group.svg`: group with shared attributes
- `color-names.svg`: named color fills
- `stroke-fill.svg`: stroked quadratic curve
- `viewbox.svg`: viewBox scaling
- `multi-path.svg`: multiple horizontal segments

## Unsupported or Problematic SVGs
- `polygon.svg`: polygon element
- `polyline.svg`: polyline element
- `text.svg`: text element
- `image.svg`: embedded image
- `transform.svg`: transform attribute
- `units-cm.svg`: non-pixel units
- `percentage.svg`: percentage units
- `defs-use.svg`: defs/use elements
- `gradient.svg`: gradient fill
- `opacity.svg`: opacity attribute
