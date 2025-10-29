import * as vscode from "vscode";
import { createHash } from "crypto";
import * as fs from "fs";
import * as path from "path";

let ivgPanel: vscode.WebviewPanel | undefined;
let webviewReady = false;
let statusBarItem: vscode.StatusBarItem | undefined;
let scheduledDocument: vscode.TextDocument | undefined;
let lastPreviewDocumentUri: string | undefined;
let currentStatusDocumentUri: string | undefined;
let updateTimer: ReturnType<typeof setTimeout> | undefined;
let lastPreviewDurationMs: number | undefined;
let transientStatusMessage: vscode.Disposable | undefined;
const pendingMessages: unknown[] = [];
const PREVIEW_LANGUAGE_ID = "ivg";
const DEFAULT_DEBOUNCE_MS = 150;
const CONFIG_SECTION = "ivgfiddle.preview";
const GENERAL_CONFIG_SECTION = "ivgfiddle";
const INCLUDE_CONFIG_SECTION = "ivgfiddle.includes";
const DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS = 150;
const WORKSPACE_STATE_INCLUDE_ROOTS_KEY = "ivgfiddle.includes.lastRoots";
// Track include-eligible assets that must stay in sync with IVGFiddle. IMPD composition (`.impd`),
// inline vector graphics (`.ivg`), IVG fonts (`.ivgfont`), and raster fallbacks (`.png`) are
// pre-packaged alongside previews, so the watcher focuses on that set of extensions.
const INCLUDE_ASSET_GLOB = "**/*.{impd,ivg,ivgfont,png}";
const TRACE_OUTPUT_CHANNEL_NAME = "IVG Preview Trace";
const TRACE_SHOW_ACTION = "Show Trace Output";

let traceOutputChannel: vscode.OutputChannel | undefined;
let traceOutputLines: string[] = [];

interface PreviewConfig {
	autoRefresh: boolean;
	debounceMs: number;
}

interface GeneralConfig {
	syncOnOpen: boolean;
	webviewUpdateDelay: number;
}

interface IncludeConfig {
	watchersEnabled: boolean;
	manifestEnabled: boolean;
	autoRescan: boolean;
	rescanDebounceMs: number;
	autoRevealExplorer: boolean;
	roots: IncludeRootConfigEntry[];
	effectiveRoots: IncludeRootConfigEntry[];
	rootErrors: IncludeRootConfigError[];
	rawRootsByWorkspace: Map<string, string[]>;
}

interface IncludeRootConfigEntry {
	id: string;
	origin: "configured" | "workspace";
	workspaceFolder?: vscode.WorkspaceFolder;
	sourcePath: string;
	resolvedFsPath: string;
	uri: vscode.Uri;
	displayName: string;
}

interface IncludeRootConfigError {
	workspaceFolder?: vscode.WorkspaceFolder;
	sourcePath: string;
	message: string;
}

interface IncludeRootQuickPickItem extends vscode.QuickPickItem {
	selectionType: "candidate" | "recent" | "browse";
	workspaceFolder?: vscode.WorkspaceFolder;
	configurationPath?: string;
	targetUri?: vscode.Uri;
}

let previewConfig: PreviewConfig = readPreviewConfig();
let generalConfig: GeneralConfig = readGeneralConfig();
let includeConfig: IncludeConfig = readIncludeConfig();

let includeWatchers: vscode.FileSystemWatcher[] = [];
let includeWatcherDisposables: vscode.Disposable[] = [];
let includeWatcherRootCount = 0;
let includeWatcherCandidateRootCount = 0;
let includeWatcherUnavailableMessage: string | undefined;
let includeWatcherEventCounts: Record<"create" | "change" | "delete", number> = {
	create: 0,
	change: 0,
	delete: 0,
};
type IncludeWatcherEvent = "create" | "change" | "delete";
let includeWatcherRootLabels: string[] = [];

const INCLUDE_CACHE_FOLDER = "include-cache";
const INCLUDE_MANIFEST_FILE_NAME = "include-manifest.json";
const INCLUDE_MANIFEST_VERSION = 1;

type IncludeManifestStatus = "idle" | "pending" | "building" | "ready" | "error";

interface IncludeManifestEntry {
	mountPath: string;
	byteLength: number;
	checksum: string;
	mimeType: string;
}

interface IncludeManifest {
	version: number;
	revision: string;
	generatedAt: string;
	entries: IncludeManifestEntry[];
}

interface IncludeManifestBuildItem {
	relativePath: string;
	content: Uint8Array;
	entry: IncludeManifestEntry;
}

interface IncludeBundleAssetPayload {
	path: string;
	checksum: string;
	mimeType: string;
	data: string;
}

interface IncludeBundleMessage {
	type: "setIncludeBundle";
	revision: string;
	manifest: IncludeManifest;
	assets: IncludeBundleAssetPayload[];
}

interface IncludeExplorerAssetEntry {
	kind: "asset";
	label: string;
	relativePath: string;
	mountPath: string;
	byteLength: number;
	checksum: string;
	mimeType: string;
	revision?: string;
}

interface IncludeExplorerMessageEntry {
	kind: "message";
	label: string;
	description?: string;
	detail?: string;
	icon?: string;
	severity?: "info" | "warning" | "error";
	command?: vscode.Command;
}

type IncludeExplorerEntry = IncludeExplorerAssetEntry | IncludeExplorerMessageEntry;

type IncludeManifestScheduleReason =
	| "activation"
	| "watcherInitialization"
	| "watcherEvent"
	| "configuration"
	| "chained"
	| "manual"
	| "traceMissing";

interface IncludeRelativePathResult {
	relativePath: string;
	matchedRoot?: IncludeRootConfigEntry;
}

let extensionContext: vscode.ExtensionContext | undefined;
let includeManifestTimer: ReturnType<typeof setTimeout> | undefined;
let includeManifestStatus: IncludeManifestStatus = "idle";
let includeManifestRevisionId: string | undefined;
let includeManifestEntryCount = 0;
let includeManifestTotalBytes = 0;
let includeManifestLastGeneratedAt: string | undefined;
let includeManifestLastError: string | undefined;
let includeManifestBuildInProgress = false;
let includeManifestRebuildQueued = false;
let includeManifestSnapshot: IncludeManifest | undefined;
let latestIncludeBundleMessage: IncludeBundleMessage | undefined;
let includeManifestManualRefreshRequired = false;

let includeExplorerProvider: IncludeExplorerProvider | undefined;
let includeExplorerView: vscode.TreeView<IncludeExplorerEntry> | undefined;
let includeExplorerLastAutoRevealRevision: string | undefined;

let includeStatusBarItem: vscode.StatusBarItem | undefined;
const includeMissingRescanPaths = new Set<string>();
let includeLastUsedRoots: string[] = [];

const MIME_TYPE_BY_EXTENSION = new Map<string, string>([
	["impd", "application/json"],
	["ivg", "application/xml"],
	["ivgfont", "application/octet-stream"],
	["png", "image/png"],
]);

