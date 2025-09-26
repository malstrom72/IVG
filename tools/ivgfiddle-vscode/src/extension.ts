import * as vscode from 'vscode';

let ivgPanel: vscode.WebviewPanel | undefined;

export function activate(context: vscode.ExtensionContext): void {
	console.log('IVGFiddle extension activated');

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
		panel.onDidDispose(() => {
			ivgPanel = undefined;
		});
	});

	context.subscriptions.push(disposable);
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
		`script-src 'nonce-${nonce}'`,
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
