# IVGFiddle VS Code Extension (Milestone 3 In Progress)

This folder contains the work-in-progress extension that embeds the IVGFiddle renderer inside Visual Studio Code. The initial milestone focuses on preparing the project structure, static assets, and helper scripts without introducing extra build dependencies. The bundled webview omits the ACE editor so the VS Code panel operates strictly as a preview surface that will render `.ivg` documents provided by the host extension logic.

## Folder Layout

- `src/` – TypeScript source for the extension entry point.
- `media/` – Static IVGFiddle assets copied from `tools/ivgfiddle/output/`.
- `scripts/` – Utility scripts for synchronizing the media folder with the upstream IVGFiddle build output.

## Building the Extension

1. Install dependencies locally (only TypeScript and VS Code types are required):

```bash
npm install
```

2. Compile the extension to produce `dist/extension.js`:

```bash
npm run compile
```

## Previewing `.ivg` Documents

1. Open or create a `.ivg` file in VS Code. The extension contributes a simple language configuration so the document is tagged with the `ivg` language ID automatically.
2. Run **IVGFiddle: Open** from the Command Palette. If the panel is already open it will be focused instead of spawning duplicates.
3. The preview canvas renders the active document after the WebAssembly runtime loads. As you edit the file, the extension debounces updates (150 ms) and re-renders without additional commands.
4. A status bar entry titled `IVGFiddle Preview` shows the currently synchronized filename. It disappears whenever the active editor is not an `.ivg` document or when the document closes.
5. Close the panel to stop synchronization; reopening the panel reuses the current editor contents.

## Updating IVGFiddle Assets

Whenever files under `tools/ivgfiddle/src/` change, rebuild and re-sync the assets before committing:

```bash
bash tools/ivgfiddle-vscode/scripts/sync-assets.sh --build
```

On Windows, run the accompanying script from any command prompt:

```cmd
scripts\sync-assets.cmd --build
```

Both scripts rebuild IVGFiddle when `--build` is supplied and mirror the fresh output into `media/`. Re-run them after every change to the IVGFiddle web app to keep the extension bundle up to date.
