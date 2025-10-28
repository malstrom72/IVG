"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.activate = activate;
exports.deactivate = deactivate;
const vscode = __importStar(require("vscode"));
const crypto_1 = require("crypto");
let ivgPanel;
let webviewReady = false;
let statusBarItem;
let scheduledDocument;
let lastPreviewDocumentUri;
let currentStatusDocumentUri;
let updateTimer;
let lastPreviewDurationMs;
let transientStatusMessage;
const pendingMessages = [];
const PREVIEW_LANGUAGE_ID = "ivg";
const DEFAULT_DEBOUNCE_MS = 150;
const CONFIG_SECTION = "ivgfiddle.preview";
const GENERAL_CONFIG_SECTION = "ivgfiddle";
const INCLUDE_CONFIG_SECTION = "ivgfiddle.includes";
const DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS = 150;
// Track include-eligible assets that must stay in sync with IVGFiddle. IMPD composition (`.impd`),
// inline vector graphics (`.ivg`), IVG fonts (`.ivgfont`), and raster fallbacks (`.png`) are
// pre-packaged alongside previews, so the watcher focuses on that set of extensions.
const INCLUDE_ASSET_GLOB = "**/*.{impd,ivg,ivgfont,png}";
const TRACE_OUTPUT_CHANNEL_NAME = "IVG Preview Trace";
const TRACE_SHOW_ACTION = "Show Trace Output";
let traceOutputChannel;
let traceOutputLines = [];
let previewConfig = readPreviewConfig();
let generalConfig = readGeneralConfig();
let includeConfig = readIncludeConfig();
let includeWatchers = [];
let includeWatcherDisposables = [];
let includeWatcherFolderCount = 0;
let includeWatcherCandidateFolderCount = 0;
let includeWatcherUnavailableMessage;
let includeWatcherEventCounts = {
    create: 0,
    change: 0,
    delete: 0,
};
const INCLUDE_CACHE_FOLDER = "include-cache";
const INCLUDE_MANIFEST_FILE_NAME = "include-manifest.json";
const INCLUDE_MANIFEST_VERSION = 1;
let extensionContext;
let includeManifestTimer;
let includeManifestStatus = "idle";
let includeManifestRevisionId;
let includeManifestEntryCount = 0;
let includeManifestTotalBytes = 0;
let includeManifestLastGeneratedAt;
let includeManifestLastError;
let includeManifestBuildInProgress = false;
let includeManifestRebuildQueued = false;
let includeManifestSnapshot;
let latestIncludeBundleMessage;
let includeManifestManualRefreshRequired = false;
let includeExplorerProvider;
let includeExplorerView;
let includeExplorerLastAutoRevealRevision;
let includeStatusBarItem;
const includeMissingRescanPaths = new Set();
const MIME_TYPE_BY_EXTENSION = new Map([
    ["impd", "application/json"],
    ["ivg", "application/xml"],
    ["ivgfont", "application/octet-stream"],
    ["png", "image/png"],
]);
function activate(context) {
    console.log("IVG Preview extension activated");
    extensionContext = context;
    traceOutputChannel = vscode.window.createOutputChannel(TRACE_OUTPUT_CHANNEL_NAME);
    context.subscriptions.push(traceOutputChannel);
    clearTraceOutput();
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    statusBarItem.command = "ivgfiddle.open";
    statusBarItem.hide();
    context.subscriptions.push(statusBarItem);
    includeStatusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 99);
    includeStatusBarItem.command = "ivgfiddle.includes.focusExplorer";
    includeStatusBarItem.name = "IVG Include Status";
    includeStatusBarItem.hide();
    context.subscriptions.push(includeStatusBarItem);
    includeExplorerProvider = new IncludeExplorerProvider();
    includeExplorerView = vscode.window.createTreeView("ivgfiddleIncludesExplorer", {
        treeDataProvider: includeExplorerProvider,
        showCollapseAll: false,
    });
    context.subscriptions.push(includeExplorerView);
    context.subscriptions.push(includeExplorerProvider);
    initializeIncludeWatchers(context);
    context.subscriptions.push({
        dispose: disposeIncludeWatchers,
    });
    const openCommand = vscode.commands.registerCommand("ivgfiddle.open", async () => {
        if (ivgPanel) {
            ivgPanel.reveal(ivgPanel.viewColumn ?? vscode.ViewColumn.Active);
            return;
        }
        const initialDocument = getActiveIvgDocument();
        const panel = vscode.window.createWebviewPanel("ivgfiddle", "IVG Preview", { viewColumn: vscode.ViewColumn.Active, preserveFocus: false }, {
            enableScripts: true,
            localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, "media")],
        });
        try {
            panel.webview.html = await getWebviewContent(context.extensionUri, panel.webview);
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`Failed to load IVG Preview: ${message}`);
            console.error("Failed to load IVG Preview webview", error);
            panel.dispose();
            return;
        }
        ivgPanel = panel;
        webviewReady = false;
        pendingMessages.length = 0;
        seedLatestIncludeBundleMessage();
        panel.webview.onDidReceiveMessage((message) => {
            if (!message || typeof message !== "object") {
                return;
            }
            const type = message.type;
            if (type === "ready") {
                webviewReady = true;
                flushPendingMessages();
                syncActiveDocument("panelFocus");
                return;
            }
            if (type === "trace") {
                const payload = message.message;
                processTraceMessage(payload);
                return;
            }
            if (type === "status") {
                processStatusMessage(message);
            }
        });
        panel.onDidChangeViewState(() => {
            if (panel.visible) {
                syncActiveDocument("panelFocus");
            }
        });
        panel.onDidDispose(() => {
            ivgPanel = undefined;
            webviewReady = false;
            pendingMessages.length = 0;
            lastPreviewDurationMs = undefined;
            clearTransientStatusMessage();
            hideStatusBar();
        });
        if (initialDocument) {
            syncDocument(initialDocument, "open");
        }
        else {
            syncActiveDocument("open");
        }
    });
    context.subscriptions.push(openCommand);
    context.subscriptions.push(vscode.commands.registerCommand("ivgfiddle.refreshPreview", handleRefreshPreviewCommand), vscode.commands.registerCommand("ivgfiddle.clearTrace", handleClearTraceCommand), vscode.commands.registerCommand("ivgfiddle.includes.focusExplorer", handleFocusIncludeExplorerCommand), vscode.commands.registerCommand("ivgfiddle.includes.rescan", handleRescanIncludesCommand), vscode.commands.registerCommand("ivgfiddle.includes.openAsset", handleOpenIncludeAssetCommand), vscode.commands.registerCommand("ivgfiddle.includes.revealAsset", handleRevealIncludeAssetCommand), vscode.commands.registerCommand("ivgfiddle.includes.copyMountPath", handleCopyIncludeMountPathCommand));
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((document) => {
        if (isIvgDocument(document)) {
            syncDocument(document, "open");
        }
    }), vscode.workspace.onDidChangeTextDocument((event) => {
        if (isIvgDocument(event.document)) {
            scheduleDocument(event.document);
        }
    }), vscode.window.onDidChangeActiveTextEditor((editor) => {
        if (editor) {
            syncActiveDocument("focus");
        }
        else if (ivgPanel && ivgPanel.active) {
            syncActiveDocument("panelFocus");
        }
        else {
            syncActiveDocument("clear");
        }
    }), vscode.workspace.onDidCloseTextDocument((document) => {
        if (scheduledDocument && scheduledDocument === document) {
            scheduledDocument = undefined;
        }
        if (lastPreviewDocumentUri === document.uri.toString()) {
            lastPreviewDocumentUri = undefined;
        }
        if (currentStatusDocumentUri === document.uri.toString()) {
            const active = getActiveIvgDocument();
            if (active) {
                syncDocument(active, "focus");
                return;
            }
            const fallback = getLastPreviewDocument();
            if (fallback) {
                showStatusBar(fallback, "focus");
                return;
            }
            hideStatusBar();
        }
    }), vscode.workspace.onDidChangeWorkspaceFolders(() => {
        initializeIncludeWatchers(context);
    }), vscode.workspace.onDidChangeConfiguration((event) => {
        if (event.affectsConfiguration(`${CONFIG_SECTION}.autoRefresh`) || event.affectsConfiguration(`${CONFIG_SECTION}.debounceMs`)) {
            previewConfig = readPreviewConfig();
            refreshStatusBar();
            if (scheduledDocument && previewConfig.autoRefresh) {
                syncDocument(scheduledDocument, "change");
            }
        }
        if (event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.syncOnOpen`) ||
            event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.webviewUpdateDelay`)) {
            generalConfig = readGeneralConfig();
            refreshStatusBar();
            if (scheduledDocument && previewConfig.autoRefresh) {
                syncDocument(scheduledDocument, "change");
            }
        }
        if (event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.syncOnOpen`) ||
            event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.webviewUpdateDelay`)) {
            generalConfig = readGeneralConfig();
            refreshStatusBar();
            if (scheduledDocument && previewConfig.autoRefresh) {
                syncDocument(scheduledDocument, "change");
            }
        }
        if (event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`) ||
            event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.manifestEnabled`) ||
            event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRescan`) ||
            event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.rescanDebounceMs`) ||
            event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRevealExplorer`)) {
            includeConfig = readIncludeConfig();
            if (event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`) ||
                event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.manifestEnabled`)) {
                initializeIncludeWatchers(context);
            }
            else {
                refreshIncludeSurfaces();
            }
            if (includeConfig.autoRescan) {
                includeManifestManualRefreshRequired = false;
            }
            if (includeConfig.watchersEnabled &&
                includeConfig.manifestEnabled &&
                includeConfig.autoRescan &&
                (event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRescan`) ||
                    event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.rescanDebounceMs`))) {
                scheduleIncludeManifestBuild("configuration");
            }
            refreshStatusBar();
            refreshIncludeSurfaces();
        }
    }));
    refreshIncludeSurfaces();
}
function deactivate() {
    // Intentionally empty; no teardown required for the bootstrap milestone.
}
async function getWebviewContent(extensionUri, webview) {
    const mediaRoot = vscode.Uri.joinPath(extensionUri, "media");
    const htmlUri = vscode.Uri.joinPath(mediaRoot, "ivgfiddle.html");
    const nonce = generateNonce();
    let html;
    try {
        const raw = await vscode.workspace.fs.readFile(htmlUri);
        html = new TextDecoder("utf-8").decode(raw);
    }
    catch (readError) {
        throw new Error("ivgfiddle.html is missing from the media directory.");
    }
    const csp = [
        "default-src 'none'",
        `img-src ${webview.cspSource} data:`,
        `script-src 'nonce-${nonce}' 'wasm-unsafe-eval'`,
        `style-src ${webview.cspSource}`,
        `font-src ${webview.cspSource}`,
    ].join("; ");
    html = html.replace("<head>", `<head>
		<meta http-equiv="Content-Security-Policy" content="${csp}">`);
    const attributeRegex = /(src|href)="\.\/([^"]+)"/g;
    const resourceMap = new Map();
    for (const match of html.matchAll(attributeRegex)) {
        const relPath = match[2];
        if (!relPath || resourceMap.has(relPath)) {
            continue;
        }
        const segments = relPath.split("/");
        const assetUri = vscode.Uri.joinPath(mediaRoot, ...segments);
        try {
            await vscode.workspace.fs.stat(assetUri);
        }
        catch (statError) {
            throw new Error(`Missing asset referenced in ivgfiddle.html: ${relPath}`);
        }
        resourceMap.set(relPath, webview.asWebviewUri(assetUri).toString());
    }
    html = html.replace(attributeRegex, (_match, attr, relPath) => {
        const mapped = resourceMap.get(relPath);
        return mapped ? `${attr}="${mapped}"` : `${attr}="${relPath}"`;
    });
    html = html.replace(/<script([^>]*)>/g, (_match, attrs) => `<script${attrs} nonce="${nonce}">`);
    return html;
}
function generateNonce() {
    const possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    let text = "";
    for (let i = 0; i < 32; i += 1) {
        text += possible.charAt(Math.floor(Math.random() * possible.length));
    }
    return text;
}
function isIvgDocument(document) {
    const lowerPath = document.uri.fsPath.toLowerCase();
    if (lowerPath.endsWith(".ivg")) {
        return true;
    }
    if (document.uri.scheme === "untitled" && document.languageId === PREVIEW_LANGUAGE_ID) {
        return true;
    }
    return false;
}
function scheduleDocument(document) {
    scheduledDocument = document;
    if (!previewConfig.autoRefresh) {
        if (updateTimer) {
            clearTimeout(updateTimer);
            updateTimer = undefined;
        }
        showStatusBar(document, "manualPending");
        return;
    }
    if (updateTimer) {
        clearTimeout(updateTimer);
    }
    updateTimer = setTimeout(() => {
        updateTimer = undefined;
        if (scheduledDocument) {
            syncDocument(scheduledDocument, "change");
        }
    }, Math.max(0, previewConfig.debounceMs));
}
function syncActiveDocument(reason) {
    if (reason === "clear") {
        const fallback = getLastPreviewDocument();
        if (fallback) {
            showStatusBar(fallback, "focus");
            return;
        }
        hideStatusBar();
        return;
    }
    const activeDocument = getActiveIvgDocument();
    if (activeDocument) {
        const resolved = reason === "focus" ? "focus" : "open";
        syncDocument(activeDocument, resolved);
        return;
    }
    const fallback = getLastPreviewDocument();
    if (fallback) {
        if (reason === "panelFocus") {
            syncDocument(fallback, "focus");
            return;
        }
        showStatusBar(fallback, "focus");
        return;
    }
    hideStatusBar();
}
function syncDocument(document, reason) {
    if (!ivgPanel) {
        return;
    }
    const source = document.getText();
    const fileName = fileNameFromDocument(document);
    const status = reason === "change" ? `Updating preview for ${fileName}…` : `Rendering ${fileName}`;
    if (reason === "open" && !generalConfig.syncOnOpen) {
        showStatusBar(document, "deferred");
        return;
    }
    queueMessage({
        type: "setSource",
        uri: document.uri.toString(),
        source,
        status,
    });
    lastPreviewDocumentUri = document.uri.toString();
    showStatusBar(document, reason);
}
function queueMessage(message) {
    if (!ivgPanel) {
        return;
    }
    if (!webviewReady) {
        pendingMessages.push(message);
        return;
    }
    postMessageToWebview(message);
}
function queueIncludeBundleMessage(message) {
    latestIncludeBundleMessage = message;
    if (!ivgPanel) {
        return;
    }
    if (!webviewReady) {
        prunePendingIncludeBundleMessages();
        pendingMessages.push(message);
        return;
    }
    postMessageToWebview(message);
}
function queueEmptyIncludeBundleMessage(reason) {
    const revision = `empty-${reason}-${Date.now().toString(36)}`;
    const manifest = {
        version: INCLUDE_MANIFEST_VERSION,
        revision,
        generatedAt: new Date().toISOString(),
        entries: [],
    };
    logIncludeTelemetry(`Include bundle cleared (${reason}).`);
    queueIncludeBundleMessage({
        type: "setIncludeBundle",
        revision,
        manifest,
        assets: [],
    });
    refreshIncludeSurfaces();
}
class IncludeAssetTreeItem extends vscode.TreeItem {
    constructor(entry) {
        super(entry.label, vscode.TreeItemCollapsibleState.None);
        this.entry = entry;
        this.contextValue = "ivgfiddleIncludeAsset";
        this.iconPath = new vscode.ThemeIcon("file");
        this.description = `${formatByteLength(entry.byteLength)} • ${entry.mimeType}`;
        const tooltipLines = [entry.mountPath, `Checksum: ${entry.checksum}`, `Size: ${formatByteLength(entry.byteLength)}`];
        if (entry.revision) {
            tooltipLines.push(`Revision: ${entry.revision}`);
        }
        this.tooltip = tooltipLines.join("\n");
        this.command = {
            command: "ivgfiddle.includes.openAsset",
            title: "Open Include Asset",
            arguments: [this],
        };
    }
}
class IncludeExplorerProvider {
    constructor() {
        this.changeEmitter = new vscode.EventEmitter();
        this.onDidChangeTreeData = this.changeEmitter.event;
    }
    dispose() {
        this.changeEmitter.dispose();
    }
    refresh() {
        this.changeEmitter.fire();
    }
    getTreeItem(element) {
        if (element.kind === "asset") {
            return new IncludeAssetTreeItem(element);
        }
        const item = new vscode.TreeItem(element.label, vscode.TreeItemCollapsibleState.None);
        item.contextValue = element.severity ? `ivgfiddleIncludeMessage.${element.severity}` : "ivgfiddleIncludeMessage.info";
        if (element.icon) {
            item.iconPath = new vscode.ThemeIcon(element.icon);
        }
        if (element.description) {
            item.description = element.description;
        }
        const tooltipLines = [];
        if (element.description) {
            tooltipLines.push(element.description);
        }
        if (element.detail) {
            tooltipLines.push(element.detail);
        }
        if (tooltipLines.length > 0) {
            item.tooltip = tooltipLines.join("\n");
        }
        if (element.command) {
            item.command = element.command;
        }
        return item;
    }
    getChildren(element) {
        if (element) {
            return [];
        }
        return buildIncludeExplorerEntries();
    }
}
function buildIncludeExplorerEntries() {
    if (!includeConfig.watchersEnabled) {
        return [
            {
                kind: "message",
                label: "Include watchers disabled",
                description: 'Enable "IVGFiddle › Includes: Watchers Enabled" to browse synchronized assets.',
                icon: "eye-closed",
                severity: "warning",
                command: {
                    command: "workbench.action.openSettings",
                    title: "Open Include Settings",
                    arguments: ["ivgfiddle.includes.watchersEnabled"],
                },
            },
        ];
    }
    if (includeWatcherCandidateFolderCount === 0) {
        return [
            {
                kind: "message",
                label: "Workspace required",
                description: includeWatcherUnavailableMessage ?? `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB}).`,
                icon: "workspace-untrusted",
                severity: "info",
            },
        ];
    }
    if (includeWatcherFolderCount === 0) {
        return [
            {
                kind: "message",
                label: "Include watchers initializing…",
                description: `Scanning ${includeWatcherCandidateFolderCount} workspace folder${includeWatcherCandidateFolderCount === 1 ? "" : "s"} for ${INCLUDE_ASSET_GLOB}.`,
                icon: "sync~spin",
                severity: "info",
            },
        ];
    }
    const entries = [];
    entries.push({
        kind: "message",
        label: `Watching ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"}`,
        description: `Pattern: ${INCLUDE_ASSET_GLOB}`,
        detail: `Events — create: ${includeWatcherEventCounts.create}, change: ${includeWatcherEventCounts.change}, delete: ${includeWatcherEventCounts.delete}`,
        icon: "watch",
        severity: "info",
    });
    if (!includeConfig.manifestEnabled) {
        entries.push({
            kind: "message",
            label: "Include manifest disabled",
            description: 'Enable "IVGFiddle › Includes: Manifest Enabled" to build the synchronized bundle.',
            icon: "circle-slash",
            severity: "info",
            command: {
                command: "workbench.action.openSettings",
                title: "Open Include Settings",
                arguments: ["ivgfiddle.includes.manifestEnabled"],
            },
        });
        return entries;
    }
    if (!includeConfig.autoRescan) {
        entries.push({
            kind: "message",
            label: "Auto rescan disabled",
            description: 'Run "IVGFiddle: Rescan Include Assets" after editing include files.',
            icon: "history",
            severity: "info",
            command: {
                command: "ivgfiddle.includes.rescan",
                title: "Rescan Include Assets",
            },
        });
    }
    if (includeManifestStatus === "error") {
        entries.push({
            kind: "message",
            label: "Include manifest error",
            description: includeManifestLastError ?? "Check the trace output for additional details.",
            icon: "error",
            severity: "error",
            command: { command: "ivgfiddle.open", title: "Open IVG Preview" },
        });
    }
    else if (includeManifestManualRefreshRequired) {
        entries.push({
            kind: "message",
            label: "Manual rescan required",
            description: "Recent include edits require rebuilding the manifest.",
            detail: 'Run "IVGFiddle: Rescan Include Assets" to refresh the bundle.',
            icon: "alert",
            severity: "warning",
            command: {
                command: "ivgfiddle.includes.rescan",
                title: "Rescan Include Assets",
            },
        });
    }
    else if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
        entries.push({
            kind: "message",
            label: "Building include manifest…",
            description: "Collecting updated include assets.",
            icon: "sync~spin",
            severity: "info",
        });
    }
    else if (includeManifestStatus === "ready") {
        entries.push({
            kind: "message",
            label: `Manifest ready — ${includeManifestEntryCount} ${includeManifestEntryCount === 1 ? "asset" : "assets"}`,
            description: includeManifestRevisionId ? `Revision ${includeManifestRevisionId}` : undefined,
            detail: includeManifestLastGeneratedAt ? `Generated at ${includeManifestLastGeneratedAt}` : undefined,
            icon: "check",
            severity: "info",
        });
    }
    const manifest = includeManifestSnapshot;
    if (manifest && manifest.entries.length > 0) {
        for (const entry of manifest.entries) {
            const relativePath = entry.mountPath.startsWith("/") ? entry.mountPath.slice(1) : entry.mountPath;
            entries.push({
                kind: "asset",
                label: relativePath,
                relativePath,
                mountPath: entry.mountPath,
                byteLength: entry.byteLength,
                checksum: entry.checksum,
                mimeType: entry.mimeType,
                revision: manifest.revision,
            });
        }
    }
    else if (includeManifestStatus === "ready") {
        entries.push({
            kind: "message",
            label: "No include assets discovered yet",
            description: "Edit or add IMPD/IVG assets matching the include glob to populate the bundle.",
            icon: "file-add",
            severity: "info",
        });
    }
    return entries;
}
function getIncludeAssetEntryFromArgument(argument) {
    if (!argument) {
        return undefined;
    }
    if (argument instanceof IncludeAssetTreeItem) {
        return argument.entry;
    }
    const candidate = argument;
    if (candidate && candidate.kind === "asset" && typeof candidate.relativePath === "string") {
        return candidate;
    }
    return undefined;
}
async function resolveIncludeAssetUri(relativePath) {
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders || workspaceFolders.length === 0) {
        return undefined;
    }
    const normalized = relativePath.replace(/\\/g, "/").replace(/^\/+/u, "");
    const segments = normalized.split("/").filter(Boolean);
    for (const folder of workspaceFolders) {
        let candidateSegments = segments;
        if (workspaceFolders.length > 1 && segments.length > 1) {
            const firstSegment = segments[0];
            if (isWorkspaceFolderSegment(firstSegment, folder)) {
                candidateSegments = segments.slice(1);
            }
        }
        const candidate = candidateSegments.length > 0 ? vscode.Uri.joinPath(folder.uri, ...candidateSegments) : folder.uri;
        try {
            await vscode.workspace.fs.stat(candidate);
            return candidate;
        }
        catch (error) {
            // Continue probing additional workspace folders.
        }
    }
    return undefined;
}
function isWorkspaceFolderSegment(segment, folder) {
    if (segment === folder.name || equalsFileSystemInsensitive(segment, folder.name)) {
        return true;
    }
    const baseName = getWorkspaceFolderBaseName(folder);
    return segment === baseName || equalsFileSystemInsensitive(segment, baseName);
}
function getWorkspaceFolderBaseName(folder) {
    const fsPath = folder.uri.fsPath;
    if (!fsPath) {
        return folder.name;
    }
    const normalized = fsPath.replace(/\\+/g, "/");
    const parts = normalized.split("/").filter(Boolean);
    return parts.length > 0 ? parts[parts.length - 1] : folder.name;
}
function equalsFileSystemInsensitive(a, b) {
    if (a === b) {
        return true;
    }
    return a.localeCompare(b, undefined, { sensitivity: "accent" }) === 0;
}
function formatByteLength(byteLength) {
    if (!Number.isFinite(byteLength) || byteLength < 0) {
        return `${byteLength} B`;
    }
    if (byteLength < 1024) {
        return `${byteLength} B`;
    }
    const units = ["KB", "MB", "GB", "TB"];
    let value = byteLength;
    let unitIndex = 0;
    while (value >= 1024 && unitIndex < units.length - 1) {
        value /= 1024;
        unitIndex += 1;
    }
    const precision = value >= 100 ? 0 : value >= 10 ? 1 : 2;
    return `${value.toFixed(precision)} ${units[unitIndex]}`;
}
function prunePendingIncludeBundleMessages() {
    for (let index = pendingMessages.length - 1; index >= 0; index -= 1) {
        const entry = pendingMessages[index];
        if (isIncludeBundleMessage(entry)) {
            pendingMessages.splice(index, 1);
        }
    }
}
function seedLatestIncludeBundleMessage() {
    if (!ivgPanel || !latestIncludeBundleMessage) {
        return;
    }
    prunePendingIncludeBundleMessages();
    pendingMessages.push(latestIncludeBundleMessage);
}
function flushPendingMessages() {
    if (!ivgPanel) {
        pendingMessages.length = 0;
        return;
    }
    while (pendingMessages.length > 0) {
        const message = pendingMessages.shift();
        if (message) {
            postMessageToWebview(message);
        }
    }
}
function isIncludeBundleMessage(value) {
    if (!value || typeof value !== "object") {
        return false;
    }
    const candidate = value;
    return candidate.type === "setIncludeBundle";
}
function postMessageToWebview(message) {
    if (!ivgPanel) {
        return;
    }
    const delay = Math.max(0, generalConfig.webviewUpdateDelay);
    if (delay === 0) {
        ivgPanel.webview.postMessage(message);
        return;
    }
    setTimeout(() => {
        if (ivgPanel) {
            ivgPanel.webview.postMessage(message);
        }
    }, delay);
}
function fileNameFromDocument(document) {
    const fsPath = document.fileName;
    const forwardSlash = fsPath.lastIndexOf("/");
    const backwardSlash = fsPath.lastIndexOf(String.fromCharCode(92));
    const index = Math.max(forwardSlash, backwardSlash);
    if (index >= 0) {
        return fsPath.substring(index + 1);
    }
    return fsPath;
}
function showStatusBar(document, reason) {
    if (!statusBarItem) {
        return;
    }
    const fileName = fileNameFromDocument(document);
    let icon = "sync";
    let suffix = "";
    if (reason === "change") {
        icon = "sync~spin";
    }
    else if (reason === "manualPending") {
        icon = "clock";
        suffix = " — refresh required";
    }
    else if (reason === "deferred") {
        icon = "clock";
        suffix = " — sync on open disabled";
    }
    if (typeof lastPreviewDurationMs === "number" && lastPreviewDurationMs >= 0 && reason !== "manualPending" && reason !== "deferred") {
        suffix = `${suffix} • ${Math.round(lastPreviewDurationMs)} ms`;
    }
    const includeSummary = getIncludeStatusSummary();
    suffix = suffix ? `${suffix} • ${includeSummary.label}` : ` • ${includeSummary.label}`;
    statusBarItem.text = `$(${icon}) IVG Preview: ${fileName}${suffix}`;
    const tooltipLines = [document.uri.fsPath];
    if (!previewConfig.autoRefresh) {
        tooltipLines.push("Auto-refresh disabled");
    }
    if (!generalConfig.syncOnOpen) {
        tooltipLines.push("Sync on open disabled");
    }
    if (typeof lastPreviewDurationMs === "number" && lastPreviewDurationMs >= 0 && reason !== "manualPending" && reason !== "deferred") {
        tooltipLines.push(`Last render: ${Math.round(lastPreviewDurationMs)} ms`);
    }
    tooltipLines.push(...includeSummary.tooltipLines);
    statusBarItem.tooltip = tooltipLines.join("\n");
    statusBarItem.show();
    currentStatusDocumentUri = document.uri.toString();
}
function getIncludeStatusSummary() {
    if (!includeConfig.watchersEnabled) {
        return {
            label: "includes off",
            tooltipLines: ["Include watchers disabled"],
        };
    }
    if (includeWatcherFolderCount === 0) {
        if (includeWatcherCandidateFolderCount === 0) {
            return {
                label: "includes unavailable",
                tooltipLines: [
                    includeWatcherUnavailableMessage
                        ? includeWatcherUnavailableMessage
                        : `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB})`,
                ],
            };
        }
        return {
            label: "includes pending",
            tooltipLines: [
                `Include watchers initializing across ${includeWatcherCandidateFolderCount} workspace folder${includeWatcherCandidateFolderCount === 1 ? "" : "s"} (pattern: ${INCLUDE_ASSET_GLOB})`,
            ],
        };
    }
    const tooltipLines = [
        `Include watchers active on ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"} (pattern: ${INCLUDE_ASSET_GLOB})`,
        `Include watcher events this session — create: ${includeWatcherEventCounts.create}, change: ${includeWatcherEventCounts.change}, delete: ${includeWatcherEventCounts.delete}`,
    ];
    if (!includeConfig.manifestEnabled) {
        const totalEvents = includeWatcherEventCounts.create + includeWatcherEventCounts.change + includeWatcherEventCounts.delete;
        const eventSuffix = totalEvents > 0 ? ` (+${totalEvents})` : "";
        const manualSuffix = includeConfig.autoRescan ? "" : " (manual)";
        tooltipLines.push("Include manifest disabled");
        if (!includeConfig.autoRescan) {
            tooltipLines.push('Include auto rescan disabled; run "IVGFiddle: Rescan Include Assets" after edits.');
        }
        return {
            label: `includes watching${eventSuffix}${manualSuffix}`,
            tooltipLines,
        };
    }
    if (!includeConfig.autoRescan) {
        tooltipLines.push("Include auto rescan disabled; manual rescan required after edits.");
    }
    if (includeManifestStatus === "error") {
        const detail = includeManifestLastError ? `: ${includeManifestLastError}` : "";
        tooltipLines.push(`Include manifest error${detail}`);
        return {
            label: "includes error",
            tooltipLines,
        };
    }
    if (includeManifestManualRefreshRequired) {
        tooltipLines.push("Include manifest awaiting manual rescan.");
        return {
            label: "includes refresh required",
            tooltipLines,
        };
    }
    if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
        tooltipLines.push(`Include manifest status: ${includeManifestStatus}`);
        return {
            label: "includes building",
            tooltipLines,
        };
    }
    if (includeManifestStatus === "ready") {
        const assetLabel = includeManifestEntryCount === 1 ? "asset" : "assets";
        const byteLabel = includeManifestTotalBytes === 1 ? "byte" : "bytes";
        const revisionLabel = includeManifestRevisionId ?? "unknown";
        tooltipLines.push(`Include manifest ready — revision ${revisionLabel}, ${includeManifestEntryCount} ${assetLabel}, ${includeManifestTotalBytes} ${byteLabel}`);
        if (includeManifestLastGeneratedAt) {
            tooltipLines.push(`Include manifest generated at ${includeManifestLastGeneratedAt}`);
        }
        if (!includeConfig.autoRescan) {
            tooltipLines.push("Manual rescan mode active.");
        }
        const manualSuffix = includeConfig.autoRescan ? "" : " (manual)";
        return {
            label: includeManifestEntryCount > 0
                ? `includes ready (${includeManifestEntryCount})${manualSuffix}`
                : `includes ready (empty)${manualSuffix}`,
            tooltipLines,
        };
    }
    tooltipLines.push("Include manifest idle");
    if (!includeConfig.autoRescan) {
        tooltipLines.push("Manual rescan mode active.");
    }
    const manualLabelSuffix = includeConfig.autoRescan ? "" : " (manual)";
    return {
        label: `includes watching${manualLabelSuffix}`,
        tooltipLines,
    };
}
function hideStatusBar() {
    if (statusBarItem) {
        statusBarItem.hide();
    }
    currentStatusDocumentUri = undefined;
}
function getActiveIvgDocument() {
    const editor = vscode.window.activeTextEditor;
    if (editor && isIvgDocument(editor.document)) {
        return editor.document;
    }
    return undefined;
}
function getLastPreviewDocument() {
    if (!lastPreviewDocumentUri) {
        return undefined;
    }
    return vscode.workspace.textDocuments.find((openDocument) => openDocument.uri.toString() === lastPreviewDocumentUri);
}
function handleRefreshPreviewCommand() {
    const targetDocument = getActiveIvgDocument() ?? getLastPreviewDocument();
    if (!targetDocument) {
        vscode.window.showInformationMessage("Open an IVG document to refresh the preview.");
        return;
    }
    if (!ivgPanel) {
        vscode.commands.executeCommand("ivgfiddle.open").then(() => {
            syncDocument(targetDocument, "change");
        });
        return;
    }
    syncDocument(targetDocument, "change");
}
function handleClearTraceCommand() {
    if (!ivgPanel) {
        vscode.window.showInformationMessage("Open the IVG Preview panel before clearing the trace.");
        return;
    }
    queueMessage({ type: "clearTrace" });
}
function handleFocusIncludeExplorerCommand() {
    void vscode.commands.executeCommand("ivgfiddleIncludesExplorer.focus");
}
async function handleRescanIncludesCommand() {
    if (!includeConfig.watchersEnabled) {
        await vscode.window.showInformationMessage('Enable "IVGFiddle › Includes: Watchers Enabled" to rescan include assets.');
        return;
    }
    if (!includeConfig.manifestEnabled) {
        await vscode.window.showInformationMessage('Enable "IVGFiddle › Includes: Manifest Enabled" before rebuilding the include bundle.');
        return;
    }
    if (includeWatcherFolderCount === 0) {
        await vscode.window.showInformationMessage("Attach a file-backed workspace folder so the include watcher can rescan your assets.");
        return;
    }
    logIncludeTelemetry("Manual include manifest rescan requested.");
    includeMissingRescanPaths.clear();
    scheduleIncludeManifestBuild("manual");
    vscode.window.setStatusBarMessage("Rescanning include assets…", 2000);
}
async function handleOpenIncludeAssetCommand(target) {
    const entry = getIncludeAssetEntryFromArgument(target);
    if (!entry) {
        await vscode.window.showInformationMessage("Select an include asset to open.");
        return;
    }
    const uri = await resolveIncludeAssetUri(entry.relativePath);
    if (!uri) {
        await vscode.window.showWarningMessage(`Unable to locate include asset ${entry.relativePath} in the current workspace.`);
        return;
    }
    const document = await vscode.workspace.openTextDocument(uri);
    await vscode.window.showTextDocument(document, { preview: false });
}
async function handleRevealIncludeAssetCommand(target) {
    const entry = getIncludeAssetEntryFromArgument(target);
    if (!entry) {
        await vscode.window.showInformationMessage("Select an include asset to reveal.");
        return;
    }
    const uri = await resolveIncludeAssetUri(entry.relativePath);
    if (!uri) {
        await vscode.window.showWarningMessage(`Unable to reveal include asset ${entry.relativePath}; the file is missing from the current workspace.`);
        return;
    }
    await vscode.commands.executeCommand("revealFileInOS", uri);
}
async function handleCopyIncludeMountPathCommand(target) {
    const entry = getIncludeAssetEntryFromArgument(target);
    if (!entry) {
        await vscode.window.showInformationMessage("Select an include asset to copy its mount path.");
        return;
    }
    await vscode.env.clipboard.writeText(entry.mountPath);
    vscode.window.setStatusBarMessage(`Copied include mount path ${entry.mountPath}`, 2000);
}
function readPreviewConfig() {
    const config = vscode.workspace.getConfiguration(CONFIG_SECTION);
    const autoRefresh = config.get("autoRefresh", true);
    const configuredDebounce = config.get("debounceMs", DEFAULT_DEBOUNCE_MS);
    const debounceMs = Number.isFinite(configuredDebounce) && configuredDebounce >= 0 ? configuredDebounce : DEFAULT_DEBOUNCE_MS;
    return {
        autoRefresh,
        debounceMs,
    };
}
function readGeneralConfig() {
    const config = vscode.workspace.getConfiguration(GENERAL_CONFIG_SECTION);
    const syncOnOpen = config.get("syncOnOpen", true);
    const configuredDelay = config.get("webviewUpdateDelay", 0);
    const webviewUpdateDelay = Number.isFinite(configuredDelay) && configuredDelay >= 0 ? configuredDelay : 0;
    return {
        syncOnOpen,
        webviewUpdateDelay,
    };
}
function readIncludeConfig() {
    const config = vscode.workspace.getConfiguration(INCLUDE_CONFIG_SECTION);
    const watchersEnabled = config.get("watchersEnabled", true);
    const manifestEnabled = config.get("manifestEnabled", false);
    const autoRescan = config.get("autoRescan", true);
    const configuredDebounce = config.get("rescanDebounceMs", DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS);
    const rescanDebounceMs = Number.isFinite(configuredDebounce) && configuredDebounce >= 0 ? configuredDebounce : DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS;
    const autoRevealExplorer = config.get("autoRevealExplorer", true);
    return {
        watchersEnabled,
        manifestEnabled,
        autoRescan,
        rescanDebounceMs,
        autoRevealExplorer,
    };
}
function initializeIncludeWatchers(context) {
    disposeIncludeWatchers();
    includeWatcherEventCounts = {
        create: 0,
        change: 0,
        delete: 0,
    };
    includeWatcherFolderCount = 0;
    includeWatcherCandidateFolderCount = 0;
    includeWatcherUnavailableMessage = undefined;
    includeManifestSnapshot = undefined;
    latestIncludeBundleMessage = undefined;
    includeManifestManualRefreshRequired = false;
    includeExplorerLastAutoRevealRevision = undefined;
    includeMissingRescanPaths.clear();
    if (!includeConfig.watchersEnabled) {
        logIncludeTelemetry("Include watchers disabled by configuration.");
        cancelIncludeManifestScheduling();
        includeManifestStatus = "idle";
        includeManifestRevisionId = undefined;
        includeManifestEntryCount = 0;
        includeManifestTotalBytes = 0;
        includeManifestLastGeneratedAt = undefined;
        includeManifestLastError = undefined;
        includeManifestBuildInProgress = false;
        includeManifestRebuildQueued = false;
        queueEmptyIncludeBundleMessage("watchers-disabled");
        refreshStatusBar();
        refreshIncludeSurfaces();
        return;
    }
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders || workspaceFolders.length === 0) {
        logIncludeTelemetry("Include watchers pending workspace folders.");
        includeWatcherUnavailableMessage = `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB})`;
        includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
        queueEmptyIncludeBundleMessage("watchers-unavailable");
        refreshStatusBar();
        refreshIncludeSurfaces();
        return;
    }
    const fileBackedFolders = workspaceFolders.filter((folder) => folder.uri.scheme === "file");
    includeWatcherCandidateFolderCount = fileBackedFolders.length;
    if (fileBackedFolders.length === 0) {
        logIncludeTelemetry("Include watchers unavailable for non-file workspace folders.");
        includeWatcherUnavailableMessage = "Include watchers require file-backed workspace folders.";
        includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
        queueEmptyIncludeBundleMessage("watchers-unavailable");
        refreshStatusBar();
        refreshIncludeSurfaces();
        return;
    }
    for (const folder of fileBackedFolders) {
        const pattern = new vscode.RelativePattern(folder, INCLUDE_ASSET_GLOB);
        const watcher = vscode.workspace.createFileSystemWatcher(pattern);
        includeWatchers.push(watcher);
        includeWatcherDisposables.push(watcher.onDidCreate((uri) => handleIncludeWatcherEvent("create", uri)), watcher.onDidChange((uri) => handleIncludeWatcherEvent("change", uri)), watcher.onDidDelete((uri) => handleIncludeWatcherEvent("delete", uri)));
        includeWatcherFolderCount += 1;
    }
    logIncludeTelemetry(`Include watchers attached across ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"} using pattern ${INCLUDE_ASSET_GLOB}.`);
    if (!includeConfig.manifestEnabled) {
        logIncludeTelemetry("Include manifest generation disabled by configuration.");
        includeManifestStatus = "idle";
        includeManifestRevisionId = undefined;
        includeManifestEntryCount = 0;
        includeManifestTotalBytes = 0;
        includeManifestLastGeneratedAt = undefined;
        includeManifestLastError = undefined;
        includeManifestBuildInProgress = false;
        includeManifestRebuildQueued = false;
        queueEmptyIncludeBundleMessage("manifest-disabled");
        refreshStatusBar();
        refreshIncludeSurfaces();
        return;
    }
    includeManifestStatus = "pending";
    refreshStatusBar();
    refreshIncludeSurfaces();
    scheduleIncludeManifestBuild("watcherInitialization");
}
function disposeIncludeWatchers() {
    for (const disposable of includeWatcherDisposables) {
        disposable.dispose();
    }
    includeWatcherDisposables = [];
    for (const watcher of includeWatchers) {
        watcher.dispose();
    }
    includeWatchers = [];
    if (includeWatcherFolderCount > 0) {
        logIncludeTelemetry("Include watchers disposed.");
    }
    includeWatcherFolderCount = 0;
    includeWatcherCandidateFolderCount = 0;
    includeWatcherUnavailableMessage = undefined;
    includeWatcherEventCounts = {
        create: 0,
        change: 0,
        delete: 0,
    };
    cancelIncludeManifestScheduling();
    includeManifestSnapshot = undefined;
    queueEmptyIncludeBundleMessage("watchers-disposed");
    latestIncludeBundleMessage = undefined;
    includeManifestManualRefreshRequired = false;
    includeExplorerLastAutoRevealRevision = undefined;
    includeMissingRescanPaths.clear();
    refreshIncludeSurfaces();
}
function handleIncludeWatcherEvent(kind, uri) {
    includeWatcherEventCounts[kind] += 1;
    const relativePath = vscode.workspace.asRelativePath(uri, false);
    logIncludeTelemetry(`Include asset ${kind} detected at ${relativePath} (events: create=${includeWatcherEventCounts.create}, change=${includeWatcherEventCounts.change}, delete=${includeWatcherEventCounts.delete}).`);
    refreshStatusBar();
    scheduleIncludeRefresh();
    includeMissingRescanPaths.clear();
    if (includeConfig.manifestEnabled) {
        scheduleIncludeManifestBuild("watcherEvent");
    }
    refreshIncludeSurfaces();
}
function scheduleIncludeRefresh() {
    const targetDocument = getActiveIvgDocument() ?? getLastPreviewDocument();
    if (!targetDocument) {
        return;
    }
    scheduleDocument(targetDocument);
}
function scheduleIncludeManifestBuild(reason) {
    if (!extensionContext || !includeConfig.watchersEnabled || !includeConfig.manifestEnabled) {
        return;
    }
    const autoRescanBlocked = reason === "watcherEvent" || reason === "traceMissing";
    if (autoRescanBlocked && !includeConfig.autoRescan) {
        if (!includeManifestManualRefreshRequired) {
            logIncludeTelemetry("Include manifest pending manual rescan (auto rescan disabled).");
        }
        includeManifestManualRefreshRequired = true;
        refreshIncludeSurfaces();
        return;
    }
    includeManifestManualRefreshRequired = false;
    includeManifestLastError = undefined;
    if (includeManifestBuildInProgress) {
        includeManifestRebuildQueued = true;
        includeManifestStatus = "building";
        refreshStatusBar();
        refreshIncludeSurfaces();
        return;
    }
    includeManifestStatus = "pending";
    refreshStatusBar();
    refreshIncludeSurfaces();
    if (includeManifestTimer) {
        clearTimeout(includeManifestTimer);
    }
    const delay = Math.max(0, includeConfig.rescanDebounceMs);
    includeManifestTimer = setTimeout(() => {
        includeManifestTimer = undefined;
        void buildIncludeManifest(reason);
    }, delay);
}
async function buildIncludeManifest(reason) {
    if (!extensionContext || !includeConfig.watchersEnabled || !includeConfig.manifestEnabled) {
        includeManifestBuildInProgress = false;
        includeManifestStatus = includeConfig.manifestEnabled ? includeManifestStatus : "idle";
        refreshStatusBar();
        return;
    }
    includeManifestBuildInProgress = true;
    includeManifestStatus = "building";
    refreshStatusBar();
    const startedAt = Date.now();
    logIncludeTelemetry(`Include manifest rebuild started (reason: ${reason}).`);
    try {
        const uris = await vscode.workspace.findFiles(INCLUDE_ASSET_GLOB);
        const records = [];
        let totalBytes = 0;
        for (const uri of uris) {
            try {
                const relativePath = getIncludeRelativePath(uri);
                const content = await vscode.workspace.fs.readFile(uri);
                const buffer = Buffer.from(content);
                const byteLength = buffer.byteLength;
                totalBytes += byteLength;
                const checksum = (0, crypto_1.createHash)("sha256").update(buffer).digest("hex");
                const mountPath = relativePath.startsWith("/") ? relativePath : `/${relativePath}`;
                const entry = {
                    mountPath,
                    byteLength,
                    checksum,
                    mimeType: inferIncludeMimeType(relativePath),
                };
                records.push({
                    relativePath,
                    content,
                    entry,
                });
            }
            catch (error) {
                const message = formatError(error);
                const relativePath = vscode.workspace.asRelativePath(uri, false);
                logIncludeTelemetry(`Failed to read include asset ${relativePath}: ${message}`);
            }
        }
        records.sort((a, b) => a.entry.mountPath.localeCompare(b.entry.mountPath));
        const revisionId = createIncludeRevisionId(records);
        const generatedAt = new Date().toISOString();
        const { manifest } = await persistIncludeRevision(extensionContext, revisionId, records, generatedAt);
        const assets = createIncludeBundleAssets(records);
        const serializedBytes = Buffer.byteLength(JSON.stringify({ manifest, assets }), "utf8");
        includeManifestSnapshot = manifest;
        includeManifestRevisionId = revisionId;
        includeManifestEntryCount = records.length;
        includeManifestTotalBytes = totalBytes;
        includeManifestLastGeneratedAt = generatedAt;
        includeManifestStatus = "ready";
        logIncludeTelemetry(`Include manifest ready (${records.length} asset${records.length === 1 ? "" : "s"}, ${totalBytes} byte${totalBytes === 1 ? "" : "s"}, revision ${revisionId}, ${Date.now() - startedAt} ms).`);
        logIncludeTelemetry(`Include bundle prepared (${assets.length} asset${assets.length === 1 ? "" : "s"}, ${serializedBytes} serialized byte${serializedBytes === 1 ? "" : "s"}) for revision ${revisionId}.`);
        queueIncludeBundleMessage({
            type: "setIncludeBundle",
            revision: revisionId,
            manifest,
            assets,
        });
        refreshIncludeSurfaces();
        maybeAutoRevealIncludeExplorer(revisionId);
    }
    catch (error) {
        includeManifestStatus = "error";
        includeManifestLastError = formatError(error);
        logIncludeTelemetry(`Include manifest rebuild failed: ${includeManifestLastError}`);
        includeManifestSnapshot = undefined;
        latestIncludeBundleMessage = undefined;
        refreshIncludeSurfaces();
    }
    includeManifestBuildInProgress = false;
    refreshStatusBar();
    refreshIncludeSurfaces();
    if (includeManifestRebuildQueued) {
        includeManifestRebuildQueued = false;
        scheduleIncludeManifestBuild("chained");
    }
}
function getIncludeRelativePath(uri) {
    const relative = vscode.workspace.asRelativePath(uri, false);
    const candidate = relative && relative !== uri.fsPath ? relative : uri.path;
    const normalized = candidate.replace(/\\/g, "/").replace(/^\.\//, "");
    return normalized.startsWith("/") ? normalized.slice(1) : normalized;
}
function inferIncludeMimeType(relativePath) {
    const lower = relativePath.toLowerCase();
    for (const [extension, mime] of MIME_TYPE_BY_EXTENSION) {
        if (lower.endsWith(`.${extension}`)) {
            return mime;
        }
    }
    return "application/octet-stream";
}
function createIncludeRevisionId(records) {
    const hash = (0, crypto_1.createHash)("sha256");
    for (const record of records) {
        hash.update(record.entry.mountPath);
        hash.update(record.entry.checksum);
    }
    const digest = hash.digest("hex").slice(0, 12);
    const timestamp = Date.now().toString(36);
    return `rev-${timestamp}-${digest}`;
}
function createIncludeBundleAssets(records) {
    return records.map((record) => ({
        path: record.relativePath,
        checksum: record.entry.checksum,
        mimeType: record.entry.mimeType,
        data: Buffer.from(record.content).toString("base64"),
    }));
}
async function persistIncludeRevision(context, revisionId, records, generatedAt) {
    const storageRoot = vscode.Uri.joinPath(context.globalStorageUri, INCLUDE_CACHE_FOLDER);
    await vscode.workspace.fs.createDirectory(storageRoot);
    const revisionRoot = vscode.Uri.joinPath(storageRoot, revisionId);
    await vscode.workspace.fs.createDirectory(revisionRoot);
    for (const record of records) {
        const segments = record.relativePath.split("/").filter(Boolean);
        if (segments.length > 1) {
            await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(revisionRoot, ...segments.slice(0, -1)));
        }
        const target = vscode.Uri.joinPath(revisionRoot, ...segments);
        await vscode.workspace.fs.writeFile(target, record.content);
    }
    const manifest = {
        version: INCLUDE_MANIFEST_VERSION,
        revision: revisionId,
        generatedAt,
        entries: records.map((record) => record.entry),
    };
    const manifestUri = vscode.Uri.joinPath(revisionRoot, INCLUDE_MANIFEST_FILE_NAME);
    await vscode.workspace.fs.writeFile(manifestUri, Buffer.from(JSON.stringify(manifest, null, 2), "utf8"));
    await pruneStaleIncludeRevisions(storageRoot, revisionId);
    return { manifest };
}
async function pruneStaleIncludeRevisions(storageRoot, activeRevisionId) {
    try {
        const entries = await vscode.workspace.fs.readDirectory(storageRoot);
        for (const [name, type] of entries) {
            if (name === activeRevisionId) {
                continue;
            }
            const target = vscode.Uri.joinPath(storageRoot, name);
            await vscode.workspace.fs.delete(target, {
                recursive: true,
                useTrash: false,
            });
        }
    }
    catch (error) {
        logIncludeTelemetry(`Failed to prune stale include revisions: ${formatError(error)}`);
    }
}
function cancelIncludeManifestScheduling() {
    if (includeManifestTimer) {
        clearTimeout(includeManifestTimer);
        includeManifestTimer = undefined;
    }
    includeManifestRebuildQueued = false;
}
function refreshIncludeSurfaces() {
    refreshIncludeStatusBarItem();
    if (includeExplorerProvider) {
        includeExplorerProvider.refresh();
    }
}
function refreshIncludeStatusBarItem() {
    if (!includeStatusBarItem) {
        return;
    }
    const summary = getIncludeStatusSummary();
    let icon = "package";
    if (!includeConfig.watchersEnabled) {
        icon = "eye-closed";
    }
    else if (includeManifestStatus === "error") {
        icon = "warning";
    }
    else if (includeManifestManualRefreshRequired) {
        icon = "history";
    }
    else if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
        icon = "sync~spin";
    }
    else if (summary.label.startsWith("includes unavailable")) {
        icon = "workspace-untrusted";
    }
    else if (summary.label.startsWith("includes pending")) {
        icon = "sync";
    }
    const tooltipLines = [...summary.tooltipLines];
    if (tooltipLines.length > 0) {
        tooltipLines.push("");
    }
    tooltipLines.push("Click to open the IVG Include explorer.");
    if (includeManifestManualRefreshRequired) {
        tooltipLines.push('Run "IVGFiddle: Rescan Include Assets" to rebuild the manifest.');
    }
    else if (includeConfig.manifestEnabled) {
        tooltipLines.push('Use "IVGFiddle: Rescan Include Assets" to force a rebuild.');
    }
    includeStatusBarItem.text = `$(${icon}) Includes: ${summary.label}`;
    includeStatusBarItem.tooltip = tooltipLines.join("\n");
    includeStatusBarItem.command = includeManifestManualRefreshRequired ? "ivgfiddle.includes.rescan" : "ivgfiddle.includes.focusExplorer";
    includeStatusBarItem.show();
}
function maybeAutoRevealIncludeExplorer(revisionId) {
    if (!includeConfig.autoRevealExplorer || !includeExplorerView || !revisionId) {
        return;
    }
    if (includeExplorerLastAutoRevealRevision === revisionId) {
        return;
    }
    includeExplorerLastAutoRevealRevision = revisionId;
    void vscode.commands.executeCommand("ivgfiddleIncludesExplorer.focus");
}
function formatError(error) {
    if (error instanceof Error && error.message) {
        return error.message;
    }
    return String(error);
}
function logIncludeTelemetry(message) {
    const timestamp = new Date().toISOString();
    console.log(`[IVG Include] ${message}`);
    appendTraceOutputLine(`[include ${timestamp}] ${message}`);
}
function processStatusMessage(message) {
    const level = typeof message.level === "string" ? message.level : "info";
    const text = typeof message.message === "string" ? message.message : "";
    if (typeof message.durationMs === "number" && message.durationMs >= 0) {
        lastPreviewDurationMs = message.durationMs;
        refreshStatusBar();
    }
    if (!text) {
        return;
    }
    if (level === "error") {
        showTraceErrorMessage(text);
        return;
    }
    clearTransientStatusMessage();
    transientStatusMessage = vscode.window.setStatusBarMessage(text, 5000);
}
function refreshStatusBar() {
    refreshIncludeStatusBarItem();
    if (!currentStatusDocumentUri) {
        return;
    }
    const document = vscode.workspace.textDocuments.find((openDocument) => openDocument.uri.toString() === currentStatusDocumentUri);
    if (document) {
        showStatusBar(document, "focus");
    }
}
function clearTransientStatusMessage() {
    if (transientStatusMessage) {
        transientStatusMessage.dispose();
        transientStatusMessage = undefined;
    }
}
function handleIncludeMissingTraceLine(line) {
    if (!includeConfig.watchersEnabled || !includeConfig.manifestEnabled) {
        return;
    }
    if (typeof line !== "string" || !line.startsWith("[IVGFiddle] Include missing:")) {
        return;
    }
    const match = line.match(/^\[IVGFiddle\] Include missing:\s*(.+?)(?:\s+\(|$)/);
    const includePath = match && match[1] ? match[1].trim() : "";
    if (!includePath || includeMissingRescanPaths.has(includePath)) {
        return;
    }
    includeMissingRescanPaths.add(includePath);
    logIncludeTelemetry(`Include missing trace detected for ${includePath}; scheduling manifest rebuild.`);
    scheduleIncludeManifestBuild("traceMissing");
}
function processTraceMessage(raw) {
    if (!raw || (typeof raw !== "object" && typeof raw !== "function")) {
        return;
    }
    const payload = raw;
    const action = typeof payload.action === "string" ? payload.action : "";
    if (!action) {
        return;
    }
    if (action === "clear") {
        clearTraceOutput();
        return;
    }
    if (action === "reset") {
        const lines = Array.isArray(payload.lines) ? payload.lines.filter((entry) => typeof entry === "string") : [];
        resetTraceOutput(lines);
        return;
    }
    if (action === "append") {
        if (typeof payload.text === "string") {
            handleIncludeMissingTraceLine(payload.text);
            appendTraceOutputLine(payload.text);
        }
        return;
    }
    if (action === "replace" && typeof payload.text === "string") {
        handleIncludeMissingTraceLine(payload.text);
        replaceLastTraceOutputLine(payload.text);
    }
}
function getTraceOutputChannel() {
    if (!traceOutputChannel) {
        traceOutputChannel = vscode.window.createOutputChannel(TRACE_OUTPUT_CHANNEL_NAME);
    }
    return traceOutputChannel;
}
function clearTraceOutput() {
    traceOutputLines = [];
    if (traceOutputChannel) {
        traceOutputChannel.clear();
    }
}
function resetTraceOutput(lines) {
    const channel = getTraceOutputChannel();
    traceOutputLines = lines.slice();
    channel.clear();
    for (const line of traceOutputLines) {
        channel.appendLine(line);
    }
}
function appendTraceOutputLine(line) {
    traceOutputLines.push(line);
    getTraceOutputChannel().appendLine(line);
}
function replaceLastTraceOutputLine(line) {
    if (traceOutputLines.length === 0) {
        appendTraceOutputLine(line);
        return;
    }
    traceOutputLines[traceOutputLines.length - 1] = line;
    const channel = getTraceOutputChannel();
    channel.clear();
    for (const entry of traceOutputLines) {
        channel.appendLine(entry);
    }
}
function revealTraceOutput(preserveFocus) {
    const channel = getTraceOutputChannel();
    channel.show(preserveFocus === true);
}
function showTraceErrorMessage(message) {
    const action = TRACE_SHOW_ACTION;
    vscode.window.showErrorMessage(message, action).then((selection) => {
        if (selection === action) {
            revealTraceOutput();
        }
    });
}
