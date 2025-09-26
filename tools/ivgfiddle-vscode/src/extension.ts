import * as vscode from 'vscode';

let ivgPanel: vscode.WebviewPanel | undefined;
let webviewReady = false;
let statusBarItem: vscode.StatusBarItem | undefined;
let scheduledDocument: vscode.TextDocument | undefined;
let updateTimer: ReturnType<typeof setTimeout> | undefined;
const pendingMessages: unknown[] = [];
const PREVIEW_LANGUAGE_ID = 'ivg';
const PREVIEW_DEBOUNCE_MS = 150;

export function activate(context: vscode.ExtensionContext): void {
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

const panel = vscode.window.createWebviewPanel(
'ivgfiddle',
'IVGFiddle',
{ viewColumn: vscode.ViewColumn.Active, preserveFocus: false },
{
enableScripts: true,
localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, 'media')],
}
);

try {
panel.webview.html = await getWebviewContent(context.extensionUri, panel.webview);
} catch (error) {
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
syncActiveDocument('focus');
}
});
panel.onDidChangeViewState(() => {
if (panel.visible) {
syncActiveDocument('focus');
}
});
panel.onDidDispose(() => {
ivgPanel = undefined;
webviewReady = false;
pendingMessages.length = 0;
if (statusBarItem) {
statusBarItem.hide();
}
});

syncActiveDocument('open');
});

context.subscriptions.push(disposable);
context.subscriptions.push(
vscode.workspace.onDidOpenTextDocument((document) => {
if (isIvgDocument(document)) {
syncDocument(document, 'open');
}
}),
vscode.workspace.onDidChangeTextDocument((event) => {
if (isIvgDocument(event.document)) {
scheduleDocument(event.document);
}
}),
vscode.window.onDidChangeActiveTextEditor((editor) => {
syncActiveDocument(editor ? 'focus' : 'clear');
}),
vscode.workspace.onDidCloseTextDocument((document) => {
if (statusBarItem && (!vscode.window.visibleTextEditors.some((editor) => editor.document === document))) {
statusBarItem.hide();
}
if (scheduledDocument && scheduledDocument === document) {
scheduledDocument = undefined;
}
})
);
}

export function deactivate(): void {
// Intentionally empty; no teardown required for the bootstrap milestone.
}

async function getWebviewContent(extensionUri: vscode.Uri, webview: vscode.Webview): Promise<string> {
	const mediaRoot = vscode.Uri.joinPath(extensionUri, 'media');
	const htmlUri = vscode.Uri.joinPath(mediaRoot, 'ivgfiddle.html');
	const nonce = generateNonce();

	let html: string;
	try {
		const raw = await vscode.workspace.fs.readFile(htmlUri);
		html = new TextDecoder('utf-8').decode(raw);
	} catch (readError) {
		throw new Error('ivgfiddle.html is missing from the media directory.');
	}

	const csp = [
		"default-src 'none'",
		`img-src ${webview.cspSource} data:`,
		`script-src 'nonce-${nonce}' 'wasm-unsafe-eval'`,
		`style-src ${webview.cspSource}`,
		`font-src ${webview.cspSource}`,
	].join('; ');

	html = html.replace(
		'<head>',
		`<head>
		<meta http-equiv="Content-Security-Policy" content="${csp}">`
	);

	const attributeRegex = /(src|href)="\.\/([^"]+)"/g;
	const resourceMap = new Map<string, string>();
	for (const match of html.matchAll(attributeRegex)) {
		const relPath = match[2];
		if (!relPath || resourceMap.has(relPath)) {
			continue;
		}
		const segments = relPath.split('/');
		const assetUri = vscode.Uri.joinPath(mediaRoot, ...segments);
		try {
			await vscode.workspace.fs.stat(assetUri);
		} catch (statError) {
			throw new Error(`Missing asset referenced in ivgfiddle.html: ${relPath}`);
		}
		resourceMap.set(relPath, webview.asWebviewUri(assetUri).toString());
	}

	html = html.replace(
		attributeRegex,
		(_match: string, attr: string, relPath: string) => {
			const mapped = resourceMap.get(relPath);
			return mapped ? `${attr}="${mapped}"` : `${attr}="${relPath}"`;
		}
	);

	html = html.replace(
		/<script([^>]*)>/g,
		(_match: string, attrs: string) => `<script${attrs} nonce="${nonce}">`
	);

return html;
}

function generateNonce(): string {
const possible = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
let text = '';
for (let i = 0; i < 32; i += 1) {
text += possible.charAt(Math.floor(Math.random() * possible.length));
}
return text;
}

function isIvgDocument(document: vscode.TextDocument): boolean {
return document.languageId === PREVIEW_LANGUAGE_ID || document.uri.fsPath.toLowerCase().endsWith('.ivg');
}

function scheduleDocument(document: vscode.TextDocument): void {
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

function syncActiveDocument(reason: 'open' | 'focus' | 'clear'): void {
const editor = vscode.window.activeTextEditor;
if (editor && isIvgDocument(editor.document)) {
const resolved: 'open' | 'focus' = reason === 'focus' ? 'focus' : 'open';
syncDocument(editor.document, resolved);
return;
}
if (statusBarItem) {
statusBarItem.hide();
}
}

function syncDocument(document: vscode.TextDocument, reason: 'open' | 'focus' | 'change'): void {
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
if (statusBarItem) {
statusBarItem.text = `$(sync) IVGFiddle Preview: ${fileName}`;
statusBarItem.tooltip = document.uri.fsPath;
statusBarItem.show();
}
}

function queueMessage(message: unknown): void {
if (!ivgPanel) {
return;
}
if (!webviewReady) {
pendingMessages.push(message);
return;
}
ivgPanel.webview.postMessage(message);
}

function flushPendingMessages(): void {
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

function fileNameFromDocument(document: vscode.TextDocument): string {
const fsPath = document.fileName;
const forwardSlash = fsPath.lastIndexOf('/');
const backwardSlash = fsPath.lastIndexOf('\\');
const index = Math.max(forwardSlash, backwardSlash);
if (index >= 0) {
return fsPath.substring(index + 1);
}
return fsPath;
}
