# Developer Guide

This document summarizes how to integrate the IVG renderer into a C++ project and provides a quick overview of the most important API classes.

## Building the library

Use `build.sh` (or `build.bat` on Windows) to compile the tools and run the regression tests. A typical invocation is:

```bash
./build.sh beta native nosimd
```

The script builds the test runner, the `IVG2PNG` example tool and executes the unit tests found under `tests/`.

## Core classes

The API is centered around the `IMPD::Interpreter` class which executes ImpD instructions. `IVG::IVGExecutor` implements the ImpD `Executor` interface and interprets IVG drawing commands. Graphics are rendered onto a `Canvas` implementation.

Key types declared in `src/IVG.h` include:

- **Canvas** – abstract rendering surface. Concrete subclasses like `SelfContainedARGB32Canvas` store pixels in memory.
- **IVGExecutor** – handles IVG instructions, loading images and fonts, and manages the current rendering `Context`.
- **Context** – holds transformation and styling state during rendering.
- **Paint** and **Stroke** – describe fill and stroke style information.

See `docs/IVG Documentation.md` for a full description of every drawing instruction.

## Basic usage

The simplest way to render an IVG file is similar to the `tools/IVG2PNG.cpp` example:

```cpp
#include "src/IVG.h"
using namespace IVG;

SelfContainedARGB32Canvas canvas;
STLMapVariables vars;
IVGExecutor executor(canvas);
IMPD::Interpreter impd(executor, vars);

impd.run(ivgSource); // ivgSource contains the IVG text
NuXPixels::SelfContainedRaster<NuXPixels::ARGB32>* raster = canvas.accessRaster();
```

After running the interpreter the resulting raster can be written to PNG or uploaded to another graphics API.

## Extending the executor

Applications typically subclass `IVGExecutor` to supply images and fonts from custom sources or to hook into tracing and error handling.

```cpp
class MyExecutor : public IVG::IVGExecutor {
public:
    MyExecutor(Canvas& canvas) : IVGExecutor(canvas) {}
    Image loadImage(IMPD::Interpreter&, const IMPD::WideString& source,
                    const NuXPixels::IntRect* rect, double scale) override {
        // Provide image data from your own asset system
    }
};
```

Instantiate your derived executor when creating the interpreter so that image and font lookups are resolved by your application.

## Reference files

- `src/IVG.h` – public declarations for canvases, paint objects and `IVGExecutor`.
- `tools/IVG2PNG.cpp` – minimal example program that converts an IVG file to PNG.
- `docs/ImpD Documentation.md` – specification of the ImpD scripting language.
- `docs/IVG Documentation.md` – detailed description of available drawing instructions.
- `docs/NuXPixels Documentation.md` – overview of the low-level rendering library.
