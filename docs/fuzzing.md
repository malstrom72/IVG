# Fuzzing

Version: 2026-10-05

How these projects fuzz with libFuzzer. Every copy of this file is identical apart from the "Local additions" section
at the end, which holds a project's targets, scripts and exceptions.

## Machines

- The Mac (clang, AddressSanitizer plus UndefinedBehaviorSanitizer) is the main fuzzing machine. It is the fastest per
  worker and the only one with both sanitizers by default.
- Windows runs MSVC `/fsanitize=address /fsanitize=fuzzer` by default. It adds capacity and covers what only Windows
  has: 32-bit `long`, the MSVC compiler and runtime, and the Win32 backends.
- clang-cl on Windows is allowed only for a build that has passed the throw test under "clang-cl on Windows".
- Check the load before starting (`uptime` on the Mac) and keep to about 4 workers per campaign while other projects
  are running. Watch free disk space: nothing stops a run when the disk fills.

## Building

Build optimized with asserts on: the release target plus `/U NDEBUG` (MSVC, clang-cl) or `-UNDEBUG` (clang). Never
`-O0`, which is several times slower. Release is required on Windows anyway, because libFuzzer links against the
static release runtime.

- Mac clang: `-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -UNDEBUG`.
- MSVC: `/fsanitize=address /fsanitize=fuzzer /U NDEBUG`, and copy MSVC's `clang_rt.asan_dynamic-x86_64.dll` next to
  the executable.

Link with an 8 MB stack on Windows (`/link /STACK:8388608`), since deep recursion otherwise overflows the 1 MB default
long before it would on the Mac.

### clang-cl on Windows

clang-cl's sanitizer instrumentation breaks MSVC C++ exception handling. A target that throws can crash inside
`__CxxFrameHandler3` (an access violation, often at `0xffffffffffffffff`), stop with exit code `0x80000003`, or
silently run the wrong code. A build needs all of the following:

