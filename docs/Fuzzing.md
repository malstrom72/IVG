# Fuzzing

## Building the fuzz target

The `tools/IVG2PNG.cpp` program contains an `LLVMFuzzerTestOneInput` entry point guarded by the `LIBFUZZ` macro.
`tools/buildIVGFuzz.cmd` builds it with MSVC's libFuzzer and AddressSanitizer (Visual Studio 2022, x64), and
`tools/buildIVGFuzz.sh` does the same with clang, adding UndefinedBehaviorSanitizer (which MSVC lacks) so that integer
overflows and out-of-range conversions are reported too. Both build `output/IVGFuzz` optimized but with asserts on.

```bash
bash tools/buildIVGFuzz.sh
```

On macOS the clang from Xcode omits libFuzzer. Install `llvm` via Homebrew and set `CPP_COMPILER`:

```bash
CPP_COMPILER=$(brew --prefix llvm)/bin/clang++ bash tools/buildIVGFuzz.sh
```

If above doesn't work, try this:

```bash
CPP_COMPILER="$(brew --prefix llvm)/bin/clang++" CPP_OPTIONS="-fsanitize=fuzzer,address -DLIBFUZZ \
-isysroot $(xcrun --sdk macosx --show-sdk-path) -stdlib=libc++ \
-L$(brew --prefix llvm)/lib/c++ -L$(brew --prefix llvm)/lib/unwind -L$(brew --prefix llvm)/lib \
-Wl,-rpath,$(brew --prefix llvm)/lib/c++ -Wl,-rpath,$(brew --prefix llvm)/lib -lunwind -lc++ -lc++abi" \
bash tools/BuildCpp.sh beta native output/IVGFuzz -I . -I externals/ -I externals/libpng \
tools/IVG2PNG.cpp src/IVG.cpp src/IMPD.cpp externals/NuX/NuXPixels.cpp
```

## Running it

`tools/runIVGFuzz.cmd` and `tools/runIVGFuzz.sh` fuzz until stopped with Ctrl-C, one process per logical CPU, starting
from every `.ivg` in the repository (collected by `tools/collectFuzzCorpus`). Crashes, out-of-memory and timeouts are
saved in `output/fuzzArtifacts` and fuzzing goes on, so one bug can leave many files there. New inputs build up in
`output/fuzzCorpus`, so a later run continues where the last one stopped. An input counts as a timeout after 30 s with
clang and 120 s with MSVC, whose instrumentation makes the code about ten times slower again; both are about 3 s of
work in a release build. A longer limit lets slow inputs into the corpus, where they hold up every later job. Extra
arguments go to libFuzzer and override the defaults:

```bash
bash tools/runIVGFuzz.sh -fork=8 -max_total_time=3600
```

On macOS `runIVGFuzz.sh` sets `ASAN_OPTIONS=detect_container_overflow=0` unless `ASAN_OPTIONS` is already set. libFuzzer's
prebuilt library there is not built with AddressSanitizer but shares `std::vector` code with the fuzz target, so without
it fork mode stops at once with a false container overflow inside libFuzzer. Set it yourself when running `IVGFuzz` on a
directory by hand.

To replay a saved input, pass it to the fuzz target directly:

```bash
./output/IVGFuzz output/fuzzArtifacts/crash-...
```
