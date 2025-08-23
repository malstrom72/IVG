# NuXPixels

## Table of Contents
- [Intro](#intro)
- [Quickstart](#quickstart)
- [Data types](#data-types)
  - [Points and Rectangles](#points-and-rectangles)
  - [Colors](#colors)
  - [AffineTransformation](#affinetransformation)
- [Rendering pipeline](#rendering-pipeline)
  - [Raster and Renderer](#raster-and-renderer)
  - [PolygonMask](#polygonmask)
  - [Gradients](#gradients)
  - [Solid and Texture](#solid-and-texture)
  - [RLERaster](#rleraster)
  - [Operator overloading](#operator-overloading)
  - [Pull model](#pull-model)
  - [Path construction](#path-construction)
  - [Limits and Safety](#limits-and-safety)
  - [Examples](#examples)
## Intro

_NuXPixels_ is a small C++ library for 2D graphics rendering. It is designed to be self contained with no operating system dependencies and focuses on a minimal API with high quality anti‑aliasing. The renderer operates entirely in memory.

## Quickstart

```cpp
#include "NuXPixels.h"
using namespace NuXPixels;

int main() {
	SelfContainedRaster<ARGB32> canvas(IntRect(0, 0, 64, 64));
	canvas |= Solid<ARGB32>(0xFFFF0000); // red
	FILE* f = fopen("out.raw", "wb");
	fwrite(canvas.getPixelPointer(), sizeof(ARGB32::Pixel), 64 * 64, f);
	fclose(f);
	return 0;
}
```

Compile with `g++ example.cpp -std=c++17` (or a similar C++17 compiler command).

## Data types

### Points and Rectangles
`NuXPixels` defines generic `Point<T>` and `Rect<T>` templates for integer and floating point coordinates. Convenience typedefs such as `IntPoint`, `IntRect` and `Vertex` (a double precision point) are available for common use.

`Rect<T>` uses half‑open bounds `[left, left+width) × [top, top+height)` and expects non‑negative `width` and `height`. When rectangles only touch at an edge the intersection is empty. Coordinates are 31‑bit signed integers; `FULL_RECT` spans ±0x40000000. `Rect<T>` provides helpers like `offset`, `calcUnion` and `calcIntersection` to manipulate regions.
These operations simplify clipping logic:

```cpp
IntRect a(0, 0, 50, 50);
IntRect b(20, 20, 10, 10);
IntRect clipped = a.calcIntersection(b);
```

### Colors
The library ships with a few pixel formats. `ARGB32` stores premultiplied 8‑bit channels in `0xAARRGGBB` order (little‑endian machines lay the bytes out as BB GG RR AA). Blending follows the conventional `dst*(255-srcA)/255 + src` equation. Colors are assumed sRGB and all computations occur on premultiplied channels in that gamma space; there is no implicit linearization. Intermediate arithmetic uses integer math with truncation, so channel multiplications are rounded toward zero. A lightweight `Mask8` type is used when rendering coverage masks.

`ARGB32` also includes `multiply` and `interpolate` utilities for pixel arithmetic. A color can be constructed from floats and then modulated:

```cpp
ARGB32::Pixel p = ARGB32::fromFloatRGB(1.0, 0.0, 0.0, 0.5); // 50% red
ARGB32::Pixel dark = ARGB32::multiply(p, 128);               // scale alpha & rgb by 128/255
// If using a literal, red opaque is 0xFFFF0000, blue is 0xFF0000FF.
```

### AffineTransformation
`AffineTransformation` represents 2×3 matrices for translation, scaling, rotation and shearing. Transformations can be composed with the `transform()` function and applied to vertices or paths.

Chained calls build complex transforms in a readable manner:

```cpp
AffineTransformation t = AffineTransformation()
	.translate(20, 10)
	.scale(2)
	.rotate(0.25 * M_PI);
```

Transforms are pre‑multiplied (points are column vectors), so calls execute left‑to‑right: `translate().scale().rotate()` means translate, then scale, then rotate in object space. Angles are specified in radians.

## Rendering pipeline

### Raster and Renderer
All drawing is expressed through `Renderer` templates that generate spans of
pixel data. A span models a contiguous horizontal run. When marked *solid* the
span stores one pixel value repeated for the entire run; otherwise it carries an
array of per-pixel values, allowing coverage or color to vary. Spans longer than
`MAX_RENDER_LENGTH` are split automatically. `Raster<T>` collects the result in a client supplied buffer while `SelfContainedRaster<T>` manages its own memory. A typical pipeline blends color data from `Renderer<ARGB32>` through coverage masks produced by `Renderer<Mask8>` sources.

```cpp
ARGB32::Pixel pixels[256 * 256];
Raster<ARGB32> view(pixels, 256, IntRect(0, 0, 256, 256), false);
```

### PolygonMask
`PolygonMask` is the workhorse renderer for filling paths. Given a vector `Path`
and an optional fill rule, it converts the geometry into a scanline coverage mask
(`Renderer<Mask8>`). Paint sources such as solid colors or gradients are then
blended through this mask onto the destination raster. It powers path filling and
stroking, processing scanlines sequentially from top to bottom. Coverage values
range 0–255 from an 8×8 area-sampling kernel. Both even‑odd and non‑zero winding
rules are exact. The constructor also accepts an optional clip rectangle
(defaulting to `FULL_RECT`); the rectangle is clamped to the maximum coordinate
range supported by the rasterizer and applied before rasterization.

Although designed for sequential rendering, a mask can be rewound to its initial
state with `PolygonMask::rewind()`. Random access is possible by invoking
`rewind()` whenever a lower row needs to be revisited, after which rendering can
continue from any scanline. Requests outside the mask's clipped bounds simply
yield transparent coverage without rewinding. Rewinding re‑sorts segments and
clears internal buffers; its cost grows with the number of path edges, so jumping
around freely is slower than processing rows in order.

Further details on the algorithm, design trade‑offs and pseudo‑code are available in `PolygonMask Rasterizer.md`.

### Gradients
A `Gradient` lookup table produces color values for linear or radial fills.
`LinearAscend` and `RadialAscend` generate a 0–255 coverage ramp that can index
into a gradient: `gradient[LinearAscend(x0, y0, x1, y1)]` or
`gradient[RadialAscend(cx, cy, rx, ry)]` yield a color renderer. The library also
provides a `GammaTable` for simple tone adjustments.

Stops are sorted by position and interpolated in premultiplied sRGB space. The ramp spans indices `0–255` inclusive and clamps at the ends; gradients neither repeat nor reflect. Large color steps may band since no dithering is applied.

```cpp
Gradient<ARGB32>::Stop stops[] = {
	{0.0, 0xFF0000FF},// blue
	{1.0, 0xFFFFFFFF}// white
};
Gradient<ARGB32> grad(2, stops);
canvas |= grad[LinearAscend(0, 0, 0, 100)];

Gradient<ARGB32>::Stop stops2[] = {
	{0.0, 0xFFFF0000},// red
	{1.0, 0xFFFFFFFF}
};
```

> **Warning:** `Gradient::operator[]` keeps a reference to the mask renderer. Passing a temporary
> `LinearAscend` or `RadialAscend` is only safe if the lookup is consumed within the same statement.
> To reuse the lookup later, store the ramp separately so it remains alive:
>
> ```cpp
> LinearAscend ramp(x0, y0, x1, y1);
> Lookup<ARGB32, LookupTable<ARGB32> > renderer = grad[ramp];
> canvas |= renderer; /// `ramp` must outlive `renderer`
> ```
>
> See [Lifetime of renderers](#lifetime-of-renderers) for the general rule that
> all NuXPixels expressions hold references to their components.

### Solid and Texture
`Solid<T>` outputs a constant pixel value. `Texture<T>` samples from a raster using an affine
transformation and optional wrapping (repeat when `wrap=true`, otherwise clamp to transparent black). Sampling uses 16.16 fixed-point coordinates with bilinear filtering.

```cpp
Texture<ARGB32> tex(image, true, AffineTransformation().scale(0.5));
canvas |= tex * mask; // Defaults: bilinear filter, wrap repeat
// Clamp with wrap=false to sample transparent black; sampling happens in premultiplied space.
```

### RLERaster
`RLERaster<T>` stores spans in run-length encoded form for reuse. It is handy for caching
masks so that complex paths need not be rasterized repeatedly.

```cpp
RLERaster<Mask8> cache(area, mask);
canvas |= Solid<ARGB32>(color) * cache;
```

Runs encode either a solid pixel or a block of per‑pixel data. Each span stores a 16‑bit header plus optional pixel payload, preserving partial alpha. Memory footprint is typically 20–60% of an uncompressed raster depending on image coherence.

### Operator overloading
Renderers can be combined with `*`, `+`, `|`, `+=`, `*=`, and `|=` operators.
Each operator returns another `Renderer` that lazily requests spans from its
inputs. An expression like `canvas |= Solid<ARGB32>(color) * mask` forms a small
pipeline. Operator precedence follows C++ rules: `*` binds tighter than `|`. The
`|` operator blends the right renderer over the left; it is not a bitwise OR.

### Pull model
As the canvas renders, it pulls spans from the expression and each stage only
computes what the next stage requires. Because drawing is demand driven,
NuXPixels can optimize away work in real time. Opaque spans automatically block
processing of any renderers beneath them since those pixels are invisible. This
culling happens per span and keeps the renderer efficient even with many
layers.

### Lifetime of renderers
Most renderer types store references to the objects passed into their
constructors or operators. C++ destroys temporary objects at the end of the
statement, so a renderer built from temporaries must also be used in that same
statement. To keep a renderer for later, create and store every component
separately so their lifetimes extend as needed.

```cpp
Gradient<ARGB32>::Stop stops[] = {{0.0, 0xff0000ff}, {1.0, 0xffffffff}};
Gradient<ARGB32> grad(2, stops);
LinearAscend ramp(x0, y0, x1, y1);
Lookup<ARGB32, LookupTable<ARGB32> > lookup = grad[ramp];
canvas |= lookup; /// `grad` and `ramp` must outlive `lookup`

canvas |= Gradient<ARGB32>(2, stops)[LinearAscend(x0, y0, x1, y1)]; /// safe: everything is temporary
```

This rule applies to all expressions in NuXPixels—`PolygonMask`, `Texture`,
`Solid`, gradients and more. Either chain the full expression in a single
statement or keep each renderer alive for as long as any derived renderer uses
it.

Even types that are trivially copyable still reference external data; treat renderers as views rather than owning objects.

| Renderer | References | Ownership |
|---|---|---|
| Solid<T> | none | value |
| Texture<T> | source `Raster<T>` | no |
| PolygonMask | `Path`, `FillRule` | no |
| Gradient<T> | copy of stops | table |
| Gradient<T>::Lookup | gradient, ramp | no |
| RLERaster<T> | none | owns span/pixel arrays |

## Path construction
`Path` is a sequence of drawing commands supporting lines, quadratic and cubic curves. It can be modified with helper methods like `addRect`, `addEllipse`, `addRoundedRect` and `stroke`. Paths operate in double precision and can be transformed with an `AffineTransformation` before rendering.

```cpp
Path star;
star.addStar(40, 40, 5, 20, 10, 0);
star.stroke(3.0, Path::ROUND, Path::MITER);
star.dash(5.0, 2.0);
```

`stroke(width, endCaps, joints, miterLimit, curveQuality)` defaults to a miter limit of `2.0`. `EndCapStyle` may be `BUTT`, `ROUND`, or `SQUARE`; `JointStyle` is `BEVEL`, `CURVE`, or `MITER`. `dash(dashLength, gapLength, dashOffset)` alternates drawn and skipped segments starting at `dashOffset`; fractional lengths accumulate along each sub-path and the pattern resets at every `moveTo`.

## Examples

A short example shows how the operator overloads work together:

```cpp
using namespace NuXPixels;

SelfContainedRaster<ARGB32> canvas(IntRect(0, 0, 256, 256));

Path rect;
rect.addRect(IntRect(50, 50, 150, 150));
PolygonMask mask(rect, canvas.calcBounds());

// Multiply the coverage mask with a solid color and alpha-blend onto the canvas
canvas |= Solid<ARGB32>(ARGB32::fromFloatRGB(1.0, 0.0, 0.0, 0.8)) * mask;
```
Here `*` multiplies the color renderer with the `PolygonMask`, producing
`Renderer<ARGB32>` spans masked by the polygon coverage. The resulting renderer
is then blended onto `canvas` with `|=`. The entire expression becomes a pull
pipeline: the canvas requests pixels, which asks the mask for coverage, which in
turn iterates the path only for the visible spans. Similar expressions can chain
gradients or multiple masks together.

Another example uses a texture and gradient:

```cpp
SelfContainedRaster<ARGB32> texCanvas(IntRect(0, 0, 64, 64));
Gradient<ARGB32> grad(ARGB32::transparent(), 0xFF00FF00); // 0xAARRGGBB: green
texCanvas |= grad[RadialAscend(32, 32, 32, 32)];

Texture<ARGB32> tex(texCanvas, true);
canvas |= tex * mask;
```

## Limits and Safety

- Maximum span length is 256 pixels (`MAX_RENDER_LENGTH`); longer runs are chunked automatically.
- Coordinates are 31‑bit signed; `FULL_RECT` defines the valid range.
- Texture sampling outside the source repeats when `wrap=true` and returns transparent black (`0x00000000`) when `wrap=false`.
- Renderers and rasters are not thread‑safe; rendering distinct instances on separate threads is safe, but do not share one object across threads.
- `PolygonMask::rewind()` re‑sorts segments; cost grows with edge count (≈O(n log n)).
- `RLERaster` memory footprint is roughly 20–60% of raw raster size.
- Paths with fewer than two points or zero-length segments yield zero coverage. Arithmetic uses integer math with truncation; invalid transforms or extreme coordinates can overflow and produce undefined results. Functions do not guarantee `noexcept` and may fail on allocation.
