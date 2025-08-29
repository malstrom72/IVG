# svg2ivg Test Cases

## Running the Converter
Build PikaScript via `timeout 300 ./build.sh` from the repository root, then run the converter from its folder so `xmlMini.ppeg` can be found:

```
cd tools
../externals/PikaScript/output/PikaCmd svg2ivg.pika ../tests/svg/supported/circle.svg
```

Process all samples in one go:

```
cd tools
for f in ../tests/svg/*/*.svg; do
	../externals/PikaScript/output/PikaCmd svg2ivg.pika "$f" >/dev/null
done
```

Or use the Node.js port:

```
cd tools
node svg2ivg.js ../tests/svg/supported/circle.svg
```

Process all samples with Node:

```
cd tools
for f in ../tests/svg/*/*.svg; do
	node svg2ivg.js "$f" >/dev/null
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
- `polygon.svg`: polygon element
- `polyline.svg`: polyline element
- `units.svg`: non-pixel units (`cm`, `mm`, `in`, `pt`, `pc`)
- `percentage.svg`: percentage units
- `transform.svg`: translate, scale, and rotate
- `skew.svg`: skewX and skewY transforms
- `matrix.svg`: matrix transform
- `gradient.svg`: simple linear gradient
- `gradient-stops.svg`: linear gradient with many stops
- `gradient-radial.svg`: radial gradient
- `gradient-transform.svg`: gradient with rotation and scaling
- `defs-use.svg`: defs/use elements
- `opacity.svg`: global and per-element opacity
- `text.svg`: basic text element
- `text-stroke.svg`: text with stroke outline

## Unsupported or Problematic SVGs
- `image.svg`: embedded image