export function activate(context: vscode.ExtensionContext): void {
	console.log("IVG Preview extension activated");
	extensionContext = context;
	includeLastUsedRoots = context.workspaceState.get<string[]>(WORKSPACE_STATE_INCLUDE_ROOTS_KEY, []);

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

		const panel = vscode.window.createWebviewPanel(
			"ivgfiddle",
			"IVG Preview",
			{ viewColumn: vscode.ViewColumn.Active, preserveFocus: false },
			{
				enableScripts: true,
				localResourceRoots: [vscode.Uri.joinPath(context.extensionUri, "media")],
			},
		);

		try {
			panel.webview.html = await getWebviewContent(context.extensionUri, panel.webview);
		} catch (error) {
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
			const type = (message as { type?: unknown }).type;
			if (type === "ready") {
				webviewReady = true;
				flushPendingMessages();
				syncActiveDocument("panelFocus");
				return;
			}
			if (type === "trace") {
				const payload = (message as { message?: unknown }).message;
				processTraceMessage(payload);
				return;
			}
			if (type === "status") {
				processStatusMessage(
					message as {
						level?: unknown;
						message?: unknown;
						durationMs?: unknown;
					},
				);
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
		} else {
			syncActiveDocument("open");
		}
	});

	context.subscriptions.push(openCommand);
	context.subscriptions.push(
		vscode.commands.registerCommand("ivgfiddle.refreshPreview", handleRefreshPreviewCommand),
		vscode.commands.registerCommand("ivgfiddle.clearTrace", handleClearTraceCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.focusExplorer", handleFocusIncludeExplorerCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.rescan", handleRescanIncludesCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.openAsset", handleOpenIncludeAssetCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.revealAsset", handleRevealIncludeAssetCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.copyMountPath", handleCopyIncludeMountPathCommand),
		vscode.commands.registerCommand("ivgfiddle.includes.setRoot", () => handleChooseIncludeRootCommand("replace")),
		vscode.commands.registerCommand("ivgfiddle.includes.addRoot", () => handleChooseIncludeRootCommand("append")),
		vscode.commands.registerCommand("ivgfiddle.includes.clearRoots", handleClearIncludeRootsCommand),
	);
	context.subscriptions.push(
		vscode.workspace.onDidOpenTextDocument((document) => {
			if (isIvgDocument(document)) {
				syncDocument(document, "open");
			}
		}),
		vscode.workspace.onDidChangeTextDocument((event) => {
			if (isIvgDocument(event.document)) {
				scheduleDocument(event.document);
			}
		}),
		vscode.window.onDidChangeActiveTextEditor((editor) => {
			if (editor) {
				syncActiveDocument("focus");
			} else if (ivgPanel && ivgPanel.active) {
				syncActiveDocument("panelFocus");
			} else {
				syncActiveDocument("clear");
			}
		}),
		vscode.workspace.onDidCloseTextDocument((document) => {
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
		}),
		vscode.workspace.onDidChangeWorkspaceFolders(() => {
			initializeIncludeWatchers(context);
		}),
		vscode.workspace.onDidChangeConfiguration((event) => {
			if (event.affectsConfiguration(`${CONFIG_SECTION}.autoRefresh`) || event.affectsConfiguration(`${CONFIG_SECTION}.debounceMs`)) {
				previewConfig = readPreviewConfig();
				refreshStatusBar();
				if (scheduledDocument && previewConfig.autoRefresh) {
					syncDocument(scheduledDocument, "change");
				}
			}
			if (
				event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.syncOnOpen`) ||
				event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.webviewUpdateDelay`)
			) {
				generalConfig = readGeneralConfig();
				refreshStatusBar();
				if (scheduledDocument && previewConfig.autoRefresh) {
					syncDocument(scheduledDocument, "change");
				}
			}
			if (
				event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.syncOnOpen`) ||
				event.affectsConfiguration(`${GENERAL_CONFIG_SECTION}.webviewUpdateDelay`)
			) {
				generalConfig = readGeneralConfig();
				refreshStatusBar();
				if (scheduledDocument && previewConfig.autoRefresh) {
					syncDocument(scheduledDocument, "change");
				}
			}
			if (
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.manifestEnabled`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRescan`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.rescanDebounceMs`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRevealExplorer`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.roots`)
			) {
				includeConfig = readIncludeConfig();
				const watcherTopologyChanged =
					event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`) ||
					event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.manifestEnabled`) ||
					event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.roots`);
				if (watcherTopologyChanged) {
					initializeIncludeWatchers(context);
				} else {
					refreshIncludeSurfaces();
				}
				if (includeConfig.autoRescan) {
					includeManifestManualRefreshRequired = false;
				}
				if (
					includeConfig.watchersEnabled &&
					includeConfig.manifestEnabled &&
					includeConfig.autoRescan &&
					(event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.autoRescan`) ||
						event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.rescanDebounceMs`) ||
						event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.roots`))
				) {
					scheduleIncludeManifestBuild("configuration");
				}
				refreshStatusBar();
				refreshIncludeSurfaces();
			}
		}),
	);
	refreshIncludeSurfaces();
}

export function deactivate(): void {
	// Intentionally empty; no teardown required for the bootstrap milestone.
}

async function getWebviewContent(extensionUri: vscode.Uri, webview: vscode.Webview): Promise<string> {
	const mediaRoot = vscode.Uri.joinPath(extensionUri, "media");
	const htmlUri = vscode.Uri.joinPath(mediaRoot, "ivgfiddle.html");
	const nonce = generateNonce();

	let html: string;
	try {
		const raw = await vscode.workspace.fs.readFile(htmlUri);
		html = new TextDecoder("utf-8").decode(raw);
	} catch (readError) {
		throw new Error("ivgfiddle.html is missing from the media directory.");
	}

	const csp = [
		"default-src 'none'",
		`img-src ${webview.cspSource} data:`,
		`script-src 'nonce-${nonce}' 'wasm-unsafe-eval'`,
		`style-src ${webview.cspSource}`,
		`font-src ${webview.cspSource}`,
	].join("; ");

	html = html.replace(
		"<head>",
		`<head>
		<meta http-equiv="Content-Security-Policy" content="${csp}">`,
	);

	const attributeRegex = /(src|href)="\.\/([^"]+)"/g;
	const resourceMap = new Map<string, string>();
	for (const match of html.matchAll(attributeRegex)) {
		const relPath = match[2];
		if (!relPath || resourceMap.has(relPath)) {
			continue;
		}
		const segments = relPath.split("/");
		const assetUri = vscode.Uri.joinPath(mediaRoot, ...segments);
		try {
			await vscode.workspace.fs.stat(assetUri);
		} catch (statError) {
			throw new Error(`Missing asset referenced in ivgfiddle.html: ${relPath}`);
		}
		resourceMap.set(relPath, webview.asWebviewUri(assetUri).toString());
	}

	html = html.replace(attributeRegex, (_match: string, attr: string, relPath: string) => {
		const mapped = resourceMap.get(relPath);
		return mapped ? `${attr}="${mapped}"` : `${attr}="${relPath}"`;
	});

	html = html.replace(/<script([^>]*)>/g, (_match: string, attrs: string) => `<script${attrs} nonce="${nonce}">`);

	return html;
}

function generateNonce(): string {
	const possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	let text = "";
	for (let i = 0; i < 32; i += 1) {
		text += possible.charAt(Math.floor(Math.random() * possible.length));
	}
	return text;
}

function isIvgDocument(document: vscode.TextDocument): boolean {
	const lowerPath = document.uri.fsPath.toLowerCase();
	if (lowerPath.endsWith(".ivg")) {
		return true;
	}
	if (document.uri.scheme === "untitled" && document.languageId === PREVIEW_LANGUAGE_ID) {
		return true;
	}
	return false;
}

function scheduleDocument(document: vscode.TextDocument): void {
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
	updateTimer = setTimeout(
		() => {
			updateTimer = undefined;
			if (scheduledDocument) {
				syncDocument(scheduledDocument, "change");
			}
		},
		Math.max(0, previewConfig.debounceMs),
	);
}

