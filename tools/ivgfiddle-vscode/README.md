# IVGFiddle VS Code Extension (Milestone 1)

This folder contains the work-in-progress extension that embeds the IVGFiddle web application inside Visual Studio Code. The initial milestone focuses on preparing the project structure, static assets, and helper scripts without introducing extra build dependencies.

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
