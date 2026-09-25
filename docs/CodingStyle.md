# Coding Style and Design Principles

The canonical coding style and design principles for this repository - the basis for both humans and agents, and held
to in review. `AGENTS.md` covers the operational side (directory layout, build and test gates, script portability) and
points here for code style, so there is one source of truth to keep from drifting.

The rules below apply to new and edited code. Parts of `src/` and `externals/NuX/` predate them; where an existing file
already settled on something else, §4 and §5 say which of the two wins.

## 1. Error handling, RAII, design by contract (PRIO 1)

These are the most important principles in the codebase. Get them wrong and the change will be rejected.

- **RAII means resource acquisition IS initialization.** A constructor either produces a fully valid object or throws.
  No two-phase construction. No `ok()` / `isValid()` / `init()` methods to check after the fact. No friend class that
  reaches in and fills the fields. A resource-owning class exposes no public data members.
- **Assert liberally - a lot of them.** Assertions are the primary tool for programmer errors: anything that cannot
  happen with correct code and valid inputs (a broken invariant, a precondition, an impossible case) gets an `assert`.
  Prefer the `assert(condition && "why this must hold")` form so a failure reads as an explanation. Include it as
  `#include "assert.h"` (with quotes, not `<cassert>`) so a client can override the handler with a local `assert.h`.
  Asserts are how programmer errors are handled - you never reach for `abort()`.
- **Trust the contract inward; validate only at the boundary.** A function states its preconditions and then relies on
  them. It does not re-check what a caller is contractually obligated to provide, and it does not defensively null-,
  range-, or enum-check a value the contract already pins down - it accesses it directly. Untrusted data (IVG or ImpD
  source text, an `.ivgfont` file, bytes off a file or OS call, anything arriving across the public API) is validated
  exactly once, at the perimeter where it enters; past that line the value is known-good and code uses it as given. A
  guard duplicated inside the perimeter is not extra safety, it is dead code that hides where the real contract lives
  and drifts out of sync with it. This split - assumption-driven inside, defensive at the edges - is not optional: a
  defensive check on a contract path is a review-blocking defect, not a nicety.
- **Exceptions are for runtime conditions, not for bugs.** Throw when a failure CAN happen with correct code because of
  the environment or input (malformed source, a missing font, allocation fails). Never silently swallow such an error
  and never return a half-filled output or a success code on a path that did not succeed - if a function cannot do its
  job, it throws. But do NOT throw a catchable exception for a programmer error you have proven cannot happen: that
  invites the caller to build recovery around a non-condition. Assert it instead.
- **No recovery or fallback for a state that cannot happen.** Do not add an `else` that handles a case the contract
  excludes, or a fallback branch around a condition correct code cannot produce. Assert the invariant and continue as
  if it holds, because it does. A recovery path for a non-condition is untested by construction, tempts callers into
  depending on the non-condition, and launders a real bug into a silent success. This is the previous rule (never
  `throw` for a proven-impossible error) applied to branches instead of exceptions.
- **`assert` + `throw` together is transitional scaffolding, not a default.** Use it only for a bug you have not yet
  PROVEN impossible, where running past it in release would corrupt state and a real safe fallback exists. The throw is
  the release net precisely because `assert` vanishes there. Once the invariant is proven (an exhaustive test plus
  fuzzing, ideally a compile-time exhaustiveness check), remove the throw and drop to assert-only. A permanent hybrid
  advertises doubt in your own invariant.
- **The interpreter is the perimeter.** Every limit that protects it (coordinate range, path instruction count,
  pattern resolution, image size) is checked where the value is parsed and in the domain it is parsed in. Clamping an
  `int` after a `double` conversion has already overflowed is not a check, it is a second bug.

## 2. Compactness and no duplicated functionality (PRIO 1)

- **No duplicated functionality.** Two functions or branches that do substantially the same thing are a defect, not a
  convenience: they drift apart and double every future fix. Before writing a function, branch, or helper, find the
  existing one that already does most of the job and generalize it. If you catch yourself writing a near-copy (a
  second number parser beside the one `IMPD` already has, a third bounds helper beside two others), unify them instead.
- **Compact by default.** Say it once, with the fewest moving parts. Prefer generalizing an existing path over adding a
  parallel one, and a data-driven table over repeated branches. Delete dead and superseded paths as you go; a change
  that adds a capability should still look for what it lets you remove.
- **A refactor must REDUCE size.** Its success metric is fewer lines (or fewer functions, fields, branches) with
  behavior unchanged and tests green. If a "refactor" grows the file it was the wrong change: reconsider the shape
  rather than bolting more on.
