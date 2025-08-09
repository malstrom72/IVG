# svg2ivg Test Cases

## Running the Converter
Build PikaScript via `timeout 180 ./build.sh` from the repository root, then run the converter from its folder so `xmlMini.ppeg` can be found:

```
cd tools/svg2ivg
../../externals/PikaScript/output/PikaCmd svg2ivg.pika ../../tests/svg/supported/circle.svg
```

Process all samples in one go:

```
cd tools/svg2ivg
for f in ../../tests/svg/*/*.svg; do
	../../externals/PikaScript/output/PikaCmd svg2ivg.pika "$f" >/dev/null
done
```

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
