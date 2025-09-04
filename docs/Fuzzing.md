# Fuzzing

## Building the fuzz target

The `tools/IVG2PNG.cpp` program contains an `LLVMFuzzerTestOneInput` entry point guarded by the `LIBFUZZ` macro. Compile it with clang and libFuzzer using the `BuildCpp.sh` helper:

```bash
CPP_COMPILER=clang++ CPP_OPTIONS="-fsanitize=fuzzer,address -DLIBFUZZ" \
bash tools/BuildCpp.sh beta native output/IVGFuzz -I . -I externals/ -I externals/libpng \
tools/IVG2PNG.cpp src/IVG.cpp src/IMPD.cpp externals/NuX/NuXPixels.cpp
```

The resulting binary appears as `output/IVGFuzz` and is invoked with a directory of seed inputs:

```bash
./output/IVGFuzz corpus/
```

On macOS the clang from Xcode omits libFuzzer. Install `llvm` via Homebrew and set `CPP_COMPILER`:

```bash
CPP_COMPILER=$(brew --prefix llvm)/bin/clang++ CPP_OPTIONS="-fsanitize=fuzzer,address -DLIBFUZZ" \
bash tools/BuildCpp.sh beta native output/IVGFuzz -I . -I externals/ -I externals/libpng \
tools/IVG2PNG.cpp src/IVG.cpp src/IMPD.cpp externals/NuX/NuXPixels.cpp
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
