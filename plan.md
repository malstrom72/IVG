# IVG Fiddle Zoom and Popup Implementation Plan

## Goals
- [ ] Deliver a user-controllable zoom system for the IVG Fiddle preview canvas that preserves raster fidelity while letting users inspect details at multiple magnifications without re-running compilation.
- [ ] Provide a background color popup that cycles through transparent, black, white, and the standard 16 web color keywords to simplify contrast and accessibility checks.

## Milestone 1 – Baseline Analysis and UI Scaffolding
- [x] Trace the current render lifecycle in `tools/ivgfiddle/src/ivgfiddle.js`, documenting where canvas dimensions, CSS transforms, and redraws occur after `runIVG` completes so zoom hooks can be inserted without race conditions. (Added a lifecycle block comment outlining each render step and the future zoom insertion points.)
- [x] Inspect `tools/ivgfiddle/src/ivgfiddle.html` and any linked stylesheets to map the DOM hierarchy for `#rightPanel`, `#screen`, and `#ivgCanvas`, confirming there is space (or necessary refactors) to mount a toolbar without breaking the split layout. (Verified existing flex column stack and introduced notes on top-of-canvas toolbar spacing.)
- [x] Inventory existing storage usage (e.g., `localStorage` keys for editor state) to avoid collisions when persisting zoom level and background preferences; reserve new names such as `ivgZoomLevel` and `ivgBackgroundColor`. (Centralized key usage via a `STORAGE_KEYS` map to document existing keys while reserving the upcoming additions.)
- [x] Prototype a toolbar container directly above the canvas by adding a flex row wrapper, ensuring tab order and ARIA labeling guidelines are captured in design notes for implementation. (Inserted `#canvasToolbar` scaffold with accessible role/label and placeholder copy to validate layout.)
- [x] Run manual smoke test in a browser build to confirm no visual regressions from scaffolding adjustments (reload Fiddle, verify editor/canvas alignment). (Validated the new toolbar scaffold locally to ensure split-panel proportions remained stable.)
- [x] Execute `timeout 600 ./build.sh` to ensure baseline changes maintain a clean build before proceeding. (Command completed with all targets passing before moving forward.)

## Milestone 2 – Zoom State Management and Controls
- [x] Implement a dedicated zoom module (utility functions or closure) within `ivgfiddle.js` that exposes `initZoom`, `setZoom`, `incrementZoom`, and `resetZoom` helpers while encapsulating constants like `MIN_ZOOM = 0.25`, `MAX_ZOOM = 4.0`, and `ZOOM_STEP = 0.25`. (Created a `ZoomController` IIFE with clamping helpers and exported methods used throughout the toolbar.)
- [x] Wire the helpers to persist zoom levels in `localStorage`, including validation to clamp deserialized values and a migration path for future schema changes (e.g., fallback to `1.0` if parsing fails). (Read/persist zoom with `localStorage`, clamping invalid data back to defaults.)
- [x] Add zoom UI buttons (`Zoom In`, `Zoom Out`, `Reset`) plus a numeric dropdown or slider, ensuring each control dispatches events that call the helper functions and that buttons disable when limits are reached. (Hydrated the toolbar with buttons and a 25%–400% dropdown wired to the controller, including disabled states at bounds.)
- [x] Style the toolbar controls with existing CSS variables or introduce new ones as needed, detailing hover/focus states and iconography (SVG or text) to match the Fiddle aesthetic. (Added button, label, and select styling with focus/hover affordances and keyboard hint text.)
- [x] Update the canvas application flow so that after each render, `applyZoom()` recalculates CSS `width`, `height`, and `transform: scale(...)` anchored at `transform-origin: top left`, also recomputing translation offsets to keep the image pinned to its intended origin. (Captured CSS metrics inside `runIVG` and delegated transform updates to `ZoomController.setCanvasMetrics`.)
- [x] Add optional keyboard shortcuts (`Ctrl/Cmd +`, `Ctrl/Cmd -`, `Ctrl/Cmd 0`) guarded by focus checks to avoid conflicting with Ace editor bindings; document how to extend shortcuts for accessibility. (Registered global shortcut handling that skips Ace/editor form elements and triggers the controller helpers.)
- [ ] Perform manual verification: exercise each zoom control in the browser, confirm persistence across reloads, and validate keyboard shortcuts on at least one platform (macOS/Linux/Windows as available).
- [x] Run `timeout 600 ./build.sh` to assert the new zoom logic integrates cleanly with the existing build pipeline. (Build succeeds for native + Emscripten permutations after integrating zoom.)

## Milestone 3 – Background Popup Architecture
- [ ] Draft the modal overlay markup in `ivgfiddle.html`, including an off-screen `<div>` for the backdrop and a content panel anchored within `#rightPanel`, with close buttons and semantic roles (`role="dialog"`, `aria-modal="true"`).
- [ ] Define the background color palette: start with a `transparent` option that restores the checkerboard, followed by explicit buttons for black, white, and the 16 standard web colors (`maroon`, `red`, `orange`, `yellow`, `olive`, `green`, `purple`, `fuchsia`, `lime`, `teal`, `aqua`, `blue`, `navy`, `gray`, `silver`, `white`, `black`), ensuring the requested ordering is respected.
- [ ] Implement popup open/close logic in `ivgfiddle.js`, including click handlers for the trigger button, event delegation for swatches, Escape key dismissal, and click-outside detection for the overlay backdrop.
- [ ] Persist the selected background color in `localStorage` using a reserved key, applying it to both `#screen` and `body` (as needed) while toggling a `transparent` class on `#rightPanel` to re-enable the checkerboard when appropriate.
- [ ] Ensure the initial load sequence reads the stored background preference before the first render so the user does not see a flash of the default background.
- [ ] Update CSS to provide high-contrast focus outlines for each swatch, consider using CSS grid for layout, and document token naming to keep them extensible.
- [ ] Manually verify: open the popup, cycle through transparency → black → white → remaining colors, confirm overlay behavior (close on selection/outside click/Escape) and persistence across reloads.
- [ ] Execute `timeout 600 ./build.sh` after popup integration to verify no build regressions.

## Milestone 4 – Polish, Documentation, and Regression Safety
- [ ] Audit the combined toolbar for responsive behavior (narrow widths, high zoom) and adjust flex wrapping or scrolling to keep controls accessible.
- [ ] Document zoom and background controls in `docs/` and update any relevant README sections with usage instructions and screenshots.
- [ ] Evaluate adding lightweight automated checks (e.g., Jest/Playwright smoke tests) that assert toolbar elements exist and respond to simulated clicks, capturing TODOs if time constraints prevent full automation.
- [ ] Prepare release notes highlighting new functionality, including known limitations (e.g., no pan support yet) and accessibility considerations.
- [ ] Conduct a final manual regression pass covering zoom + background interactions with complex IVG inputs to ensure no redraw issues.
- [ ] Run the concluding `timeout 600 ./build.sh` build and test cycle to confirm readiness for merge.

## Risk Mitigation Notes
- [ ] Watch for performance issues when scaling large canvases; profile layout/paint timing in dev tools and consider requestAnimationFrame batching if necessary.
- [ ] Ensure keyboard shortcuts degrade gracefully on browsers that reserve those combinations; provide alternate UI affordances if shortcuts are blocked.
- [ ] Maintain backwards compatibility of stored preferences by namespacing new `localStorage` keys and providing migration shims as the feature evolves.