function syncActiveDocument(reason: "open" | "focus" | "panelFocus" | "clear"): void {
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
		const resolved: "open" | "focus" = reason === "focus" ? "focus" : "open";
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

function syncDocument(document: vscode.TextDocument, reason: "open" | "focus" | "change"): void {
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

function queueMessage(message: unknown): void {
	if (!ivgPanel) {
		return;
	}
	if (!webviewReady) {
		pendingMessages.push(message);
		return;
	}
	postMessageToWebview(message);
}

function queueIncludeBundleMessage(message: IncludeBundleMessage): void {
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

function queueEmptyIncludeBundleMessage(reason: string): void {
	const revision = `empty-${reason}-${Date.now().toString(36)}`;
	const manifest: IncludeManifest = {
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
	public readonly entry: IncludeExplorerAssetEntry;

	public constructor(entry: IncludeExplorerAssetEntry) {
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

class IncludeExplorerProvider implements vscode.TreeDataProvider<IncludeExplorerEntry>, vscode.Disposable {
	private readonly changeEmitter = new vscode.EventEmitter<IncludeExplorerEntry | undefined | void>();

	public readonly onDidChangeTreeData = this.changeEmitter.event;

	public dispose(): void {
		this.changeEmitter.dispose();
	}

	public refresh(): void {
		this.changeEmitter.fire();
	}

	public getTreeItem(element: IncludeExplorerEntry): vscode.TreeItem {
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
		const tooltipLines: string[] = [];
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

	public getChildren(element?: IncludeExplorerEntry): vscode.ProviderResult<IncludeExplorerEntry[]> {
		if (element) {
			return [];
		}
		return buildIncludeExplorerEntries();
	}
}

function buildIncludeExplorerEntries(): IncludeExplorerEntry[] {
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
				description:
					includeWatcherUnavailableMessage ?? `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB}).`,
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
				description: `Scanning ${includeWatcherCandidateFolderCount} workspace folder${
					includeWatcherCandidateFolderCount === 1 ? "" : "s"
				} for ${INCLUDE_ASSET_GLOB}.`,
				icon: "sync~spin",
				severity: "info",
			},
		];
	}
	const entries: IncludeExplorerEntry[] = [];
	entries.push({
		kind: "message",
		label: `Watching ${includeWatcherRootCount} include root${includeWatcherRootCount === 1 ? "" : "s"}`,
		description: `Pattern: ${INCLUDE_ASSET_GLOB}`,
		detail: `Events — create: ${includeWatcherEventCounts.create}, change: ${includeWatcherEventCounts.change}, delete: ${includeWatcherEventCounts.delete}`,
		icon: "watch",
		severity: "info",
	});
	if (includeWatcherRootLabels.length > 0) {
		entries.push({
			kind: "message",
			label: "Active roots",
			description: includeWatcherRootLabels.join(", "),
			icon: "folder",
			severity: "info",
		});
	}
	if (includeConfig.roots.length > 0) {
		entries.push({
			kind: "message",
			label: "Configured roots",
			description: includeConfig.roots.map((root) => root.displayName).join(", "),
			icon: "settings-gear",
			severity: "info",
		});
	}
	if (includeConfig.rootErrors.length > 0) {
		entries.push({
			kind: "message",
			label: "Include root issues",
			description: includeConfig.rootErrors.map((error) => formatIncludeRootError(error)).join("; "),
			icon: "warning",
			severity: "warning",
			command: {
				command: "workbench.action.openSettings",
				title: "Open Include Root Settings",
				arguments: ["ivgfiddle.includes.roots"],
			},
		});
	}
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
	} else if (includeManifestManualRefreshRequired) {
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
	} else if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
		entries.push({
			kind: "message",
			label: "Building include manifest…",
			description: "Collecting updated include assets.",
			icon: "sync~spin",
			severity: "info",
		});
	} else if (includeManifestStatus === "ready") {
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
	} else if (includeManifestStatus === "ready") {
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

function getIncludeAssetEntryFromArgument(argument: unknown): IncludeExplorerAssetEntry | undefined {
	if (!argument) {
		return undefined;
	}
	if (argument instanceof IncludeAssetTreeItem) {
		return argument.entry;
	}
	const candidate = argument as Partial<IncludeExplorerAssetEntry>;
	if (candidate && candidate.kind === "asset" && typeof candidate.relativePath === "string") {
		return candidate as IncludeExplorerAssetEntry;
	}
	return undefined;
}

async function resolveIncludeAssetUri(relativePath: string): Promise<vscode.Uri | undefined> {
	const normalized = normalizeIncludeRelativePath(relativePath);
	const segments = normalized.split("/").filter(Boolean);
	for (const root of includeConfig.effectiveRoots) {
		const target = segments.length > 0 ? vscode.Uri.joinPath(root.uri, ...segments) : root.uri;
		try {
			await vscode.workspace.fs.stat(target);
			return target;
		} catch (error) {
			// Continue probing remaining roots.
		}
	}
	const workspaceFolders = vscode.workspace.workspaceFolders;
	if (!workspaceFolders || workspaceFolders.length === 0) {
		return undefined;
	}
	for (const folder of workspaceFolders) {
		const candidate = segments.length > 0 ? vscode.Uri.joinPath(folder.uri, ...segments) : folder.uri;
		try {
			await vscode.workspace.fs.stat(candidate);
			return candidate;
		} catch (error) {
			// Continue probing additional workspace folders.
		}
	}
	return undefined;
}

function formatByteLength(byteLength: number): string {
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

function prunePendingIncludeBundleMessages(): void {
	for (let index = pendingMessages.length - 1; index >= 0; index -= 1) {
		const entry = pendingMessages[index];
		if (isIncludeBundleMessage(entry)) {
			pendingMessages.splice(index, 1);
		}
	}
}

function seedLatestIncludeBundleMessage(): void {
	if (!ivgPanel || !latestIncludeBundleMessage) {
		return;
	}
	prunePendingIncludeBundleMessages();
	pendingMessages.push(latestIncludeBundleMessage);
}

function flushPendingMessages(): void {
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

function isIncludeBundleMessage(value: unknown): value is IncludeBundleMessage {
	if (!value || typeof value !== "object") {
		return false;
	}
	const candidate = value as { type?: unknown };
	return candidate.type === "setIncludeBundle";
}

function postMessageToWebview(message: unknown): void {
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

function fileNameFromDocument(document: vscode.TextDocument): string {
	const fsPath = document.fileName;
	const forwardSlash = fsPath.lastIndexOf("/");
	const backwardSlash = fsPath.lastIndexOf(String.fromCharCode(92));
	const index = Math.max(forwardSlash, backwardSlash);
	if (index >= 0) {
		return fsPath.substring(index + 1);
	}
	return fsPath;
}

type StatusReason = "open" | "focus" | "change" | "manualPending" | "deferred";

interface IncludeStatusSummary {
	label: string;
	tooltipLines: string[];
}

function showStatusBar(document: vscode.TextDocument, reason: StatusReason): void {
	if (!statusBarItem) {
		return;
	}
	const fileName = fileNameFromDocument(document);
	let icon = "sync";
	let suffix = "";
	if (reason === "change") {
		icon = "sync~spin";
	} else if (reason === "manualPending") {
		icon = "clock";
		suffix = " — refresh required";
	} else if (reason === "deferred") {
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

function getIncludeStatusSummary(): IncludeStatusSummary {
	if (!includeConfig.watchersEnabled) {
		return {
			label: "includes off",
			tooltipLines: ["Include watchers disabled"],
		};
	}
	if (includeWatcherRootCount === 0) {
		if (includeWatcherCandidateRootCount === 0) {
			return {
				label: "includes unavailable",
				tooltipLines: [
					includeWatcherUnavailableMessage
						? includeWatcherUnavailableMessage
						: `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB})`,
				],
			};
		}
		const pendingTooltip: string[] = [
			`Include watchers initializing across ${includeWatcherCandidateRootCount} include root${includeWatcherCandidateRootCount === 1 ? "" : "s"} (pattern: ${INCLUDE_ASSET_GLOB})`,
		];
		appendIncludeRootDiagnostics(pendingTooltip);
		return {
			label: "includes pending",
			tooltipLines: pendingTooltip,
		};
	}
	const tooltipLines = [
		`Include watchers active on ${includeWatcherRootCount} include root${includeWatcherRootCount === 1 ? "" : "s"} (pattern: ${INCLUDE_ASSET_GLOB})`,
		`Include watcher events this session — create: ${includeWatcherEventCounts.create}, change: ${includeWatcherEventCounts.change}, delete: ${includeWatcherEventCounts.delete}`,
	];
	appendIncludeRootDiagnostics(tooltipLines);
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
		tooltipLines.push(
			`Include manifest ready — revision ${revisionLabel}, ${includeManifestEntryCount} ${assetLabel}, ${includeManifestTotalBytes} ${byteLabel}`,
		);
		if (includeManifestLastGeneratedAt) {
			tooltipLines.push(`Include manifest generated at ${includeManifestLastGeneratedAt}`);
		}
		if (!includeConfig.autoRescan) {
			tooltipLines.push("Manual rescan mode active.");
		}
		const manualSuffix = includeConfig.autoRescan ? "" : " (manual)";
		return {
			label:
				includeManifestEntryCount > 0
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

function hideStatusBar(): void {
	if (statusBarItem) {
		statusBarItem.hide();
	}
	currentStatusDocumentUri = undefined;
}

function getActiveIvgDocument(): vscode.TextDocument | undefined {
	const editor = vscode.window.activeTextEditor;
	if (editor && isIvgDocument(editor.document)) {
		return editor.document;
	}
	return undefined;
}

function getLastPreviewDocument(): vscode.TextDocument | undefined {
	if (!lastPreviewDocumentUri) {
		return undefined;
	}
	return vscode.workspace.textDocuments.find((openDocument) => openDocument.uri.toString() === lastPreviewDocumentUri);
}

function handleRefreshPreviewCommand(): void {
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

function handleClearTraceCommand(): void {
	if (!ivgPanel) {
		vscode.window.showInformationMessage("Open the IVG Preview panel before clearing the trace.");
		return;
	}
	queueMessage({ type: "clearTrace" });
}

function handleFocusIncludeExplorerCommand(): void {
	void vscode.commands.executeCommand("ivgfiddleIncludesExplorer.focus");
}

async function handleRescanIncludesCommand(): Promise<void> {
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

async function handleOpenIncludeAssetCommand(target?: IncludeAssetTreeItem | IncludeExplorerAssetEntry): Promise<void> {
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

async function handleRevealIncludeAssetCommand(target?: IncludeAssetTreeItem | IncludeExplorerAssetEntry): Promise<void> {
	const entry = getIncludeAssetEntryFromArgument(target);
	if (!entry) {
		await vscode.window.showInformationMessage("Select an include asset to reveal.");
		return;
	}
	const uri = await resolveIncludeAssetUri(entry.relativePath);
	if (!uri) {
		await vscode.window.showWarningMessage(
			`Unable to reveal include asset ${entry.relativePath}; the file is missing from the current workspace.`,
		);
		return;
	}
	await vscode.commands.executeCommand("revealFileInOS", uri);
}

async function handleCopyIncludeMountPathCommand(target?: IncludeAssetTreeItem | IncludeExplorerAssetEntry): Promise<void> {
	const entry = getIncludeAssetEntryFromArgument(target);
	if (!entry) {
		await vscode.window.showInformationMessage("Select an include asset to copy its mount path.");
		return;
	}
	await vscode.env.clipboard.writeText(entry.mountPath);
	vscode.window.setStatusBarMessage(`Copied include mount path ${entry.mountPath}`, 2000);
}

async function handleChooseIncludeRootCommand(mode: "replace" | "append"): Promise<void> {
	const workspaceFolders = vscode.workspace.workspaceFolders?.filter((folder) => folder.uri.scheme === "file") ?? [];
	if (workspaceFolders.length === 0) {
		await vscode.window.showInformationMessage("Open a file-backed workspace to configure include roots.");
		return;
	}
	const quickPickItems = await buildIncludeRootQuickPickItems(workspaceFolders);
	quickPickItems.push({
		label: "$(folder-opened) Browse for folder…",
		description: "Select an include root directory from the filesystem",
		selectionType: "browse",
	});
	const placeHolder = mode === "replace" ? "Select the include root to use" : "Select an additional include root";
	const pick = await vscode.window.showQuickPick(quickPickItems, {
		placeHolder,
		matchOnDescription: true,
		matchOnDetail: true,
	});
	if (!pick) {
		return;
	}
	if (pick.selectionType === "browse") {
		const selection = await vscode.window.showOpenDialog({ canSelectFiles: false, canSelectFolders: true, canSelectMany: false });
		if (!selection || selection.length === 0) {
			return;
		}
		const targetUri = selection[0];
		const workspaceFolder = vscode.workspace.getWorkspaceFolder(targetUri);
		if (!workspaceFolder || workspaceFolder.uri.scheme !== "file") {
			await vscode.window.showWarningMessage("Choose a folder inside an open workspace to use it as an include root.");
			return;
		}
		const configurationPath = computeConfigurationPath(workspaceFolder, targetUri);
		await updateWorkspaceIncludeRoots(workspaceFolder, configurationPath, mode);
		return;
	}
	if (!pick.workspaceFolder || !pick.configurationPath) {
		await vscode.window.showWarningMessage("Select a workspace include root to continue.");
		return;
	}
	await updateWorkspaceIncludeRoots(pick.workspaceFolder, pick.configurationPath, mode);
}

async function handleClearIncludeRootsCommand(): Promise<void> {
	const workspaceFolders = vscode.workspace.workspaceFolders?.filter((folder) => folder.uri.scheme === "file") ?? [];
	if (workspaceFolders.length === 0) {
		await vscode.window.showInformationMessage("Open a file-backed workspace to clear include roots.");
		return;
	}
	const candidates = workspaceFolders
		.map((folder) => {
			const key = folder.uri.toString();
			const configured = includeConfig.rawRootsByWorkspace.get(key) ?? [];
			const hasErrors = includeConfig.rootErrors.some((error) => error.workspaceFolder && error.workspaceFolder.uri.toString() === key);
			return { folder, configured, hasErrors };
		})
		.filter((entry) => entry.configured.length > 0 || entry.hasErrors);
	if (candidates.length === 0) {
		await vscode.window.showInformationMessage("No include roots are configured for the current workspace.");
		return;
	}
	let targetFolder: vscode.WorkspaceFolder;
	if (candidates.length === 1) {
		targetFolder = candidates[0].folder;
	} else {
		const pick = await vscode.window.showQuickPick(
			candidates.map((entry) => ({
				label: entry.folder.name,
				description: entry.folder.uri.fsPath,
				detail: entry.configured.length > 0 ? entry.configured.join(", ") : "Root entries reported issues.",
				folder: entry.folder,
			})),
			{ placeHolder: "Select the workspace to clear include roots" },
		);
		if (!pick || !pick.folder) {
			return;
		}
		targetFolder = pick.folder;
	}
	const config = vscode.workspace.getConfiguration(INCLUDE_CONFIG_SECTION, targetFolder);
	await config.update("roots", [], vscode.ConfigurationTarget.WorkspaceFolder);
	vscode.window.setStatusBarMessage(`Cleared include roots for ${targetFolder.name}`, 2000);
}

async function buildIncludeRootQuickPickItems(workspaceFolders: vscode.WorkspaceFolder[]): Promise<IncludeRootQuickPickItem[]> {
	const items: IncludeRootQuickPickItem[] = [];
	const seenKeys = new Set<string>();
	for (const folder of workspaceFolders) {
		const folderKey = folder.uri.toString();
		const folderPath = folder.uri.fsPath;
		if (!folderPath) {
			continue;
		}
		const configured = includeConfig.rawRootsByWorkspace.get(folderKey) ?? [];
		const isConfigured = (candidate: string) => configured.includes(candidate);
		const addItem = (
			label: string,
			description: string,
			detail: string | undefined,
			configurationPath: string,
			targetUri: vscode.Uri,
			selectionType: "candidate" | "recent",
		) => {
			const key = `${folderKey}::${configurationPath}`;
			if (seenKeys.has(key)) {
				return;
			}
			seenKeys.add(key);
			items.push({
				label,
				description,
				detail: detail
					? isConfigured(configurationPath)
						? `${detail} - Configured`
						: detail
					: isConfigured(configurationPath)
						? "Configured"
						: undefined,
				picked: isConfigured(configurationPath),
				workspaceFolder: folder,
				configurationPath,
				targetUri,
				selectionType,
			});
		};
		addItem(`${folder.name}/.`, folderPath, "Workspace root", ".", folder.uri, "candidate");
		const notableDirectories = await collectNotableIncludeDirectories(folder);
		for (const uri of notableDirectories) {
			const configurationPath = computeConfigurationPath(folder, uri);
			const displayPath = configurationPath === "." ? "." : configurationPath;
			addItem(`${folder.name}/${displayPath}`, uri.fsPath, "Contains include assets", configurationPath, uri, "candidate");
		}
	}
	for (const rootPath of includeLastUsedRoots) {
		const uri = vscode.Uri.file(rootPath);
		const workspaceFolder = vscode.workspace.getWorkspaceFolder(uri);
		if (!workspaceFolder || workspaceFolder.uri.scheme !== "file") {
			continue;
		}
		const configurationPath = computeConfigurationPath(workspaceFolder, uri);
		const label = describeIncludeRootFromPath(rootPath) ?? `${workspaceFolder.name}/${configurationPath}`;
		const description = uri.fsPath;
		const detail = "Recent include root";
		const key = `${workspaceFolder.uri.toString()}::${configurationPath}`;
		if (seenKeys.has(key)) {
			continue;
		}
		items.push({
			label,
			description,
			detail,
			workspaceFolder,
			configurationPath,
			targetUri: uri,
			selectionType: "recent",
			picked: (includeConfig.rawRootsByWorkspace.get(workspaceFolder.uri.toString()) ?? []).includes(configurationPath),
		});
		seenKeys.add(key);
	}
	return items;
}

async function collectNotableIncludeDirectories(folder: vscode.WorkspaceFolder): Promise<vscode.Uri[]> {
	try {
		const pattern = new vscode.RelativePattern(folder, "**/*.{ivg,impd}");
		const files = await vscode.workspace.findFiles(pattern, undefined, 200);
		const directories = new Map<string, vscode.Uri>();
		for (const file of files) {
			const directoryPath = path.dirname(file.fsPath);
			if (!directories.has(directoryPath)) {
				directories.set(directoryPath, vscode.Uri.file(directoryPath));
			}
		}
		return Array.from(directories.values()).slice(0, 50);
	} catch (error) {
		logIncludeTelemetry(`Failed to discover include root candidates in ${folder.name}: ${formatError(error)}`);
		return [];
	}
}

function computeConfigurationPath(folder: vscode.WorkspaceFolder, targetUri: vscode.Uri): string {
	const folderPath = folder.uri.fsPath;
	const targetPath = targetUri.fsPath;
	if (!folderPath || !targetPath) {
		return toPosixPath(targetUri.fsPath);
	}
	const relative = safeRelativePath(folderPath, targetPath);
	if (typeof relative === "string") {
		return relative === "" ? "." : toPosixPath(relative);
	}
	return toPosixPath(targetPath);
}

async function updateWorkspaceIncludeRoots(
	folder: vscode.WorkspaceFolder,
	configurationPath: string,
	mode: "replace" | "append",
): Promise<void> {
	const config = vscode.workspace.getConfiguration(INCLUDE_CONFIG_SECTION, folder);
	const current = config.get<string[]>("roots", []) ?? [];
	const normalized = configurationPath === "." ? "." : toPosixPath(configurationPath);
	let next: string[];
	if (mode === "replace") {
		next = [normalized];
	} else {
		const combined = new Map<string, string>();
		for (const entry of current) {
			combined.set(entry, entry);
		}
		combined.set(normalized, normalized);
		next = Array.from(combined.values());
	}
	await config.update("roots", next, vscode.ConfigurationTarget.WorkspaceFolder);
	const action = mode === "replace" ? "Set" : "Added";
	vscode.window.setStatusBarMessage(`${action} include root for ${folder.name}`, 2000);
}

function readPreviewConfig(): PreviewConfig {
	const config = vscode.workspace.getConfiguration(CONFIG_SECTION);
	const autoRefresh = config.get<boolean>("autoRefresh", true);
	const configuredDebounce = config.get<number>("debounceMs", DEFAULT_DEBOUNCE_MS);
	const debounceMs = Number.isFinite(configuredDebounce) && configuredDebounce >= 0 ? configuredDebounce : DEFAULT_DEBOUNCE_MS;
	return {
		autoRefresh,
		debounceMs,
	};
}

function readGeneralConfig(): GeneralConfig {
	const config = vscode.workspace.getConfiguration(GENERAL_CONFIG_SECTION);
	const syncOnOpen = config.get<boolean>("syncOnOpen", true);
	const configuredDelay = config.get<number>("webviewUpdateDelay", 0);
	const webviewUpdateDelay = Number.isFinite(configuredDelay) && configuredDelay >= 0 ? configuredDelay : 0;
	return {
		syncOnOpen,
		webviewUpdateDelay,
	};
}

function readIncludeConfig(): IncludeConfig {
	const config = vscode.workspace.getConfiguration(INCLUDE_CONFIG_SECTION);
	const watchersEnabled = config.get<boolean>("watchersEnabled", true);
	const manifestEnabled = config.get<boolean>("manifestEnabled", false);
	const autoRescan = config.get<boolean>("autoRescan", true);
	const configuredDebounce = config.get<number>("rescanDebounceMs", DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS);
	const rescanDebounceMs =
		Number.isFinite(configuredDebounce) && configuredDebounce >= 0 ? configuredDebounce : DEFAULT_INCLUDE_RESCAN_DEBOUNCE_MS;
	const autoRevealExplorer = config.get<boolean>("autoRevealExplorer", true);
	const rootScan = collectIncludeRoots();
	return {
		watchersEnabled,
		manifestEnabled,
		autoRescan,
		rescanDebounceMs,
		autoRevealExplorer,
		roots: rootScan.configured,
		effectiveRoots: rootScan.effective,
		rootErrors: rootScan.errors,
		rawRootsByWorkspace: rootScan.rawRootsByWorkspace,
	};
}

interface IncludeRootScanResult {
	configured: IncludeRootConfigEntry[];
	effective: IncludeRootConfigEntry[];
	errors: IncludeRootConfigError[];
	rawRootsByWorkspace: Map<string, string[]>;
}

function collectIncludeRoots(): IncludeRootScanResult {
	const workspaceFolders = vscode.workspace.workspaceFolders ?? [];
	const configured: IncludeRootConfigEntry[] = [];
	const errors: IncludeRootConfigError[] = [];
	const defaults: IncludeRootConfigEntry[] = [];
	const rawRootsByWorkspace = new Map<string, string[]>();
	const seenConfigured = new Set<string>();
	const seenDefaults = new Set<string>();
	for (const folder of workspaceFolders) {
		if (folder.uri.scheme !== "file") {
			continue;
		}
		const folderKey = folder.uri.toString();
		const folderConfig = vscode.workspace.getConfiguration(INCLUDE_CONFIG_SECTION, folder);
		const rawRoots = folderConfig.get<unknown>("roots", []);
		const sanitizedRoots: string[] = [];
		if (Array.isArray(rawRoots)) {
			for (const raw of rawRoots) {
				if (typeof raw !== "string") {
					continue;
				}
				const trimmed = raw.trim();
				if (!trimmed) {
					continue;
				}
				sanitizedRoots.push(trimmed);
				const resolved = resolveIncludeRootFsPath(folder, trimmed);
				if (!resolved) {
					errors.push({
						workspaceFolder: folder,
						sourcePath: trimmed,
						message: "Resolve include roots against file-backed workspace folders.",
					});
					continue;
				}
				const normalizedFsPath = normalizeFileSystemPath(resolved);
				let directoryMissing = false;
				try {
					const stat = fs.statSync(normalizedFsPath);
					if (!stat.isDirectory()) {
						errors.push({ workspaceFolder: folder, sourcePath: trimmed, message: "Include root must point to a directory." });
						continue;
					}
				} catch (error) {
					directoryMissing = true;
				}
				if (directoryMissing) {
					errors.push({ workspaceFolder: folder, sourcePath: trimmed, message: "Include root directory not found." });
					continue;
				}
				const id = createIncludeRootId(normalizedFsPath);
				if (seenConfigured.has(id)) {
					continue;
				}
				seenConfigured.add(id);
				configured.push(
					createIncludeRootEntry({
						origin: "configured",
						workspaceFolder: folder,
						sourcePath: trimmed,
						resolvedFsPath: normalizedFsPath,
					}),
				);
			}
		}
		rawRootsByWorkspace.set(folderKey, Array.from(new Set(sanitizedRoots)));
		const workspaceFsPath = folder.uri.fsPath;
		if (workspaceFsPath) {
			const defaultResolved = normalizeFileSystemPath(workspaceFsPath);
			const defaultId = createIncludeRootId(defaultResolved);
			if (!seenDefaults.has(defaultId)) {
				defaults.push(
					createIncludeRootEntry({
						origin: "workspace",
						workspaceFolder: folder,
						sourcePath: workspaceFsPath,
						resolvedFsPath: defaultResolved,
					}),
				);
				seenDefaults.add(defaultId);
			}
		}
	}
	const effective = configured.length > 0 ? configured : defaults;
	return {
		configured,
		effective,
		errors,
		rawRootsByWorkspace,
	};
}

function createIncludeRootEntry(params: {
	origin: "configured" | "workspace";
	workspaceFolder?: vscode.WorkspaceFolder;
	sourcePath: string;
	resolvedFsPath: string;
}): IncludeRootConfigEntry {
	const normalizedFsPath = normalizeFileSystemPath(params.resolvedFsPath);
	const uri = vscode.Uri.file(normalizedFsPath);
	let displayName = toPosixPath(normalizedFsPath);
	if (params.workspaceFolder) {
		const folderPath = params.workspaceFolder.uri.fsPath;
		if (folderPath) {
			const relative = safeRelativePath(folderPath, normalizedFsPath);
			if (typeof relative === "string") {
				displayName = `${params.workspaceFolder.name}/${relative ? toPosixPath(relative) : "."}`;
			} else {
				displayName = `${params.workspaceFolder.name} → ${toPosixPath(normalizedFsPath)}`;
			}
		}
	}
	return {
		id: createIncludeRootId(normalizedFsPath),
		origin: params.origin,
		workspaceFolder: params.workspaceFolder,
		sourcePath: params.sourcePath,
		resolvedFsPath: normalizedFsPath,
		uri,
		displayName,
	};
}

function resolveIncludeRootFsPath(folder: vscode.WorkspaceFolder, sourcePath: string): string | undefined {
	if (folder.uri.scheme !== "file") {
		return undefined;
	}
	if (path.isAbsolute(sourcePath)) {
		return path.normalize(sourcePath);
	}
	const folderPath = folder.uri.fsPath;
	if (!folderPath) {
		return undefined;
	}
	return path.normalize(path.join(folderPath, sourcePath));
}

function safeRelativePath(base: string, target: string): string | undefined {
	const relative = path.relative(base, target);
	if (!relative || relative === ".") {
		return "";
	}
	if (relative.startsWith("..")) {
		return undefined;
	}
	return relative;
}

function normalizeFileSystemPath(fsPath: string): string {
	if (!fsPath) {
		return fsPath;
	}
	return path.resolve(fsPath);
}

function createIncludeRootId(fsPath: string): string {
	const normalized = normalizeFileSystemPath(fsPath).replace(/\\+/g, "/");
	return process.platform === "win32" ? normalized.toLowerCase() : normalized;
}

function toPosixPath(fsPath: string): string {
	return fsPath.replace(/\\+/g, "/");
}

function isPathInside(parent: string, candidate: string): boolean {
	const normalizedParent = normalizeFileSystemPath(parent);
	const normalizedCandidate = normalizeFileSystemPath(candidate);
	if (normalizedParent === normalizedCandidate) {
		return true;
	}
	const relative = path.relative(normalizedParent, normalizedCandidate);
	return Boolean(relative) && !relative.startsWith("..") && !path.isAbsolute(relative);
}

function normalizeIncludeRelativePath(candidate: string): string {
	const normalized = candidate.replace(/\\/g, "/").replace(/^\.\//, "");
	return normalized.startsWith("/") ? normalized.slice(1) : normalized;
}

function appendIncludeRootDiagnostics(lines: string[]): void {
	if (includeWatcherRootLabels.length > 0) {
		lines.push(`Active include roots (${includeWatcherRootLabels.length}):`);
		for (const label of includeWatcherRootLabels) {
			lines.push(` • ${label}`);
		}
	}
	const configuredCount = includeConfig.roots.length;
	if (configuredCount > 0) {
		lines.push(`Configured include roots (${configuredCount}):`);
		for (const root of includeConfig.roots) {
			lines.push(` • ${root.displayName}`);
		}
	} else {
		lines.push("Configured include roots: none (watching workspace folders).");
	}
	if (includeConfig.rootErrors.length > 0) {
		lines.push("Include root issues:");
		for (const error of includeConfig.rootErrors) {
			lines.push(` • ${formatIncludeRootError(error)}`);
		}
	}
	if (includeLastUsedRoots.length > 0) {
		const lastRoots = includeLastUsedRoots
			.map((pathValue) => describeIncludeRootFromPath(pathValue) ?? toPosixPath(pathValue))
			.filter((label, index, array) => array.indexOf(label) === index);
		if (lastRoots.length > 0) {
			lines.push(`Last manifest roots (${lastRoots.length}):`);
			for (const label of lastRoots) {
				lines.push(` • ${label}`);
			}
		}
	}
}

function formatIncludeRootError(error: IncludeRootConfigError): string {
	const scope = error.workspaceFolder ? error.workspaceFolder.name : "workspace";
	return `${scope}: ${error.sourcePath} - ${error.message}`;
}

function describeIncludeRootFromPath(fsPath: string): string | undefined {
	const normalized = normalizeFileSystemPath(fsPath);
	for (const root of includeConfig.effectiveRoots) {
		if (createIncludeRootId(root.resolvedFsPath) === createIncludeRootId(normalized)) {
			return root.displayName;
		}
	}
	return undefined;
}

function initializeIncludeWatchers(context: vscode.ExtensionContext): void {
	disposeIncludeWatchers();
	includeWatcherEventCounts = {
		create: 0,
		change: 0,
		delete: 0,
	};
	includeWatcherRootCount = 0;
	includeWatcherCandidateRootCount = 0;
	includeWatcherUnavailableMessage = undefined;
	includeWatcherRootLabels = [];
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
	if (fileBackedFolders.length === 0) {
		logIncludeTelemetry("Include watchers unavailable for non-file workspace folders.");
		includeWatcherUnavailableMessage = "Include watchers require file-backed workspace folders.";
		includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
		queueEmptyIncludeBundleMessage("watchers-unavailable");
		refreshStatusBar();
		refreshIncludeSurfaces();
		return;
	}
	const candidateRoots = includeConfig.effectiveRoots.length > 0 ? includeConfig.effectiveRoots : [];
	includeWatcherCandidateRootCount = candidateRoots.length;
	if (candidateRoots.length === 0) {
		logIncludeTelemetry("Include watchers pending include roots.");
		includeWatcherUnavailableMessage = "Configure at least one include root to enable include watching.";
		includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
		queueEmptyIncludeBundleMessage("watchers-unavailable");
		refreshStatusBar();
		refreshIncludeSurfaces();
		return;
	}
	for (const root of candidateRoots) {
		const pattern = new vscode.RelativePattern(root.resolvedFsPath, INCLUDE_ASSET_GLOB);
		const watcher = vscode.workspace.createFileSystemWatcher(pattern);
		includeWatchers.push(watcher);
		includeWatcherDisposables.push(
			watcher.onDidCreate((uri) => handleIncludeWatcherEvent("create", uri)),
			watcher.onDidChange((uri) => handleIncludeWatcherEvent("change", uri)),
			watcher.onDidDelete((uri) => handleIncludeWatcherEvent("delete", uri)),
		);
		includeWatcherRootCount += 1;
		includeWatcherRootLabels.push(root.displayName);
	}
	logIncludeTelemetry(
		`Include watchers attached across ${includeWatcherRootCount} include root${includeWatcherRootCount === 1 ? "" : "s"} using pattern ${INCLUDE_ASSET_GLOB}.`,
	);
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

function disposeIncludeWatchers(): void {
	for (const disposable of includeWatcherDisposables) {
		disposable.dispose();
	}
	includeWatcherDisposables = [];
	for (const watcher of includeWatchers) {
		watcher.dispose();
	}
	includeWatchers = [];
	if (includeWatcherRootCount > 0) {
		logIncludeTelemetry("Include watchers disposed.");
	}
	includeWatcherRootCount = 0;
	includeWatcherCandidateRootCount = 0;
	includeWatcherUnavailableMessage = undefined;
	includeWatcherEventCounts = {
		create: 0,
		change: 0,
		delete: 0,
	};
	includeWatcherRootLabels = [];
	cancelIncludeManifestScheduling();
	includeManifestSnapshot = undefined;
	queueEmptyIncludeBundleMessage("watchers-disposed");
	latestIncludeBundleMessage = undefined;
	includeManifestManualRefreshRequired = false;
	includeExplorerLastAutoRevealRevision = undefined;
	includeMissingRescanPaths.clear();
	refreshIncludeSurfaces();
}

function handleIncludeWatcherEvent(kind: IncludeWatcherEvent, uri: vscode.Uri): void {
	includeWatcherEventCounts[kind] += 1;
	const relativeInfo = getIncludeRelativePath(uri);
	const locationLabel = relativeInfo.matchedRoot
		? `${relativeInfo.relativePath} @ ${relativeInfo.matchedRoot.displayName}`
		: relativeInfo.relativePath;
	logIncludeTelemetry(
		`Include asset ${kind} detected at ${locationLabel} (events: create=${includeWatcherEventCounts.create}, change=${includeWatcherEventCounts.change}, delete=${includeWatcherEventCounts.delete}).`,
	);
	refreshStatusBar();
	scheduleIncludeRefresh();
	includeMissingRescanPaths.clear();
	if (includeConfig.manifestEnabled) {
		scheduleIncludeManifestBuild("watcherEvent");
	}
	refreshIncludeSurfaces();
}

function scheduleIncludeRefresh(): void {
	const targetDocument = getActiveIvgDocument() ?? getLastPreviewDocument();
	if (!targetDocument) {
		return;
	}
	scheduleDocument(targetDocument);
}

function scheduleIncludeManifestBuild(reason: IncludeManifestScheduleReason): void {
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

async function buildIncludeManifest(reason: IncludeManifestScheduleReason): Promise<void> {
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
		const uriSet = new Map<string, vscode.Uri>();
		for (const root of includeConfig.effectiveRoots) {
			try {
				const pattern = new vscode.RelativePattern(root.resolvedFsPath, INCLUDE_ASSET_GLOB);
				const rootUris = await vscode.workspace.findFiles(pattern);
				for (const uri of rootUris) {
					const id = createIncludeRootId(uri.fsPath);
					if (!uriSet.has(id)) {
						uriSet.set(id, uri);
					}
				}
			} catch (error) {
				logIncludeTelemetry(`Failed to enumerate include assets under ${root.displayName}: ${formatError(error)}`);
			}
		}
		const uris = Array.from(uriSet.values());
		const records: IncludeManifestBuildItem[] = [];
		let totalBytes = 0;
		const usedRootPaths = new Set<string>();
		for (const uri of uris) {
			try {
				const relativeInfo = getIncludeRelativePath(uri);
				const relativePath = relativeInfo.relativePath;
				if (relativeInfo.matchedRoot) {
					usedRootPaths.add(relativeInfo.matchedRoot.resolvedFsPath);
				}
				const content = await vscode.workspace.fs.readFile(uri);
				const buffer = Buffer.from(content);
				const byteLength = buffer.byteLength;
				totalBytes += byteLength;
				const checksum = createHash("sha256").update(buffer).digest("hex");
				const mountPath = relativePath.startsWith("/") ? relativePath : `/${relativePath}`;
				const entry: IncludeManifestEntry = {
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
			} catch (error) {
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
		includeLastUsedRoots = Array.from(usedRootPaths.values());
		await extensionContext.workspaceState.update(WORKSPACE_STATE_INCLUDE_ROOTS_KEY, includeLastUsedRoots);
		const serializedBytes = Buffer.byteLength(JSON.stringify({ manifest, assets }), "utf8");
		includeManifestSnapshot = manifest;
		includeManifestRevisionId = revisionId;
		includeManifestEntryCount = records.length;
		includeManifestTotalBytes = totalBytes;
		includeManifestLastGeneratedAt = generatedAt;
		includeManifestStatus = "ready";
		logIncludeTelemetry(
			`Include manifest ready (${records.length} asset${records.length === 1 ? "" : "s"}, ${totalBytes} byte${totalBytes === 1 ? "" : "s"}, revision ${revisionId}, ${Date.now() - startedAt} ms).`,
		);
		logIncludeTelemetry(
			`Include bundle prepared (${assets.length} asset${assets.length === 1 ? "" : "s"}, ${serializedBytes} serialized byte${serializedBytes === 1 ? "" : "s"}) for revision ${revisionId}.`,
		);
		queueIncludeBundleMessage({
			type: "setIncludeBundle",
			revision: revisionId,
			manifest,
			assets,
		});
		refreshIncludeSurfaces();
		maybeAutoRevealIncludeExplorer(revisionId);
	} catch (error) {
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

function getIncludeRelativePath(uri: vscode.Uri): IncludeRelativePathResult {
	const normalizedFsPath = normalizeFileSystemPath(uri.fsPath);
	let matchedRoot: IncludeRootConfigEntry | undefined;
	let matchedRelative: string | undefined;
	for (const root of includeConfig.effectiveRoots) {
		if (!isPathInside(root.resolvedFsPath, normalizedFsPath)) {
			continue;
		}
		const relative = path.relative(root.resolvedFsPath, normalizedFsPath);
		if (!relative || relative.startsWith("..") || path.isAbsolute(relative)) {
			continue;
		}
		if (!matchedRoot || root.resolvedFsPath.length > matchedRoot.resolvedFsPath.length) {
			matchedRoot = root;
			matchedRelative = relative;
		}
	}
	if (matchedRoot && typeof matchedRelative === "string") {
		return {
			relativePath: normalizeIncludeRelativePath(matchedRelative),
			matchedRoot,
		};
	}
	const relative = vscode.workspace.asRelativePath(uri, false);
	const candidate = relative && relative !== uri.fsPath ? relative : uri.path;
	return {
		relativePath: normalizeIncludeRelativePath(candidate),
	};
}

function inferIncludeMimeType(relativePath: string): string {
	const lower = relativePath.toLowerCase();
	for (const [extension, mime] of MIME_TYPE_BY_EXTENSION) {
		if (lower.endsWith(`.${extension}`)) {
			return mime;
		}
	}
	return "application/octet-stream";
}

function createIncludeRevisionId(records: IncludeManifestBuildItem[]): string {
	const hash = createHash("sha256");
	for (const record of records) {
		hash.update(record.entry.mountPath);
		hash.update(record.entry.checksum);
	}
	const digest = hash.digest("hex").slice(0, 12);
	const timestamp = Date.now().toString(36);
	return `rev-${timestamp}-${digest}`;
}

function createIncludeBundleAssets(records: IncludeManifestBuildItem[]): IncludeBundleAssetPayload[] {
	return records.map((record) => ({
		path: record.relativePath,
		checksum: record.entry.checksum,
		mimeType: record.entry.mimeType,
		data: Buffer.from(record.content).toString("base64"),
	}));
}

async function persistIncludeRevision(
	context: vscode.ExtensionContext,
	revisionId: string,
	records: IncludeManifestBuildItem[],
	generatedAt: string,
): Promise<{ manifest: IncludeManifest }> {
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
	const manifest: IncludeManifest = {
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

async function pruneStaleIncludeRevisions(storageRoot: vscode.Uri, activeRevisionId: string): Promise<void> {
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
	} catch (error) {
		logIncludeTelemetry(`Failed to prune stale include revisions: ${formatError(error)}`);
	}
}

function cancelIncludeManifestScheduling(): void {
	if (includeManifestTimer) {
		clearTimeout(includeManifestTimer);
		includeManifestTimer = undefined;
	}
	includeManifestRebuildQueued = false;
}

function refreshIncludeSurfaces(): void {
	refreshIncludeStatusBarItem();
	if (includeExplorerProvider) {
		includeExplorerProvider.refresh();
	}
}

function refreshIncludeStatusBarItem(): void {
	if (!includeStatusBarItem) {
		return;
	}
	const summary = getIncludeStatusSummary();
	const rootCount = includeConfig.watchersEnabled ? includeConfig.effectiveRoots.length : 0;
	let icon = "package";
	if (!includeConfig.watchersEnabled) {
		icon = "eye-closed";
	} else if (includeManifestStatus === "error") {
		icon = "warning";
	} else if (includeManifestManualRefreshRequired) {
		icon = "history";
	} else if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
		icon = "sync~spin";
	} else if (summary.label.startsWith("includes unavailable")) {
		icon = "workspace-untrusted";
	} else if (summary.label.startsWith("includes pending")) {
		icon = "sync";
	}
	const tooltipLines = [...summary.tooltipLines];
	if (tooltipLines.length > 0) {
		tooltipLines.push("");
	}
	tooltipLines.push("Click to open the IVG Include explorer.");
	if (includeManifestManualRefreshRequired) {
		tooltipLines.push('Run "IVGFiddle: Rescan Include Assets" to rebuild the manifest.');
	} else if (includeConfig.manifestEnabled) {
		tooltipLines.push('Use "IVGFiddle: Rescan Include Assets" to force a rebuild.');
	}
	tooltipLines.push('Configure roots with "IVGFiddle: Set Include Root…", "Add Another Root…", or "Clear Roots".');
	const rootSuffix = rootCount > 0 ? ` • roots ${rootCount}` : "";
	includeStatusBarItem.text = `$(${icon}) Includes: ${summary.label}${rootSuffix}`;
	includeStatusBarItem.tooltip = tooltipLines.join("\n");
	includeStatusBarItem.command = includeManifestManualRefreshRequired ? "ivgfiddle.includes.rescan" : "ivgfiddle.includes.focusExplorer";
	includeStatusBarItem.show();
}

function maybeAutoRevealIncludeExplorer(revisionId: string | undefined): void {
	if (!includeConfig.autoRevealExplorer || !includeExplorerView || !revisionId) {
		return;
	}
	if (includeExplorerLastAutoRevealRevision === revisionId) {
		return;
	}
	includeExplorerLastAutoRevealRevision = revisionId;
	void vscode.commands.executeCommand("ivgfiddleIncludesExplorer.focus");
}

function formatError(error: unknown): string {
	if (error instanceof Error && error.message) {
		return error.message;
	}
	return String(error);
}

function logIncludeTelemetry(message: string): void {
	const timestamp = new Date().toISOString();
	console.log(`[IVG Include] ${message}`);
	appendTraceOutputLine(`[include ${timestamp}] ${message}`);
}

function processStatusMessage(message: { level?: unknown; message?: unknown; durationMs?: unknown }): void {
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

function refreshStatusBar(): void {
	refreshIncludeStatusBarItem();
	if (!currentStatusDocumentUri) {
		return;
	}
	const document = vscode.workspace.textDocuments.find((openDocument) => openDocument.uri.toString() === currentStatusDocumentUri);
	if (document) {
		showStatusBar(document, "focus");
	}
}

function clearTransientStatusMessage(): void {
	if (transientStatusMessage) {
		transientStatusMessage.dispose();
		transientStatusMessage = undefined;
	}
}

function handleIncludeMissingTraceLine(line: string): void {
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

function processTraceMessage(raw: unknown): void {
	if (!raw || (typeof raw !== "object" && typeof raw !== "function")) {
		return;
	}
	const payload = raw as { action?: unknown; text?: unknown; lines?: unknown };
	const action = typeof payload.action === "string" ? payload.action : "";
	if (!action) {
		return;
	}
	if (action === "clear") {
		clearTraceOutput();
		return;
	}
	if (action === "reset") {
		const lines = Array.isArray(payload.lines) ? payload.lines.filter((entry): entry is string => typeof entry === "string") : [];
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

function getTraceOutputChannel(): vscode.OutputChannel {
	if (!traceOutputChannel) {
		traceOutputChannel = vscode.window.createOutputChannel(TRACE_OUTPUT_CHANNEL_NAME);
	}
	return traceOutputChannel;
}

function clearTraceOutput(): void {
	traceOutputLines = [];
	if (traceOutputChannel) {
		traceOutputChannel.clear();
	}
}

function resetTraceOutput(lines: string[]): void {
	const channel = getTraceOutputChannel();
	traceOutputLines = lines.slice();
	channel.clear();
	for (const line of traceOutputLines) {
		channel.appendLine(line);
	}
}

function appendTraceOutputLine(line: string): void {
	traceOutputLines.push(line);
	getTraceOutputChannel().appendLine(line);
}

function replaceLastTraceOutputLine(line: string): void {
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

function revealTraceOutput(preserveFocus?: boolean): void {
	const channel = getTraceOutputChannel();
	channel.show(preserveFocus === true);
}

function showTraceErrorMessage(message: string): void {
	const action = TRACE_SHOW_ACTION;
	vscode.window.showErrorMessage(message, action).then((selection) => {
		if (selection === action) {
			revealTraceOutput();
		}
	});
}
