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
const pendingMessages = [];
const PREVIEW_LANGUAGE_ID = 'ivg';
const PREVIEW_DEBOUNCE_MS = 150;
function activate(context) {
    console.log('IVGFiddle extension activated');
    statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    statusBarItem.command = 'ivgfiddle.open';
    statusBarItem.hide();
    context.subscriptions.push(statusBarItem);
    const disposable = vscode.commands.registerCommand('ivgfiddle.open', async () => {
        if (ivgPanel) {
            ivgPanel.reveal(ivgPanel.viewColumn ?? vscode.ViewColumn.Active);
            return;
        }
        const initialDocument = getActiveIvgDocument();
        const panel = vscode.window.createWebviewPanel('ivgfiddle', 'IVGFiddle', { viewColumn: vscode.ViewColumn.Active, preserveFocus: false }, {
            enableScripts: true,
            localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, 'media')],
        });
        try {
            panel.webview.html = await getWebviewContent(context.extensionUri, panel.webview);
        }
        catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            vscode.window.showErrorMessage(`Failed to load IVGFiddle: ${message}`);
            console.error('Failed to load IVGFiddle Webview', error);
            panel.dispose();
            return;
        }
        ivgPanel = panel;
        webviewReady = false;
        pendingMessages.length = 0;
        panel.webview.onDidReceiveMessage((message) => {
            if (message && message.type === 'ready') {
                webviewReady = true;
                flushPendingMessages();
                syncActiveDocument('panelFocus');
            }
        });
        panel.onDidChangeViewState(() => {
            if (panel.visible) {
                syncActiveDocument('panelFocus');
            }
        });
        panel.onDidDispose(() => {
            ivgPanel = undefined;
            webviewReady = false;
            pendingMessages.length = 0;
            hideStatusBar();
        });
        if (initialDocument) {
            syncDocument(initialDocument, 'open');
        }
        else {
            syncActiveDocument('open');
        }
    });
    context.subscriptions.push(disposable);
    context.subscriptions.push(vscode.workspace.onDidOpenTextDocument((document) => {
        if (isIvgDocument(document)) {
            syncDocument(document, 'open');
        }
    }), vscode.workspace.onDidChangeTextDocument((event) => {
        if (isIvgDocument(event.document)) {
            scheduleDocument(event.document);
        }
    }), vscode.window.onDidChangeActiveTextEditor((editor) => {
        if (editor) {
            syncActiveDocument('focus');
        }
        else if (ivgPanel && ivgPanel.active) {
            syncActiveDocument('panelFocus');
        }
        else {
            syncActiveDocument('clear');
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
                syncDocument(active, 'focus');
                return;
            }
            const fallback = getLastPreviewDocument();
            if (fallback) {
                showStatusBar(fallback, 'focus');
                return;
            }
            hideStatusBar();
        }
    }));
}
function deactivate() {
    // Intentionally empty; no teardown required for the bootstrap milestone.
}
async function getWebviewContent(extensionUri, webview) {
    const mediaRoot = vscode.Uri.joinPath(extensionUri, 'media');
    const htmlUri = vscode.Uri.joinPath(mediaRoot, 'ivgfiddle.html');
    const nonce = generateNonce();
    let html;
    try {
        const raw = await vscode.workspace.fs.readFile(htmlUri);
        html = new TextDecoder('utf-8').decode(raw);
    }
    catch (readError) {
        throw new Error('ivgfiddle.html is missing from the media directory.');
    }
    const csp = [
        "default-src 'none'",
        `img-src ${webview.cspSource} data:`,
        `script-src 'nonce-${nonce}' 'wasm-unsafe-eval'`,
        `style-src ${webview.cspSource}`,
        `font-src ${webview.cspSource}`,
    ].join('; ');
    html = html.replace('<head>', `<head>
		<meta http-equiv="Content-Security-Policy" content="${csp}">`);
    const attributeRegex = /(src|href)="\.\/([^"]+)"/g;
    const resourceMap = new Map();
    for (const match of html.matchAll(attributeRegex)) {
        const relPath = match[2];
        if (!relPath || resourceMap.has(relPath)) {
            continue;
        }
        const segments = relPath.split('/');
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
    const possible = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
    let text = '';
    for (let i = 0; i < 32; i += 1) {
        text += possible.charAt(Math.floor(Math.random() * possible.length));
    }
    return text;
}
function isIvgDocument(document) {
    return document.languageId === PREVIEW_LANGUAGE_ID || document.uri.fsPath.toLowerCase().endsWith('.ivg');
}
function scheduleDocument(document) {
    scheduledDocument = document;
    if (updateTimer) {
        clearTimeout(updateTimer);
    }
    updateTimer = setTimeout(() => {
        updateTimer = undefined;
        if (scheduledDocument) {
            syncDocument(scheduledDocument, 'change');
        }
    }, PREVIEW_DEBOUNCE_MS);
}
function syncActiveDocument(reason) {
    if (reason === 'clear') {
        const fallback = getLastPreviewDocument();
        if (fallback) {
            showStatusBar(fallback, 'focus');
            return;
        }
        hideStatusBar();
        return;
    }
    const activeDocument = getActiveIvgDocument();
    if (activeDocument) {
        const resolved = reason === 'focus' ? 'focus' : 'open';
        syncDocument(activeDocument, resolved);
        return;
    }
    const fallback = getLastPreviewDocument();
    if (fallback) {
        if (reason === 'panelFocus') {
            syncDocument(fallback, 'focus');
            return;
        }
        showStatusBar(fallback, 'focus');
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
    const status = reason === 'change' ? `Updating preview for ${fileName}…` : `Rendering ${fileName}`;
    queueMessage({
        type: 'setSource',
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
    ivgPanel.webview.postMessage(message);
}
function flushPendingMessages() {
    if (!ivgPanel) {
        pendingMessages.length = 0;
        return;
    }
    while (pendingMessages.length > 0) {
        const message = pendingMessages.shift();
        if (message) {
            ivgPanel.webview.postMessage(message);
        }
    }
}
function fileNameFromDocument(document) {
    const fsPath = document.fileName;
    const forwardSlash = fsPath.lastIndexOf('/');
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
    const icon = reason === 'change' ? 'sync~spin' : 'sync';
    statusBarItem.text = `$(${icon}) IVGFiddle Preview: ${fileName}`;
    statusBarItem.tooltip = document.uri.fsPath;
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
