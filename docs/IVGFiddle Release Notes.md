# IVGFiddle Toolbar Release Notes

## Summary

The IVGFiddle toolbar now provides built-in zoom controls, a background palette, and an optional
vector rescale mode to improve inspection of IVG artwork.

### Highlights

- Zoom presets from 25 % to 1000 % with keyboard shortcuts and persistent state.
- Vector zoom toggle that reruns the rasterizer at the active magnification with heap safety
  checks and bitmap fallback after failures.
- Background popup covering black, white, gray, silver, the remaining 12 web-safe colors, and a
  checkerboard “None” option with a darker stage outline to delineate canvas bounds.
- Toolbar layout refinements that stabilize control sizing, maintain focus outlines, and support
  horizontal scrolling on narrow viewports.

### Known limitations

- Extremely large bounds may still trigger vector fallback if the requested pixel budget exceeds the
  WebAssembly heap. The previous bitmap remains visible and bitmap zoom continues to function.
- Canvas panning is not yet available; use the browser scrollbars when working at high zoom levels.
- Rendering progress is reported in the console; the UI does not currently show a spinner.

### Accessibility considerations

- All interactive elements provide ARIA labels, keyboard focus states, and preserved tab order.
- The background popup traps focus while open and closes on `Escape` or outside clicks.
- The toolbar remains 44 px tall, preventing layout jumps when toggling modes or zoom levels.

### Upgrade notes

- Clear any stored `ivgBackgroundColor` values set to `transparent`; the new palette normalizes the
  key to `none` while preserving the transparent checkerboard behavior.
- Projects embedding the compiled HTML should update `tools/ivgfiddle/output/ivgfiddle.html` and the
  accompanying scripts to pick up the new controls.
