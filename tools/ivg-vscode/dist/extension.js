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
let includeWatcherEventCounts = {
    create: 0,
    change: 0,
    delete: 0,
};
function activate(context) {
    console.log("IVG Preview extension activated");
    traceOutputChannel = vscode.window.createOutputChannel(TRACE_OUTPUT_CHANNEL_NAME);
    context.subscriptions.push(traceOutputChannel);
    clearTraceOutput();
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    statusBarItem.command = "ivgfiddle.open";
    statusBarItem.hide();
    context.subscriptions.push(statusBarItem);
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
    context.subscriptions.push(vscode.commands.registerCommand("ivgfiddle.refreshPreview", handleRefreshPreviewCommand), vscode.commands.registerCommand("ivgfiddle.clearTrace", handleClearTraceCommand));
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
        if (event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`)) {
            includeConfig = readIncludeConfig();
            initializeIncludeWatchers(context);
            refreshStatusBar();
        }
    }));
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
    return document.languageId === PREVIEW_LANGUAGE_ID || document.uri.fsPath.toLowerCase().endsWith(".ivg");
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
    const includeSuffix = includeConfig.watchersEnabled
        ? includeWatcherFolderCount > 0
            ? "includes watching"
            : "includes pending"
        : "includes off";
    suffix = suffix ? `${suffix} • ${includeSuffix}` : ` • ${includeSuffix}`;
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
    if (!includeConfig.watchersEnabled) {
        tooltipLines.push("Include watchers disabled");
    }
    else if (includeWatcherFolderCount === 0) {
        tooltipLines.push(`Include watchers pending workspace folders (pattern: ${INCLUDE_ASSET_GLOB})`);
    }
    else {
        tooltipLines.push(`Include watchers active on ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"} (pattern: ${INCLUDE_ASSET_GLOB})`);
        tooltipLines.push(`Include watcher events this session — create: ${includeWatcherEventCounts.create}, change: ${includeWatcherEventCounts.change}, delete: ${includeWatcherEventCounts.delete}`);
    }
    statusBarItem.tooltip = tooltipLines.join("\n");
    statusBarItem.show();
    currentStatusDocumentUri = document.uri.toString();
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
    return {
        watchersEnabled,
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
    if (!includeConfig.watchersEnabled) {
        logIncludeTelemetry("Include watchers disabled by configuration.");
        return;
    }
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders || workspaceFolders.length === 0) {
        logIncludeTelemetry("Include watchers pending workspace folders.");
        return;
    }
    for (const folder of workspaceFolders) {
        const pattern = new vscode.RelativePattern(folder, INCLUDE_ASSET_GLOB);
        const watcher = vscode.workspace.createFileSystemWatcher(pattern);
        includeWatchers.push(watcher);
        includeWatcherDisposables.push(watcher.onDidCreate((uri) => handleIncludeWatcherEvent("create", uri)), watcher.onDidChange((uri) => handleIncludeWatcherEvent("change", uri)), watcher.onDidDelete((uri) => handleIncludeWatcherEvent("delete", uri)));
        includeWatcherFolderCount += 1;
    }
    logIncludeTelemetry(`Include watchers attached across ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"} using pattern ${INCLUDE_ASSET_GLOB}.`);
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
    includeWatcherEventCounts = {
        create: 0,
        change: 0,
        delete: 0,
    };
}
function handleIncludeWatcherEvent(kind, uri) {
    includeWatcherEventCounts[kind] += 1;
    const relativePath = vscode.workspace.asRelativePath(uri, false);
    logIncludeTelemetry(`Include asset ${kind} detected at ${relativePath} (events: create=${includeWatcherEventCounts.create}, change=${includeWatcherEventCounts.change}, delete=${includeWatcherEventCounts.delete}).`);
    scheduleIncludeRefresh();
}
function scheduleIncludeRefresh() {
    const targetDocument = getActiveIvgDocument() ?? getLastPreviewDocument();
    if (!targetDocument) {
        return;
    }
    scheduleDocument(targetDocument);
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
            appendTraceOutputLine(payload.text);
        }
        return;
    }
    if (action === "replace" && typeof payload.text === "string") {
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