- **Do not let the data model accrete.** When a record keeps gaining a field to carry one more case, reshape it; a
  struct that has doubled its members is telling you the abstraction moved.
- **Minimizing the number of code paths is the single most important thing for correctness.** Every extra branch is a
  combination that must be reasoned about and tested; two paths that could be one are where bugs hide. Prefer one path
  that handles all cases over a special-case branch, and collapse branches that differ only in a value into a table or
  a computed operand.
- **Rewrite in three passes.** One: a prototype, thrown away. Two: the first real implementation, 100% working. Three:
  rewrite it to the fewest lines and strongest structure. Do not ship stage two - the experiments and the paths they
  left behind are the debt this section is about.
- **Optimize only for a PROVEN win.** An optimization that adds lines or code paths is allowed only when it buys a
  measured performance gain that matters. A speculative optimization is a net loss: it costs the one thing (code paths)
  most worth protecting for a benefit you have not shown. The rasterizer spans and inner loops are the one place where
  a measured win is common enough to expect; everywhere else, assume there is none until a benchmark says otherwise.

## 3. Naming

- **No abbreviations.** Full words: `instructionCount` not `instrCount`, `background` not `bg`, `resolution` not `res`,
  `parsePaint` not `parsePnt`. Established short domain terms that read as words are fine (`rgb`, `argb`, `svg`, `dpi`).
- **Boolean queries are prefixed `isX()`**: `isVisible()`, `isEmpty()`.
- Wrap variable, parameter, class, and function names in back-ticks inside comment text: "`opacity` premultiplies the
  channels".

## 4. Class layout and headers

- **Hard encapsulation.** A class owns its representation and hides it completely. Data members are private; state
  changes only through methods that preserve the class invariant, which the constructor establishes (see RAII, §1). No
  client, subclass, or friend reaches past the interface to read or write internals, and there is no backdoor that
  fills the fields from outside. Expose behavior, not state: a getter/setter pair over what is really a public field is
  still a leak if it lets a caller drive the object into an invalid state. Keep implementation detail - helper types,
  buffers, bookkeeping - out of the public header so the client surface shows only what a client must call. Hard
  encapsulation is what makes RAII and design-by-contract enforceable: if the representation can only change through
  vetted methods, an inconsistent or half-built object is simply unobservable.
- **Access specifiers go on every declaration.** `src/IVG.h`, `src/IMPD.h` and `externals/NuX/NuXPixels.h` all prefix
  each declaration with its own specifier (`public:`, a tab, then the declaration, on that line) and use no grouped
  `public:` / `private:` section headers at all. That is the form here; new declarations and new headers match it, and
  the two forms are never mixed within one file.
- **No heavy headers.** Big function bodies live in a `.cpp`; only small or hot inlines belong in a header. A large
  method defined inline in a header will be moved out in review. The painter and canvas templates in `src/IVG.h` are
  the unavoidable exception: being templates, they have to be visible to the client.
- **Keep the client surface minimal.** Internal helpers are not public API - make them protected members of the class
  that uses them, or `static` in the `.cpp`, not part of what a client sees when they include the header.
- **The library is C++03, as Emscripten compiles it.** Emscripten builds `src/` and `externals/NuX/NuXPixels.cpp`
  as `-std=c++03` with clang and libc++, and `build.sh` checks them that way on macOS. Keep to the C++03 language:
  lambdas, `constexpr`, brace-initialised containers and `>>` closing a nested template are errors there, and `auto`,
  range-`for`, `enum class`, `override` and rvalue references compile only as extensions. The standard library is
  wider, since libc++ provides `std::unique_ptr`, `<cstdint>` and even `nullptr` in C++03 mode. This is not taste:
  the library ships as source, so a construct Emscripten rejects breaks a build someone else runs. Tools, tests and
  examples are free to use more.

## 5. Comments

- **Comment sparingly - few and short.** Every comment is a maintenance liability that drifts as the code moves out
  from under it, so minimize the surface that can go stale. A comment must earn its place: the non-obvious *why*, an
  invariant, a gotcha - never the *what* (the code says that). Default to no comment. Lean on a reference to `docs/`
  instead of re-explaining the design inline, and when editing prefer deleting a stale comment to updating it.
