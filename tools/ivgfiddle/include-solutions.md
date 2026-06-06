# Handling Problematic `#include` Statements in IVGFiddle

This document outlines four complementary strategies for mitigating the disruptive effects of `#include` statements inside the IVGFiddle environment.

## 1. Add a Header Stub Layer
- Maintain a curated set of stub headers that mirror the interfaces of commonly included platform headers but remove heavy or incompatible dependencies.
- Point IVGFiddle's compiler configuration to search the stub directory first so the lightweight versions satisfy includes during previews.
- Document the stub coverage to avoid silent divergences from real builds.

## 2. Precompile a Virtual SDK Snapshot
- Use the existing build scripts to generate a precompiled header bundle (PCH) that aggregates frequently included headers.
- Ship the PCH snapshot with IVGFiddle so preview compiles reuse it instead of reprocessing expensive include chains.
- Refresh the snapshot automatically as part of the regular CI pipeline to keep it aligned with the repository state.

## 3. Introduce Include Guards & IWYU Checks
- Audit the IVGFiddle source for redundant includes and enforce `#pragma once` or classic include guards in every header.
- Integrate "Include What You Use" (IWYU) analysis into the IVGFiddle test suite to flag over-inclusive files.
- Track IWYU results to ensure incremental changes do not reintroduce unnecessary dependencies.

## 4. Leverage Module-Friendly Wrappers
- Create thin wrapper modules (either C++20 modules or generated headers) that expose only the symbols IVGFiddle requires from heavy dependencies.
- Update the IVGFiddle build configuration to import wrappers in place of direct includes where modules are supported.
- Provide a compatibility fallback that maps the wrappers back to traditional includes for toolchains without module support.
