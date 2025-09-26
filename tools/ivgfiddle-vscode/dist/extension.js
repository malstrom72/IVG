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
function activate(context) {
    console.log('IVGFiddle extension activated');
    const disposable = vscode.commands.registerCommand('ivgfiddle.open', async () => {
        if (ivgPanel) {
            ivgPanel.reveal(ivgPanel.viewColumn ?? vscode.ViewColumn.Active);
            return;
        }
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
        panel.onDidDispose(() => {
            ivgPanel = undefined;
        });
    });
    context.subscriptions.push(disposable);
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
