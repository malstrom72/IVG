# Repository Guidelines

To run the test suite use the helper script with up to two minutes allowed for execution:

```bash
timeout 120 ./build.sh beta native nosimd|simd
```

Always execute this command before committing changes to verify that the build and regression tests succeed.

### Formatting rules

- Tabs (width 4) for indentation.
- Opening braces stay on the same line as the control statement and closing braces are on their own line.
- Maximum line width is 120 characters. End-of-line comments may start at column 120.
- Line continuations should start with the operator and be indented two tabs from the original line.
- `#if`/`#endif` blocks should appear one tab *left* of the current indentation level.