- `/Ob0`: SanitizerCoverage puts callbacks in catch blocks without the funclet bundle, so the rest of the handler is
  replaced with unreachable code (llvm#212404). Without inlining, fewer comparisons land in catch blocks. This makes
  the bug rarer; it does not remove it.
- `-fsanitize-address-use-after-return=never`: ASan's fake stack breaks unwinding.
- `/D _DISABLE_STRING_ANNOTATION /D _DISABLE_VECTOR_ANNOTATION`: otherwise linking fails on `annotate_string`.
- The release target: libFuzzer needs the static release runtime (`/MT`).

Copy LLVM's `clang_rt.asan_dynamic-x86_64.dll` (from `lib\clang\<version>\lib\windows`), not MSVC's. With BuildCpp,
quote the compiler path because it contains a space:
`CPP_COMPILER="C:\Program Files\LLVM\bin\clang-cl.exe"`.

The throw test: before a clang-cl build is trusted, replay a corpus in which most inputs make the target throw, with
zero crashes. Repeat it whenever LLVM is updated.

## The harness

Remove every route to files, the console and the system from the target itself. Replacing a variable or a name is not
enough when the same function can still be reached another way.

Turn off CRT dialogs in `LLVMFuzzerInitialize`, or a failed assert hangs the worker on a message box:

```cpp
#if defined(_MSC_VER)
	_set_error_mode(_OUT_TO_STDERR);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
```

## Running

- Pass `-artifact_prefix=<folder>/`, or crash files land in the current directory, which is easy to commit by mistake.
- Pass a scratch folder as the first corpus directory, since libFuzzer writes new inputs there. Use forward slashes in
  `-dict` paths on Windows.
- Mac environment:
  ```bash
  symbolizer="$(brew --prefix llvm)/bin/llvm-symbolizer"
  export ASAN_OPTIONS="detect_container_overflow=0:external_symbolizer_path=$symbolizer"
  export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:external_symbolizer_path=$symbolizer"
  ```
  `detect_container_overflow=0` avoids false reports from uninstrumented system libraries. The explicit symbolizer
  avoids a deadlock in `atos`.
- Start runs longer than half an hour with `nohup caffeinate -i ... & disown`, so that neither sleep nor the end of the
  session that started them stops them.

## Corpus and regression

- `tests/fuzz/` holds a minimized corpus archive per target, a dictionary and hand-made seeds.
- Refresh an archive only after a substantial run: merge with `-merge=1` into an empty folder, then pack
  deterministically:
  ```bash
  tar --sort=name --owner=0 --group=0 --numeric-owner --mtime='2000-01-01 00:00Z' -cf - corpus | gzip -9n
  ```
  GNU tar and bsdtar do not produce identical archives, so refresh with GNU tar. Use `xz -9` only where it saves a
  lot: the `tar.exe` that ships with Windows cannot read xz and hangs instead of failing.
- The normal build replays the corpus, the seeds and every past crash input through a plain `main()` that reads files
  and calls `LLVMFuzzerTestOneInput`. It is built without fuzzer instrumentation, with every compiler the project uses.
- Commit the input of each fixed crash as a regression input.
- Keep a crash file from a Windows clang-cl build only if it also crashes with MSVC or on the Mac.

## Local additions

IVG has one target, `IVGFuzz`: `LLVMFuzzerTestOneInput` in `tools/IVG2PNG.cpp`, built when `LIBFUZZ` is defined. One
input is a complete IVG document, run by `IMPD::Interpreter` with an `IVGExecutor` on a `SelfContainedARGB32Canvas`, so
it reaches the IMPD interpreter, IVG and the NuXPixels rasterizer. It does not reach external fonts, includes, image
files or PNG output; text is only reached through `define font` in the input.

- **Building.** `bash tools/buildIVGFuzz.sh` (clang) and `tools\buildIVGFuzz.cmd` (MSVC) build `output/IVGFuzz`. On
  macOS, Xcode's clang has no libFuzzer, so use Homebrew's:
  `CPP_COMPILER=$(brew --prefix llvm)/bin/clang++ bash tools/buildIVGFuzz.sh`. Both builds define
  `FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION`, which makes `checkBounds` cap canvases, images and patterns at 16M
  pixels, so that legal but huge bounds are not reported as out of memory.
- **No clang-cl on Windows.** IMPD's `Interpreter::run` catches and rethrows at every nesting level, and every
  clang-cl build tried so far, `/Ob0` included, crashes on that rethrow, so none passes the throw test. Windows
  fuzzing uses MSVC.
- **Windows stack.** `LLVMFuzzerInitialize` also calls `SetThreadStackGuarantee`, so that a stack overflow leaves
  enough stack to save the input.
- **Running.** `bash tools/runIVGFuzz.sh` and `tools\runIVGFuzz.cmd` fuzz until stopped, one process per logical CPU,
  with `-ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 -rss_limit_mb=4096` and artifacts in
  `output/fuzzArtifacts/`. The seeds are every `.ivg` in the repository, gathered into `output/fuzzSeeds` by
  `tools/collectFuzzCorpus.sh` at the start of each run, and new inputs build up in `output/fuzzCorpus`, outside git.
  Extra arguments go to libFuzzer, for example `-fork=4 -max_total_time=43200`. An input counts as a timeout after
  30 s with clang and 120 s with MSVC, both about 3 s of work in a release build. On macOS `runIVGFuzz.sh` keeps an
  `ASAN_OPTIONS` you have exported and otherwise sets `detect_container_overflow=0` alone.
- **Replaying.** Pass a saved input to the target: `./output/IVGFuzz output/fuzzArtifacts/crash-...`.
- **Corpus and regression.** IVG has no `tests/fuzz/` archive and no replay `main()` yet. Past crash inputs are
  committed in `fuzzCrashes/`, but nothing replays them automatically.
