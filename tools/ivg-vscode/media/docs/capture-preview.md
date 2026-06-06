# Capturing IVG Preview Evidence

Use these steps to record a screenshot or GIF of the IVG Preview panel when preparing release notes or manual verification logs.

1. Launch the Extension Development Host (press **F5** from `tools/ivg-vscode`).
2. Open an `.ivg` file with representative content or paste the **IVG Canvas Skeleton** snippet to generate a sample image.
3. Run **IVG Preview: Open Preview** to display the preview panel.
4. Wait for the toolbar to finish initializing (the zoom buttons become enabled) and confirm the canvas reflects the active file.
5. Resize the panel so the canvas, toolbar, and trace log are all visible.
6. Capture the window:
   - **Screenshot:** use the operating system shortcut (e.g., `Shift+Cmd+4` on macOS) to grab the dev host window.
   - **GIF:** record a short clip with a lightweight tool such as macOS QuickTime, Windows Snipping Tool, or `peek` on Linux, focusing on the preview area while you make a small edit so the rerender is visible.
7. Save the asset under `tools/ivg-vscode/media/docs/` using a descriptive filename like `preview-success.png` or `preview-refresh.gif`.
8. Reference the asset in documentation or release notes using a relative path (for example, `![IVG Preview panel](media/docs/preview-success.png)`).

Keeping the assets in the repository ensures future verifications use the same baseline instructions without introducing extra dependencies or cloud storage.