- **Write for the reader of the file you are in, not for yourself.** A public header is read by someone USING the
  interface, so it states the contract and nothing else. Rationale aimed at whoever implements the other side is not
  contract: why a limit has the value it has, what a wrong conversion would have looked like, which alternatives were
  rejected, how a decision was reached. That belongs in `docs/`, referenced by name from the declaration. After a
  design is argued out, what lands in the code is the CONCLUSION - never the argument.
- **The FIRST SENTENCE is the brief.** Whatever form the comment takes, its opening sentence states what the signature
  cannot: allocates or not, ownership, what it does to the parse position, the constraint that decides whether to call
  it. Never a paraphrase of the name. Everything after it is detail, and a reader who stops at the first sentence must
  not be misled.
- **Keep a block comment adjacent to what it describes.** Two blocks stacked with a declaration below them silently
  orphan the first, for a reader and for any tool that lifts briefs out of the source.
- **Multi-line block comments** use `/*` on its own line, the body indented one tab, and `*/` on its own line - single
  asterisk, tab-indented body:
  ```
  /*
  	One or more sentences. Wrap `names` in back-ticks.
  */
  ```
  Do NOT write a paragraph as a stack of `//` lines, and do NOT use decorative empty `//` banner lines.
- **Struct members take an end-of-line comment, never a preceding block comment.** However long the text runs, it stays
  on the member's own line. Block comments in a public header belong to the file as a whole (`/* # Title ... */`) or to
  a GROUP of declarations (an enum), never to one member. If a member seems to need more than its line, the excess is
  rationale - see above.
- **Short inline comments** use a single end-of-line `//`, sitting at column 120 (the wrap column) padded with tabs -
  that is the general rule, with exceptions. A run of short related declarations may align to a common local column
  instead.
- **No Doxygen.** No `///`, no `///<`, no `/** */`, no `@param` / `@return` tags. Plain `//` and `/* */` only. The
  existing headers still carry an older convention (`/** **/` blocks above a class, `///` after a declaration); it is
  abandoned. Do not copy it into new code, and do not convert a file wholesale as a side effect of an unrelated change.
  The license banner at the top of each file keeps the form it has.
- **One declaration per line.**
- **Never use en or em dashes** (the `-` and `--` characters) anywhere - in code, comments, docs, or commit messages.
  Plain ASCII hyphen only. Long dashes read as an AI giveaway; normal coders do not type them.

## 6. Formatting

- **Tabs for indentation, width 4.** Never spaces, and that holds for every project file, not just the C++ sources.
  When processing a file with command-line tools, run `expand -t 4` on it first and `unexpand -t 4` on it afterwards.
- **Opening brace on the same line** as the control statement; closing brace on its own line.
- **Control-flow bodies are always braced and never inlined.** `if`, `else`, `for`, `while`, `do`, and `switch` always
  use `{ }` - even for a single statement - and the body goes on its own line(s), never on the control statement's
  line. The opening brace ends the control line, each body statement sits on its own indented line, and the closing
  brace is on its own line:
  ```
  if (x) {
  	blahblah;
  	duhduh;
  }
  ```
  Both a braceless body (`if (x) blahblah;`) and a one-line braced body (`if (x) { blahblah; duhduh; }`) are wrong.
  **Exception, sparingly and by agreement:** relax this only where braces clearly hurt - e.g. a flat lookup/dispatch
  table of `if (cond) return X;` rows reads better one-per-line than as dozens of braced blocks, and bracing there only
  bloats the line count for no clarity. When in doubt, brace it; take the exception only when it plainly wins, and
  confirm it in review.
- **Short inline function bodies may be a single line.** A function or method body of one or two simple statements may
  sit on one line when that reads more elegantly (`double getGamma() const { return gamma; }`). This is about function
  bodies only, not the control-flow rule above - a control statement inside an inline body still follows that rule.
- **Maximum line width 120 columns.** (A trailing `//` comment may start at column 120 and run past it - see §5.)
- **`#if` / `#endif` sit one tab LEFT** of the surrounding code's indentation.
- **Break long lines by leading with the operator**, indented two tabs from the original line - the double tab marks a
  continuation, distinct from a nested block:
  ```
  someCall(veryLongFirstArgument, secondArgument, thirdArgument
  		, fourthArgument, fifthArgument)
  ```

## 7. Commits

- **Short imperative subject, little or no body.** "Fix IVG pen joints documentation", not a paragraph restating what
  the diff already shows. Add a body only when the *why* is not visible in the change itself.
- **No attribution trailers.** No `Co-Authored-By` for tools or agents, no generated-with footers.
- The dash rule in §5 applies to commit messages too.
