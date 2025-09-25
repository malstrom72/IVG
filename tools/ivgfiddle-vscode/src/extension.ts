import * as vscode from 'vscode';

export function activate(context: vscode.ExtensionContext): void {
	console.log('IVGFiddle extension activated');

	const disposable = vscode.commands.registerCommand('ivgfiddle.open', () => {
		vscode.window.showInformationMessage('IVGFiddle command triggered.');
	});

	context.subscriptions.push(disposable);
}

export function deactivate(): void {
	// Intentionally empty; no teardown required for the bootstrap milestone.
}
