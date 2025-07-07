# IVG

IVG (Imperative Vector Graphics) is a compact, dependency-free 2D vector format and renderer written in C++. Graphics are described using the small imperative language **ImpD**, enabling variables and basic control flow for generative images.

The renderer relies only on the included **NuXPixels** library and provides high-quality anti-aliasing. A typical file begins with the version line:

```
format IVG-2 requires:ImpD-1
```

Instructions cover shapes, paths, images and text as well as directives for styling and transformations. See `docs/IVG Documentation.md` and `docs/ImpD Documentation.md` for the complete specification.
