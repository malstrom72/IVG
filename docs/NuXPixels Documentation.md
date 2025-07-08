# NuXPixels

## Table of Contents
- [Intro](#intro)
- [Data types](#data-types)
  - [Points and Rectangles](#points-and-rectangles)
  - [Colors](#colors)
  - [AffineTransformations](#affinetransformations)
- [Rendering pipeline](#rendering-pipeline)
  - [Raster and Renderer](#raster-and-renderer)
  - [Gradients](#gradients)
- [Path construction](#path-construction)
- [Examples](#examples)
## Intro

_NuXPixels_ is a small C++ library for 2D graphics rendering. It is designed to be self contained with no operating system dependencies and is used by the IVG engine to produce pixel rasters. The library focuses on a minimal API with high quality anti‑aliasing and can operate entirely in memory.

## Data types

### Points and Rectangles
`NuXPixels` defines generic `Point<T>` and `Rect<T>` templates for integer and floating point coordinates. Convenience typedefs such as `IntPoint`, `IntRect` and `Vertex` (a double precision point) are available for common use.

### Colors
The library ships with a few pixel formats. `ARGB32` stores premultiplied 8‑bit channels and provides helpers like `isOpaque`, `add`, `blend` and `fromFloatRGB`. A lightweight `Mask8` type is used when rendering coverage masks.

### AffineTransformations
`AffineTransformation` represents 2×3 matrices for translation, scaling, rotation and shearing. Transformations can be composed with the `transform()` function and applied to vertices or paths.

## Rendering pipeline

### Raster and Renderer
All drawing is expressed through `Renderer` templates that generate spans of
pixel data. A span represents a contiguous horizontal run of pixels with identical
coverage or color. `Raster<T>` collects the result in a client supplied buffer
while `SelfContainedRaster<T>` manages its own memory. Canvases in IVG typically
blend color data from `Renderer<ARGB32>` through coverage masks produced by
`Renderer<Mask8>` sources.

### PolygonMask
`PolygonMask` is the workhorse renderer for filling shapes. Given a vector `Path` and an optional fill rule, it converts the geometry into a scanline coverage mask (`Renderer<Mask8>`). Paint sources such as solid colors or gradients are then blended through this mask onto the destination raster. Most higher level drawing operations in NuXPixels build a `PolygonMask` internally and render it row by row from top to bottom. Both even‑odd and non‑zero winding rules are supported.

### Gradients
A `Gradient` lookup table produces color values for linear or radial fills.
`LinearAscend` and `RadialAscend` generate a 0–255 coverage ramp that can index
into a gradient: `gradient[LinearAscend(x0, y0, x1, y1)]` or
`gradient[RadialAscend(cx, cy, rx, ry)]` yield a color renderer. The library also
provides a `GammaTable` for simple tone adjustments.

### Operator overloading and pull model
Renderers can be combined with `*`, `+`, `|`, `+=`, `*=`, and `|=` operators.
Each operator returns another `Renderer` that lazily requests spans from its
inputs. An expression like `canvas |= Solid<ARGB32>(color) * mask` forms a small
pipeline. As the canvas renders, it pulls spans from that pipeline and each stage
only computes what the next stage requires.

Because drawing is demand driven, NuXPixels can optimize away work in real time.
Opaque spans automatically block processing of any renderers beneath them since
those pixels are invisible. This culling happens per span and keeps the renderer
efficient even with many layers.

## Path construction
`Path` is a sequence of drawing commands supporting lines, quadratic and cubic curves. It can be modified with helper methods like `addRect`, `addEllipse`, `addRoundedRect` and `stroke`. Paths operate in double precision and can be transformed with an `AffineTransformation` before rendering.


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
