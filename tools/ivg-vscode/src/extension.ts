import * as vscode from "vscode";
import { createHash } from "crypto";

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
// Track include-eligible assets that must stay in sync with IVGFiddle. IMPD composition (`.impd`),
// inline vector graphics (`.ivg`), IVG fonts (`.ivgfont`), and raster fallbacks (`.png`) are
// pre-packaged alongside previews, so the watcher focuses on that set of extensions.
const INCLUDE_ASSET_GLOB = "**/*.{impd,ivg,ivgfont,png}";
const TRACE_OUTPUT_CHANNEL_NAME = "IVG Preview Trace";
const TRACE_SHOW_ACTION = "Show Trace Output";
const INCLUDE_SERVICE_PRESIGN_PATH = "/include-bundles/presign";
const INCLUDE_SERVICE_DEFAULT_METHOD = "PUT";

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
	serviceBaseUrl?: string;
	serviceAuthToken?: string;
}

let previewConfig: PreviewConfig = readPreviewConfig();
let generalConfig: GeneralConfig = readGeneralConfig();
let includeConfig: IncludeConfig = readIncludeConfig();

let includeWatchers: vscode.FileSystemWatcher[] = [];
let includeWatcherDisposables: vscode.Disposable[] = [];
let includeWatcherFolderCount = 0;
let includeWatcherCandidateFolderCount = 0;
let includeWatcherUnavailableMessage: string | undefined;
let includeWatcherEventCounts: Record<"create" | "change" | "delete", number> = {
	create: 0,
	change: 0,
	delete: 0,
};
type IncludeWatcherEvent = "create" | "change" | "delete";

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
	uploadedAt?: string;
	bundle?: {
		id?: string;
		downloadUrl?: string;
		expiresAt?: string;
		latencyMs?: number;
	};
}

type IncludeUploadStatus = "idle" | "pending" | "uploading" | "ready" | "error" | "disabled";

interface IncludeUploadTarget {
	bundleId: string;
	uploadUrl?: string;
	downloadUrl?: string;
	method?: string;
	headers?: Record<string, string>;
	expiresAt?: string;
}

type IncludeUploadScheduleReason = "manifestReady" | "retry" | "configuration";

type IncludeManifestScheduleReason = "activation" | "watcherInitialization" | "watcherEvent" | "configuration" | "chained";

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
let includeBundlePayload: Uint8Array | undefined;
let latestIncludeBundleMessage: IncludeBundleMessage | undefined;

let includeUploadStatus: IncludeUploadStatus = "idle";
let includeUploadRevisionId: string | undefined;
let includeUploadBundleId: string | undefined;
let includeUploadLastError: string | undefined;
let includeUploadLastCompletedAt: string | undefined;
let includeUploadLatencyMs: number | undefined;
let includeUploadTimer: ReturnType<typeof setTimeout> | undefined;
let includeUploadInProgress = false;
let includeUploadQueued = false;
let includeUploadRetryCount = 0;

const MIME_TYPE_BY_EXTENSION = new Map<string, string>([
	["impd", "application/json"],
	["ivg", "application/xml"],
	["ivgfont", "application/octet-stream"],
	["png", "image/png"],
]);

