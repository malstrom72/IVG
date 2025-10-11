# IVGFiddle Toolbar Guide

## Overview

The IVGFiddle toolbar lives directly above the preview canvas and provides quick access to zoom,
vector scaling, and background contrast tools. Preferences persist across sessions, so the editor
reopens with the same magnification and backdrop you previously selected.

## Zoom controls

- **Buttons** – `−`, `Reset`, and `+` step the zoom level through curated presets. The controller
  clamps values between 25 % (0.25×) and 1000 % (10×).
- **Dropdown** – pick an exact preset if you need to jump to a specific magnification. Custom zooms
  introduced by the buttons still appear in the list so the UI stays in sync.
- **Keyboard shortcuts** – `Ctrl`/`Cmd` `+`, `Ctrl`/`Cmd` `−`, and `Ctrl`/`Cmd` `0` map to zoom in,
  zoom out, and reset respectively. Shortcuts are ignored when focus is inside the Ace editor to
  avoid conflicts with text editing.

The zoom controller reapplies the transform after each render. In bitmap mode the canvas uses
`image-rendering: pixelated` (with crisp-edge fallbacks) so pixel art remains sharp at high zoom.

## Vector rescale toggle

Enabling **Vector zoom** reruns the IVG rasterizer at the requested scale instead of stretching the
final bitmap. This produces smoother curves for vector content at the cost of additional work. The
toolbar button reflects the current mode through its label and pressed state.

### Safety limits

Vector rerenders observe the following safeguards to keep the WebAssembly module responsive:

- Maximum magnification is 10× (matching 1000 %).
- Canvas dimensions and total pixels are checked against allocator limits before rendering.
- Failed rerenders automatically fall back to bitmap zoom until the next successful vector render.

Failures surface as toast-style log messages in the console and leave the previous bitmap visible so
you can reduce the zoom or bounds before retrying.

## Background palette

Activate the **Background** button to open the swatch popup. The palette includes black, white,
gray, silver, the 12 additional CSS keyword colors requested by design, and a “None” entry that
restores the checkerboard for transparency checks. The surrounding stage uses a darker checkerboard
so the artboard bounds remain visible even when the canvas itself is transparent.

Selections apply instantly to the preview, persist across reloads, and close the popup. Press
`Escape` or click outside the dialog to cancel without changing the active background.

## Accessibility and usability notes

- Toolbar buttons have ARIA labels, consistent focus outlines, and keyboard access through `Tab`/`Shift+Tab`.
- The toolbar stays fixed at 44 px tall and scrolls horizontally if the window is narrow, keeping
  controls reachable on smaller screens.
- Dragging the divider between the editor and preview maintains minimum widths so the layout stays
  stable while adjusting panes.

## Known limitations

- The preview does not yet support panning. At extreme zoom levels you may need to scroll the page to
  view different portions of the canvas.
- Vector rerenders can take noticeably longer for complex IVGs. Progress feedback appears in the
  console but not the UI.
- Keyboard shortcuts use the browser defaults for zooming; some platforms may reserve these
  combinations for system-level features. The toolbar buttons remain available in those cases.

## Further reading

- [README – Canvas toolbar quick reference](../README.md#canvas-toolbar-quick-reference)
- [`tools/ivgfiddle/src/ivgfiddle.js`](../tools/ivgfiddle/src/ivgfiddle.js) for implementation
  details.
