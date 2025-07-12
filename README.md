# IVG

IVG (Imperative Vector Graphics) is a compact, dependency-free 2D vector format and renderer written in standard C++. Graphics are described using the small imperative language **ImpD**, which supports variables and control flow for defining procedural images.

The renderer is built on the included **NuXPixels** rasterizer and provides high-quality gamma-correct anti-aliasing. The format is concise and designed for both hand-written and generated vector graphics.

## Features

- Graphics are described using **ImpD**, a minimal imperative language for image construction.  
- Built-in **NuXPixels** rasterizer provides high-quality, gamma-correcting anti-aliasing.  
- Renderer written in **portable, dependency-free C++**, with no reliance on third-party libraries.  
- Supports **paths, shapes, images, text, styling, transformations**, and nesting.  
- Simple `.ivgfont` format for embedded vector fonts, converted from standard font formats.
- **Standalone HTML editor** (IVGFiddle) for live editing and previewing IVG code.  
- Built-in **test suite** with regression output compared to golden PNGs.  
- Self-contained format and tools designed for experimentation and integration.

## Prerequisites

You will need a standard C++ compiler.

- On **macOS** or **Linux**, use `g++` or `clang++`.
- On **Windows**, the build requires Microsoft Visual C++. Any version from Visual Studio 2008 (VC9.0) onward should work. The build scripts locate the compiler automatically using `vswhere.exe`, falling back to known versions if needed.

## Build & Test

Run `./build.sh` (or `build.cmd` on Windows) from the repository root. This builds the renderer tools and runs the regression tests.

Both the **beta** and **release** targets are compiled with optimizations enabled. The **beta** build additionally has assertions turned on.

## Helper Scripts

- `build.sh` / `build.cmd` – build both the **beta** and **release** targets and run all tests  
- `tools/updateIVGTests.sh` / `.bat` – regenerate golden PNGs from all `.ivg` test files  
- `tools/updateDocumentation.sh` – rebuild HTML documentation using Pandoc and PikaScript (Mac / Linux only)

## IVGFiddle

Included in this repository is a standalone HTML application called **IVGFiddle**. You can open it in your browser to write IVG code and see the output rendered in real time.

- File location: `tools/ivgfiddle/output/ivgfiddle.html`

You can also try it live without cloning the repo:  
[IVGFiddle](https://htmlpreview.github.io/?https://github.com/malstrom72/IVG/blob/main/tools/ivgfiddle/output/ivgfiddle.html)

## Fonts

The repository includes several `.ivgfont` files converted from the following open-source fonts:

- Source Sans Pro  
- Source Serif Pro  
- Source Code Pro  

These fonts are licensed under the [SIL Open Font License 1.1](https://scripts.sil.org/OFL).

## Documentation

- [IVG Documentation](docs/IVG%20Documentation.md)  
- [ImpD Documentation](docs/ImpD%20Documentation.md)  
- [NuXPixels Documentation](docs/NuXPixels%20Documentation.md)  
- [Developer Guide](docs/Developer%20Guide.md)  

## AI Usage

AI tools (such as OpenAI Codex) have occasionally been used to assist with documentation, code comments, test generation, and repetitive edits. All core source code has been written and refined by hand over many years.

## License

This project is released under the [BSD 2-Clause License](LICENSE).  
Fonts are distributed under the [SIL Open Font License 1.1](https://scripts.sil.org/OFL).
