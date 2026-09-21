# Repository Guidelines

To run the test suite use the helper script with up to ten minutes allowed for execution:

```bash
timeout 600 ./build.sh
```

Always execute this command before committing changes to verify that the build and regression tests succeed.

A complete run ends with the line:

```
=== ALL BUILDS AND TESTS COMPLETED SUCCESSFULLY ===
```

If this message does not appear, the build has not finished correctly.

## Repository layout
The project uses a consistent folder structure. Build output is written to `output/` and no source files live there. Useful locations:

- `tools/` - scripts for building and maintaining the code and documentation.
- `projects/` - Xcode and Visual Studio project files.
- `docs/` - documentation.
- `externals/` - projects and source code from other repositories (only touch this content when explicitly asked to).
- `src/` - C++ source code for the library. The library is distributed as source rather than prebuilt binaries.
- `tests/` - regression tests.
- `examples/` - small sample programs.
- `benchmarks/` - JavaScript performance tests.
- `output/` - contains only build artifacts (and any runtime dependencies), no source files.

Root-level `build.sh` and `build.cmd` (mirrored implementations) should build and test both the beta and release targets.

BuildCpp.sh and BuildCpp.cmd are copied from another repository. Only make changes to them if there is no other solution.

## Coding style
Code style and design principles live in [docs/CodingStyle.md](docs/CodingStyle.md). That document is canonical for
everything about how the code itself is written; this file covers only the operational side. If the two ever conflict,
CodingStyle.md wins.

## Script portability
All user-facing `.sh` and `.cmd` files must work when launched from any directory. They should start by changing
to their own folder (or the repository root) so that relative paths resolve correctly.

`.sh` scripts must be runnable without requiring `chmod +x`; always invoke them with `bash path/to/script.sh` (do
**not** rely on the system-default `sh`).  Each script must start with a portable she-bang:

```
#!/usr/bin/env bash
set -e -o pipefail -u
```

Every `.sh` script must have a corresponding `.cmd` implementation with identical behavior. Use `.cmd` files rather than `.bat`.

```
# example for a shell script
cd "$(dirname "$0")"/..
```

REM example for a .cmd script  
```
CD /D "%~dp0\.."
```

For robust error handling, `.sh` scripts should begin as shown above, and `.cmd`
scripts normally use a simple error check:

```
CALL buildAndTest.cmd %target% || GOTO error
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
```