export function activate(context: vscode.ExtensionContext): void {
	console.log("IVG Preview extension activated");
	extensionContext = context;

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
				processStatusMessage(message as { level?: unknown; message?: unknown; durationMs?: unknown });
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
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.watchersEnabled`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.manifestEnabled`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.serviceBaseUrl`) ||
				event.affectsConfiguration(`${INCLUDE_CONFIG_SECTION}.serviceAuthToken`)
			) {
				includeConfig = readIncludeConfig();
				initializeIncludeWatchers(context);
				if (
					includeConfig.manifestEnabled &&
					includeManifestStatus === "ready" &&
					includeManifestSnapshot &&
					includeManifestRevisionId
				) {
					scheduleIncludeBundleUpload("configuration");
				}
				refreshStatusBar();
			}
		}),
	);
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
		tooltipLines.push("Include manifest disabled");
		return {
			label: `includes watching${eventSuffix}`,
			tooltipLines,
		};
	}
	if (includeManifestStatus === "error") {
		const detail = includeManifestLastError ? `: ${includeManifestLastError}` : "";
		tooltipLines.push(`Include manifest error${detail}`);
		appendIncludeUploadTooltip(tooltipLines);
		const label = includeUploadStatus === "error" ? "includes upload error" : "includes error";
		return {
			label,
			tooltipLines,
		};
	}
	if (includeManifestStatus === "building" || includeManifestStatus === "pending") {
		tooltipLines.push(`Include manifest status: ${includeManifestStatus}`);
		appendIncludeUploadTooltip(tooltipLines);
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
		appendIncludeUploadTooltip(tooltipLines);
		let label = includeManifestEntryCount > 0 ? `includes ready (${includeManifestEntryCount})` : "includes ready (empty)";
		if (includeUploadStatus === "pending") {
			label = `${label} • upload pending`;
		} else if (includeUploadStatus === "uploading") {
			label = `${label} • uploading`;
		} else if (includeUploadStatus === "ready") {
			label = `${label} • bundle ready`;
		} else if (includeUploadStatus === "error") {
			label = "includes upload error";
		} else if (includeUploadStatus === "disabled") {
			label = `${label} • upload disabled`;
		}
		return {
			label,
			tooltipLines,
		};
	}
	tooltipLines.push("Include manifest idle");
	appendIncludeUploadTooltip(tooltipLines);
	return {
		label: "includes watching",
		tooltipLines,
	};
	function appendIncludeUploadTooltip(target: string[]): void {
		if (!includeConfig.manifestEnabled) {
			return;
		}
		switch (includeUploadStatus) {
			case "disabled":
				target.push("Include bundle upload disabled — configure ivgfiddle.includes.serviceBaseUrl.");
				return;
			case "pending":
				target.push("Include bundle upload pending.");
				return;
			case "uploading":
				target.push("Include bundle upload in progress…");
				return;
			case "ready": {
				const bundleLabel = includeUploadBundleId ? `bundle ${includeUploadBundleId}` : "bundle";
				const latencyLabel =
					typeof includeUploadLatencyMs === "number" && includeUploadLatencyMs >= 0 ? ` in ${includeUploadLatencyMs} ms` : "";
				target.push(`Include bundle uploaded (${bundleLabel}${latencyLabel}).`);
				if (includeUploadLastCompletedAt) {
					target.push(`Include bundle uploaded at ${includeUploadLastCompletedAt}.`);
				}
				return;
			}
			case "error": {
				const detail = includeUploadLastError ? `: ${includeUploadLastError}` : "";
				target.push(`Include bundle upload failed${detail}.`);
				if (includeUploadRetryCount > 0) {
					target.push(`Include bundle upload retries: ${includeUploadRetryCount}.`);
				}
				return;
			}
			default:
				return;
		}
	}
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
	const configuredBaseUrl = config.get<string>("serviceBaseUrl", "");
	const configuredAuthToken = config.get<string>("serviceAuthToken", "");
	const serviceBaseUrl = configuredBaseUrl && configuredBaseUrl.trim().length > 0 ? configuredBaseUrl.trim() : undefined;
	const serviceAuthToken = configuredAuthToken && configuredAuthToken.trim().length > 0 ? configuredAuthToken.trim() : undefined;
	return {
		watchersEnabled,
		manifestEnabled,
		serviceBaseUrl,
		serviceAuthToken,
	};
}

function initializeIncludeWatchers(context: vscode.ExtensionContext): void {
	disposeIncludeWatchers();
	includeWatcherEventCounts = {
		create: 0,
		change: 0,
		delete: 0,
	};
	includeWatcherFolderCount = 0;
	includeWatcherCandidateFolderCount = 0;
	includeWatcherUnavailableMessage = undefined;
	cancelIncludeUploadScheduling();
	includeManifestSnapshot = undefined;
	includeBundlePayload = undefined;
	latestIncludeBundleMessage = undefined;
	includeUploadStatus = "idle";
	includeUploadRevisionId = undefined;
	includeUploadBundleId = undefined;
	includeUploadLastError = undefined;
	includeUploadLastCompletedAt = undefined;
	includeUploadLatencyMs = undefined;
	includeUploadInProgress = false;
	includeUploadQueued = false;
	includeUploadRetryCount = 0;
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
		refreshStatusBar();
		return;
	}
	const workspaceFolders = vscode.workspace.workspaceFolders;
	if (!workspaceFolders || workspaceFolders.length === 0) {
		logIncludeTelemetry("Include watchers pending workspace folders.");
		includeWatcherUnavailableMessage = `Open a workspace or folder to enable include watching (pattern: ${INCLUDE_ASSET_GLOB})`;
		includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
		refreshStatusBar();
		return;
	}
	const fileBackedFolders = workspaceFolders.filter((folder) => folder.uri.scheme === "file");
	includeWatcherCandidateFolderCount = fileBackedFolders.length;
	if (fileBackedFolders.length === 0) {
		logIncludeTelemetry("Include watchers unavailable for non-file workspace folders.");
		includeWatcherUnavailableMessage = "Include watchers require file-backed workspace folders.";
		includeManifestStatus = includeConfig.manifestEnabled ? "pending" : "idle";
		refreshStatusBar();
		return;
	}
	for (const folder of fileBackedFolders) {
		const pattern = new vscode.RelativePattern(folder, INCLUDE_ASSET_GLOB);
		const watcher = vscode.workspace.createFileSystemWatcher(pattern);
		includeWatchers.push(watcher);
		includeWatcherDisposables.push(
			watcher.onDidCreate((uri) => handleIncludeWatcherEvent("create", uri)),
			watcher.onDidChange((uri) => handleIncludeWatcherEvent("change", uri)),
			watcher.onDidDelete((uri) => handleIncludeWatcherEvent("delete", uri)),
		);
		includeWatcherFolderCount += 1;
	}
	logIncludeTelemetry(
		`Include watchers attached across ${includeWatcherFolderCount} workspace folder${includeWatcherFolderCount === 1 ? "" : "s"} using pattern ${INCLUDE_ASSET_GLOB}.`,
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
		refreshStatusBar();
		return;
	}
	includeManifestStatus = "pending";
	includeUploadStatus = "pending";
	includeUploadLastError = undefined;
	includeUploadBundleId = undefined;
	includeUploadRevisionId = undefined;
	includeUploadLastCompletedAt = undefined;
	includeUploadLatencyMs = undefined;
	includeUploadRetryCount = 0;
	refreshStatusBar();
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
	cancelIncludeUploadScheduling();
	includeManifestSnapshot = undefined;
	includeBundlePayload = undefined;
	latestIncludeBundleMessage = undefined;
	includeUploadStatus = includeConfig.manifestEnabled ? "pending" : "idle";
	includeUploadRevisionId = undefined;
	includeUploadBundleId = undefined;
	includeUploadLastError = undefined;
	includeUploadLastCompletedAt = undefined;
	includeUploadLatencyMs = undefined;
	includeUploadInProgress = false;
	includeUploadQueued = false;
	includeUploadRetryCount = 0;
}

function handleIncludeWatcherEvent(kind: IncludeWatcherEvent, uri: vscode.Uri): void {
	includeWatcherEventCounts[kind] += 1;
	const relativePath = vscode.workspace.asRelativePath(uri, false);
	logIncludeTelemetry(
		`Include asset ${kind} detected at ${relativePath} (events: create=${includeWatcherEventCounts.create}, change=${includeWatcherEventCounts.change}, delete=${includeWatcherEventCounts.delete}).`,
	);
	refreshStatusBar();
	scheduleIncludeRefresh();
	if (includeConfig.manifestEnabled) {
		scheduleIncludeManifestBuild("watcherEvent");
	}
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
	includeManifestLastError = undefined;
	if (includeManifestBuildInProgress) {
		includeManifestRebuildQueued = true;
		includeManifestStatus = "building";
		refreshStatusBar();
		return;
	}
	includeManifestStatus = "pending";
	includeUploadStatus = "pending";
	includeUploadLastError = undefined;
	includeUploadBundleId = undefined;
	includeUploadRevisionId = undefined;
	includeUploadLastCompletedAt = undefined;
	includeUploadLatencyMs = undefined;
	includeUploadRetryCount = 0;
	refreshStatusBar();
	if (includeManifestTimer) {
		clearTimeout(includeManifestTimer);
	}
	const delay = Math.max(0, previewConfig.debounceMs);
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
		const uris = await vscode.workspace.findFiles(INCLUDE_ASSET_GLOB);
		const records: IncludeManifestBuildItem[] = [];
		let totalBytes = 0;
		for (const uri of uris) {
			try {
				const relativePath = getIncludeRelativePath(uri);
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
		includeManifestSnapshot = manifest;
		includeBundlePayload = createIncludeBundlePayload(manifest, records);
		includeManifestRevisionId = revisionId;
		includeManifestEntryCount = records.length;
		includeManifestTotalBytes = totalBytes;
		includeManifestLastGeneratedAt = generatedAt;
		includeManifestStatus = "ready";
		includeUploadStatus = "pending";
		includeUploadRevisionId = revisionId;
		includeUploadBundleId = undefined;
		includeUploadLastError = undefined;
		includeUploadLastCompletedAt = undefined;
		includeUploadLatencyMs = undefined;
		includeUploadRetryCount = 0;
		logIncludeTelemetry(
			`Include manifest ready (${records.length} asset${records.length === 1 ? "" : "s"}, ${totalBytes} byte${totalBytes === 1 ? "" : "s"}, revision ${revisionId}, ${Date.now() - startedAt} ms).`,
		);
		if (includeBundlePayload) {
			logIncludeTelemetry(
				`Include bundle prepared (${includeBundlePayload.byteLength} byte${includeBundlePayload.byteLength === 1 ? "" : "s"}) for revision ${revisionId}.`,
			);
			scheduleIncludeBundleUpload("manifestReady");
		}
	} catch (error) {
		includeManifestStatus = "error";
		includeManifestLastError = formatError(error);
		logIncludeTelemetry(`Include manifest rebuild failed: ${includeManifestLastError}`);
		includeManifestSnapshot = undefined;
		includeBundlePayload = undefined;
		includeUploadStatus = "error";
		includeUploadBundleId = undefined;
		includeUploadRevisionId = undefined;
		includeUploadLastError = includeManifestLastError;
		includeUploadLastCompletedAt = undefined;
		includeUploadLatencyMs = undefined;
		includeUploadInProgress = false;
		includeUploadQueued = false;
		cancelIncludeUploadScheduling();
	}
	includeManifestBuildInProgress = false;
	refreshStatusBar();
	if (includeManifestRebuildQueued) {
		includeManifestRebuildQueued = false;
		scheduleIncludeManifestBuild("chained");
	}
}

function scheduleIncludeBundleUpload(reason: IncludeUploadScheduleReason): void {
	if (!extensionContext || !includeConfig.watchersEnabled || !includeConfig.manifestEnabled) {
		return;
	}
	if (!includeManifestSnapshot || !includeBundlePayload || !includeManifestRevisionId) {
		logIncludeTelemetry("Include bundle upload skipped: manifest snapshot unavailable.");
		return;
	}
	if (includeUploadInProgress) {
		includeUploadQueued = true;
		logIncludeTelemetry(`Include bundle upload queued (reason: ${reason}).`);
		includeUploadStatus = "uploading";
		refreshStatusBar();
		return;
	}
	includeUploadStatus = "pending";
	includeUploadQueued = false;
	if (includeUploadTimer) {
		clearTimeout(includeUploadTimer);
	}
	includeUploadTimer = setTimeout(
		() => {
			includeUploadTimer = undefined;
			void performIncludeBundleUpload(reason);
		},
		Math.max(0, previewConfig.debounceMs),
	);
	logIncludeTelemetry(`Include bundle upload scheduled (reason: ${reason}, revision: ${includeManifestRevisionId}).`);
	refreshStatusBar();
}

async function performIncludeBundleUpload(reason: IncludeUploadScheduleReason): Promise<void> {
	if (!extensionContext || !includeConfig.watchersEnabled || !includeConfig.manifestEnabled) {
		return;
	}
	if (!includeManifestSnapshot || !includeBundlePayload || !includeManifestRevisionId) {
		return;
	}
	const baseUrl = includeConfig.serviceBaseUrl;
	if (!baseUrl) {
		includeUploadStatus = "disabled";
		includeUploadLastError = "Configure ivgfiddle.includes.serviceBaseUrl to enable uploads.";
		includeUploadBundleId = undefined;
		includeUploadLastCompletedAt = undefined;
		includeUploadLatencyMs = undefined;
		logIncludeTelemetry("Include bundle upload disabled — serviceBaseUrl not configured.");
		queueIncludeBundleMessage({ type: "setIncludeBundle", revision: includeManifestRevisionId, manifest: includeManifestSnapshot });
		refreshStatusBar();
		return;
	}
	includeUploadInProgress = true;
	includeUploadQueued = false;
	includeUploadStatus = "uploading";
	includeUploadLastError = undefined;
	refreshStatusBar();
	const startedAt = Date.now();
	logIncludeTelemetry(`Include bundle upload started (reason: ${reason}, revision: ${includeManifestRevisionId}).`);
	try {
		const target = await requestIncludeUploadTarget(
			baseUrl,
			includeManifestSnapshot,
			includeBundlePayload.byteLength,
			includeConfig.serviceAuthToken,
		);
		if (target.uploadUrl) {
			await uploadIncludeBundle(target, includeBundlePayload);
		}
		includeUploadStatus = "ready";
		includeUploadBundleId = target.bundleId;
		includeUploadRevisionId = includeManifestRevisionId;
		includeUploadLastCompletedAt = new Date().toISOString();
		includeUploadLatencyMs = Date.now() - startedAt;
		includeUploadRetryCount = 0;
		logIncludeTelemetry(`Include bundle upload complete (bundle ${target.bundleId}, ${includeUploadLatencyMs} ms).`);
		queueIncludeBundleMessage({
			type: "setIncludeBundle",
			revision: includeManifestRevisionId,
			manifest: includeManifestSnapshot,
			uploadedAt: includeUploadLastCompletedAt,
			bundle: {
				id: target.bundleId,
				downloadUrl: target.downloadUrl ?? target.uploadUrl,
				expiresAt: target.expiresAt,
				latencyMs: includeUploadLatencyMs,
			},
		});
	} catch (error) {
		includeUploadStatus = "error";
		includeUploadBundleId = undefined;
		includeUploadLastError = formatError(error);
		includeUploadLatencyMs = Date.now() - startedAt;
		includeUploadRetryCount += 1;
		logIncludeTelemetry(`Include bundle upload failed: ${includeUploadLastError}`);
	}
	includeUploadInProgress = false;
	refreshStatusBar();
	if (includeUploadQueued) {
		includeUploadQueued = false;
		scheduleIncludeBundleUpload("retry");
	}
}

async function requestIncludeUploadTarget(
	baseUrl: string,
	manifest: IncludeManifest,
	payloadBytes: number,
	authToken?: string,
): Promise<IncludeUploadTarget> {
	let targetUrl: URL;
	try {
		targetUrl = new URL(INCLUDE_SERVICE_PRESIGN_PATH, baseUrl);
	} catch (error) {
		throw new Error(`Invalid include service base URL: ${formatError(error)}`);
	}
	const headers: Record<string, string> = { "Content-Type": "application/json" };
	if (authToken) {
		headers.Authorization = authToken;
	}
	const response = await fetch(targetUrl.toString(), {
		method: "POST",
		headers,
		body: JSON.stringify({
			revision: manifest.revision,
			manifestVersion: manifest.version,
			generatedAt: manifest.generatedAt,
			entryCount: manifest.entries.length,
			totalBytes: payloadBytes,
		}),
	});
	if (!response.ok) {
		const responseText = await response.text().catch(() => "");
		throw new Error(
			`Include service responded with ${response.status} ${response.statusText}${responseText ? `: ${responseText}` : ""}`,
		);
	}
	const raw = await response.json();
	return normalizeIncludeUploadTarget(raw);
}

function normalizeIncludeUploadTarget(raw: unknown): IncludeUploadTarget {
	if (!raw || typeof raw !== "object") {
		throw new Error("Include service response missing target details.");
	}
	const payload = raw as Record<string, unknown>;
	const bundleIdValue = payload["bundleId"];
	const bundleId = typeof bundleIdValue === "string" && bundleIdValue.trim().length > 0 ? bundleIdValue.trim() : undefined;
	if (!bundleId) {
		throw new Error("Include service response missing bundleId.");
	}
	const uploadUrlValue = payload["uploadUrl"];
	const downloadUrlValue = payload["downloadUrl"];
	const methodValue = payload["method"];
	const headersValue = payload["headers"];
	const expiresAtValue = payload["expiresAt"];
	const uploadUrl = typeof uploadUrlValue === "string" && uploadUrlValue.trim().length > 0 ? uploadUrlValue : undefined;
	const downloadUrl = typeof downloadUrlValue === "string" && downloadUrlValue.trim().length > 0 ? downloadUrlValue : undefined;
	const method = typeof methodValue === "string" && methodValue.trim().length > 0 ? methodValue.toUpperCase() : undefined;
	let headers: Record<string, string> | undefined;
	if (headersValue && typeof headersValue === "object") {
		headers = {};
		for (const [key, value] of Object.entries(headersValue as Record<string, unknown>)) {
			if (typeof value === "string") {
				headers[key] = value;
			}
		}
		if (Object.keys(headers).length === 0) {
			headers = undefined;
		}
	}
	const expiresAt = typeof expiresAtValue === "string" && expiresAtValue.trim().length > 0 ? expiresAtValue : undefined;
	return {
		bundleId,
		uploadUrl,
		downloadUrl,
		method,
		headers,
		expiresAt,
	};
}

async function uploadIncludeBundle(target: IncludeUploadTarget, payload: Uint8Array): Promise<void> {
	if (!target.uploadUrl) {
		return;
	}
	const method = target.method && target.method.trim().length > 0 ? target.method : INCLUDE_SERVICE_DEFAULT_METHOD;
	const headers: Record<string, string> = target.headers ? { ...target.headers } : {};
	if (!headers["Content-Type"]) {
		headers["Content-Type"] = "application/json";
	}
	const response = await fetch(target.uploadUrl, {
		method,
		headers,
		body: Buffer.from(payload),
	});
	if (!response.ok) {
		const responseText = await response.text().catch(() => "");
		throw new Error(
			`Failed to upload include bundle (${response.status} ${response.statusText}${responseText ? `: ${responseText}` : ""})`,
		);
	}
}

function getIncludeRelativePath(uri: vscode.Uri): string {
	const relative = vscode.workspace.asRelativePath(uri, false);
	const candidate = relative && relative !== uri.fsPath ? relative : uri.path;
	const normalized = candidate.replace(/\\/g, "/").replace(/^\.\//, "");
	return normalized.startsWith("/") ? normalized.slice(1) : normalized;
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

function createIncludeBundlePayload(manifest: IncludeManifest, records: IncludeManifestBuildItem[]): Uint8Array {
	const assets: IncludeBundleAssetPayload[] = records.map((record) => ({
		path: record.relativePath,
		checksum: record.entry.checksum,
		mimeType: record.entry.mimeType,
		data: Buffer.from(record.content).toString("base64"),
	}));
	const payload = { manifest, assets };
	return Buffer.from(JSON.stringify(payload), "utf8");
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
			await vscode.workspace.fs.delete(target, { recursive: true, useTrash: false });
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

function cancelIncludeUploadScheduling(): void {
	if (includeUploadTimer) {
		clearTimeout(includeUploadTimer);
		includeUploadTimer = undefined;
	}
	includeUploadQueued = false;
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
			appendTraceOutputLine(payload.text);
		}
		return;
	}
	if (action === "replace" && typeof payload.text === "string") {
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
