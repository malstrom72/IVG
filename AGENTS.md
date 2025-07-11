# Repository Guidelines

To run the test suite use the helper script with up to two minutes allowed for execution:

```bash
timeout 120 ./build.sh beta native nosimd|simd
```

Always execute this command before committing changes to verify that the build and regression tests succeed.

### Repository layout
The project uses a consistent folder structure. Build output is written to `output/` and no source files live there. Useful locations:

- `tools/` – scripts for building and maintaining the code and documentation.
- `projects/` – Xcode and Visual Studio project files.
- `docs/` – documentation.
- `src/` – C++ source code for the library. The library is distributed as source rather than prebuilt binaries.
- `tests/` – regression tests.
- `examples/` – small sample programs.
- `benchmarks/` – JavaScript performance tests.
- `output/` – contains only build artifacts (and any runtime dependencies), no source files.

Root level `build.sh` and `build.cmd` (mirrored implementations) should build and test both the beta and release targets.

### Formatting rules

- Tabs (width 4) for indentation.
- Opening braces stay on the same line as the control statement and closing braces are on their own line.
- Maximum line width is 120 characters. End-of-line comments may start at column 120.
- Line continuations should start with the operator and be indented two tabs from the original line.
- `#if`/`#endif` blocks should appear one tab *left* of the current indentation level.

### Script portability
All user-facing `.sh` and `.cmd` files must work when launched from any directory.
They should start by changing to their own folder (or the repository root) so that
relative paths resolve correctly.

Every `.sh` script must have a corresponding `.cmd` implementation with identical behavior. Use `.cmd` files rather than `.bat`.

```bash
# example for a shell script
cd "$(dirname "$0")"/..

REM example for a .cmd script
CD /D "%~dp0\.."
```

For robust error handling, `.sh` scripts should begin with:

```bash
set -e -o pipefail
```

And `.cmd` scripts normally use a simple error check:

```batch
CALL buildAndTest.cmd %target% || GOTO error
EXIT /b 0
:error
EXIT /b %ERRORLEVEL%
```
