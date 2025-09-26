# IMPD-Native Specifications for IVG Regression Scenarios

All proposals below keep the scenario metadata in executable IMPD or `meta` directives so runtime renderers either evaluate harmless assignments or skip the metadata entirely. Tooling can parse the same constructs to discover inputs, dependency bundles, and verification rules without scraping comments.

## 1. `meta scenario` Blocks with Structured Values
* Reserve the existing `meta` statement for scenario manifests: `meta scenario name:"sector-25" params:{ volume:0.25 sector:3 } size:{ w:256 h:512 }`.
* Unrecognized keys are ignored by today’s parsers, yet automation can enumerate every `meta scenario` entry to drive IVG2PNG with exact parameter dictionaries and viewport hints.
* Additional keys such as `profile:"ci"` or `output:"meters/sector-25"` keep routing logic declarative and colocated with the snippet.

## 2. `meta scenario.setup` with Inline IMPD
* Allow each manifest to point at literal setup code: `meta scenario.setup name:"sector-25" code:[ volume = 0.25; sector = 3; include "support.ivg"; ]`.
* The harness concatenates the `code` array ahead of the snippet so the assignments and includes execute before drawing begins, mirroring how product code seeds locals.
* Because the payload is valid IMPD, authors can call helpers, compute derived parameters, or reuse macros without inventing another DSL.

## 3. Array-Driven Sweeps via `meta scenario.matrix`
* Introduce a matrix key that expands sweeps: `meta scenario.matrix name:"volume-grid" vary:{ volume:[0,0.25,0.5,0.75,1] } fixed:{ sector:2 }`.
* Test runners evaluate the cartesian product to render a batch while still emitting discrete job identifiers such as `volume-grid/volume=0.5`.
* Optional `meta scenario.matrix tolerance:{ metric:"ssim" value:0.995 }` lets each expansion carry bespoke comparison thresholds.

## 4. Asset Registration Through `meta scenario.assets`
* Encode includes, bitmaps, and fonts in a structured list: `meta scenario.assets name:"sector-25" files:["support.ivg", { image:"textures/leds.png" }, { font:"fonts/seg7.otf" }]`.
* Automation hydrates a sandbox by copying the listed resources before invoking IVG2PNG; IDE integrations mount the same files when launching previews.
* Missing entries can surface as lint warnings, ensuring every scenario enumerates its full dependency graph.

## 5. Golden Provenance in `meta scenario.expect`
* Store reference hashes and auxiliary data inline: `meta scenario.expect name:"sector-25" sha256:"d9c0…" renderer:"ivg2png@1.9" scale:2`.
* During regression checks the harness compares the freshly rendered PNG hash to the manifest, and can even validate renderer versions to detect environmental drift.
* Additional fields (e.g., `luminanceMax:240`) let QA enforce semantic limits alongside pixel matches.

## 6. Programmatic Assertions via `meta scenario.assert`
* Let manifests declare validation snippets: `meta scenario.assert name:"sector-25" code:[ assert $volume >= 0 && $volume <= 1; boundsCheck -20 -50 40 120; ]`.
* The runner injects the code after rendering (or wraps the drawing in an instrumentation pass) to confirm bounding boxes, color ranges, or custom diagnostics.
* Assertions can emit structured failure records that CI surfaces next to the golden diff for rapid triage.

## 7. Lifecycle Controls with `meta scenario.state`
* Track readiness through metadata instead of comments: `meta scenario.state name:"sector-25" status:draft` or `status:sealed`.
* Tooling can skip hash enforcement for `draft` entries yet still produce preview PNGs, while release branches fail if any declared scenario remains non-sealed.
* A secondary `status:experimental` mode could allow opt-in execution on developer machines without polluting CI dashboards.

## 8. Batch Playlists Using Executable Registries
* Provide an optional helper include that registers scenarios procedurally: `scenario.register("sector-25", setup:[ volume = 0.25 ])`.
* The helper defines `scenario.register` as an IMPD `call` that appends to a global list; the runner evaluates the snippet in discovery mode to collect every registration before rendering.
* This keeps scenario definitions in runnable code, enables loops (`for v in:[0,0.5,1] [ scenario.register("volume-"+$v, setup:[ volume = $v ]) ]`), and avoids separate manifest files.

## 9. Derived Outputs through `meta scenario.outputs`
* Declare alternate artefacts such as SVG dumps or JSON metrics: `meta scenario.outputs name:"sector-25" items:["png", { histogram:"hist/sector-25.json" }]`.
* The harness inspects the list to trigger extra analysis passes (e.g., gradient smoothness) while production renderers ignore the directive entirely.
* IDE tooling can expose the same outputs as quick actions ("Recompute histogram") without needing bespoke configuration.

## 10. Harness Negotiation via `meta scenario.require`
* Scenarios can advertise prerequisites for execution: `meta scenario.require name:"sector-25" features:["ivg2png>=1.9", "extended-masks"] timeout:120`.
* CI filters out jobs if the current toolchain lacks the requested features, and developers receive explicit error messages rather than opaque runtime failures.
* Requirements also provide a migration path when new IMPD opcodes or renderer capabilities are introduced—scenarios pin what they need and older branches simply skip them.

## Coordinating With IDEs and Automation
All metadata above rides on core IMPD syntax, allowing interpreters that do not recognize the keys to continue unharmed. Tooling walks the AST once, gathers every `meta` directive or procedural registration, and builds a manifest that powers ivgfiddle previews, VS Code integrations, and CI regressions. Because setup code and assertions remain valid IMPD, authors debug scenarios inside their usual workflow while the harness reuses the same snippets to hydrate locals, mount assets, and verify the rendered output.
