"use strict";

const MAX_LOG_SIZE = 64 * 1024;
const MAX_LOG_LINES = 1000;
const MAX_VECTOR_RASTER_DIMENSION = 8192;
const MAX_VECTOR_RASTER_PIXELS = MAX_VECTOR_RASTER_DIMENSION * MAX_VECTOR_RASTER_DIMENSION;
const VECTOR_MEMORY_RESERVE_BYTES = 12 * 1024 * 1024;

function readHeapByteLength() {
	if (typeof Module !== "object" || Module === null) {
		return 0;
	}
	const heaps = [];
	if (Module.HEAPU8 && Module.HEAPU8.buffer) {
		heaps.push(Module.HEAPU8.buffer.byteLength);
	}
	if (Module.HEAP8 && Module.HEAP8.buffer) {
		heaps.push(Module.HEAP8.buffer.byteLength);
	}
	if (Module.wasmMemory && Module.wasmMemory.buffer) {
		heaps.push(Module.wasmMemory.buffer.byteLength);
	}
	if (heaps.length === 0) {
		return 0;
	}
	let maxBytes = 0;
	for (let index = 0; index < heaps.length; ++index) {
		const bytes = heaps[index];
		if (Number.isFinite(bytes) && bytes > maxBytes) {
			maxBytes = bytes;
		}
	}
	return maxBytes;
}

function readFreeHeapByteLength() {
	if (typeof Module !== "object" || Module === null) {
		return 0;
	}
	if (typeof Module._getFreeHeapBytes === "function") {
		try {
			const freeBytes = Module._getFreeHeapBytes();
			if (Number.isFinite(freeBytes) && freeBytes > 0) {
				return freeBytes;
			}
		} catch (error) {
			// Ignore errors – fall back to coarse heap size estimates below.
		}
	}
	return 0;
}

function estimateVectorPixelBudget() {
	const freeHeapBytes = readFreeHeapByteLength();
	if (Number.isFinite(freeHeapBytes) && freeHeapBytes > VECTOR_MEMORY_RESERVE_BYTES) {
		const availableFreeBytes = freeHeapBytes - VECTOR_MEMORY_RESERVE_BYTES;
		if (availableFreeBytes > 0) {
			return Math.floor(availableFreeBytes / 4);
		}
		return 0;
	}
	const heapBytes = readHeapByteLength();
	if (!Number.isFinite(heapBytes) || heapBytes <= VECTOR_MEMORY_RESERVE_BYTES) {
		return 0;
	}
	const availableBytes = heapBytes - VECTOR_MEMORY_RESERVE_BYTES;
	if (availableBytes <= 0) {
		return 0;
	}
	return Math.floor(availableBytes / 4);
}

function formatByteSize(bytes) {
	if (!Number.isFinite(bytes) || bytes <= 0) {
		return "";
	}
	const megabytes = bytes / (1024 * 1024);
	if (megabytes >= 1) {
		if (megabytes >= 100) {
			return Math.round(megabytes) + " MB";
		}
		return megabytes.toFixed(1) + " MB";
	}
	const kilobytes = bytes / 1024;
	if (kilobytes >= 1) {
		return kilobytes.toFixed(1) + " KB";
	}
	return Math.round(bytes) + " bytes";
}

function computeSourceSignature(source) {
	if (typeof source !== "string") {
		return "0:0";
	}
	let hash = 0;
	for (let index = 0; index < source.length; ++index) {
		hash = (hash * 31 + source.charCodeAt(index)) | 0;
	}
	return source.length + ":" + (hash >>> 0);
}

function withElement(element, callback) {
	if (element !== null) {
		callback(element);
	}
}

function toggleClass(element, className, shouldHave) {
	if (element === null) {
		return;
	}
	if (shouldHave) {
		element.classList.add(className);
	} else {
		element.classList.remove(className);
	}
}

const Settings = (function createSettingsAdapter() {
		function read(key, fallback) {
				try {
						const value = localStorage.getItem(key);
						return value === null ? fallback : value;
		} catch (error) {
			return fallback;
		}
	}

	function write(key, value) {
		try {
			if (value === null) {
				localStorage.removeItem(key);
			} else {
				localStorage.setItem(key, value);
			}
		} catch (error) {
			// Ignore storage failures; they are non-fatal for the fiddle UI.
		}
	}

		return {
				read: read,
				write: write,
		};
})();

const leftPanelElement = document.getElementById("leftPanel");
const leftRightSplitElement = document.getElementById("leftRightSplit");
const traceElement = document.getElementById("trace");
const traceDiv = document.getElementById("traceDiv");
const canvasToolbarElement = document.getElementById("canvasToolbar");
const rightPanelElement = document.getElementById("rightPanel");
const screenElement = document.getElementById("screen");
const zoomOutButton = document.getElementById("zoomOutButton");
const zoomInButton = document.getElementById("zoomInButton");
const zoomResetButton = document.getElementById("zoomResetButton");
const vectorScalingToggle = document.getElementById("vectorScalingToggle");
const zoomLevelSelect = document.getElementById("zoomLevelSelect");
const backgroundButton = document.getElementById("backgroundButton");
const backgroundOverlay = document.getElementById("backgroundOverlay");
const backgroundDialog = document.getElementById("backgroundDialog");
const backgroundCloseButton = document.getElementById("backgroundCloseButton");
const backgroundSwatchContainer = document.getElementById("backgroundSwatchContainer");
const snapshotToolbarGroup = document.getElementById("snapshotToolbarGroup");
const snapshotScenarioSelect = document.getElementById("snapshotScenarioSelect");
const ivgCanvas = document.getElementById("ivgCanvas");
const ivgContext = ivgCanvas.getContext("2d");
const MIN_LEFT_PANEL_WIDTH = 250;
const heapTextDecoder = typeof TextDecoder !== "undefined" ? new TextDecoder("utf-8") : null;

function readUtf8FromHeap(offset, byteLength) {
	if (!Module || !Module.HEAPU8 || !Number.isInteger(offset) || !Number.isInteger(byteLength) || byteLength <= 0) {
		return "";
	}
	if (typeof Module.UTF8ArrayToString === "function") {
		return Module.UTF8ArrayToString(Module.HEAPU8, offset, byteLength);
	}
	if (typeof UTF8ArrayToString === "function") {
		return UTF8ArrayToString(Module.HEAPU8, offset, byteLength);
	}
	const heap = Module.HEAPU8;
	const end = offset + byteLength;
	if (heapTextDecoder && typeof heap.subarray === "function") {
		return heapTextDecoder.decode(heap.subarray(offset, end));
	}
	let result = "";
	let index = offset;
	while (index < end) {
		let u0 = heap[index++];
		if (u0 === 0) {
			break;
		}
		if ((u0 & 0x80) === 0) {
			result += String.fromCharCode(u0);
			continue;
		}
		if ((u0 & 0xe0) === 0xc0 && index < end) {
			const u1 = heap[index++] & 0x3f;
			result += String.fromCharCode(((u0 & 0x1f) << 6) | u1);
			continue;
		}
		if ((u0 & 0xf0) === 0xe0 && index + 1 < end) {
			const u1 = heap[index++] & 0x3f;
			const u2 = heap[index++] & 0x3f;
			result += String.fromCharCode(((u0 & 0x0f) << 12) | (u1 << 6) | u2);
			continue;
		}
		if (index + 2 < end) {
			const u1 = heap[index++] & 0x3f;
			const u2 = heap[index++] & 0x3f;
			const u3 = heap[index++] & 0x3f;
			let codePoint = ((u0 & 0x07) << 18) | (u1 << 12) | (u2 << 6) | u3;
			codePoint -= 0x10000;
			result += String.fromCharCode(0xd800 | (codePoint >> 10), 0xdc00 | (codePoint & 0x3ff));
		}
	}
	return result;
}

const STORAGE_KEYS = Object.freeze({
	SOURCE: "ivgSource",
	RUN_ON_STARTUP: "runOnStartup",
	ZOOM_LEVEL: "ivgZoomLevel",
	BACKGROUND_COLOR: "ivgBackgroundColor",
	VECTOR_SCALING: "ivgVectorScaling",
	SNAPSHOT_SELECTION: "ivgSnapshotSelection",
});

const SnapshotController = (function createSnapshotController() {
	let catalog = null;
	let defaultSelection = null;
	let activeSelection = null;
	let activeSourceSignature = "";
	const catalogCache = new Map();
	const selectionCache = new Map();
	let persistedKey = Settings.read(STORAGE_KEYS.SNAPSHOT_SELECTION, "");

	function selectionKey(selection) {
		if (!selection) {
			return "";
		}
		return String(selection.scenarioIndex) + ":" + String(selection.entryOrdinal);
	}

	function selectionFromKey(key) {
		if (typeof key !== "string" || key.length === 0) {
			return null;
		}
		const parts = key.split(":");
		if (parts.length !== 2) {
			return null;
		}
		const scenarioIndex = parseInt(parts[0], 10);
		const entryOrdinal = parseInt(parts[1], 10);
		if (!Number.isInteger(scenarioIndex) || !Number.isInteger(entryOrdinal)) {
			return null;
		}
		return { scenarioIndex: scenarioIndex, entryOrdinal: entryOrdinal };
	}

	function parseCatalog(jsonText) {
		if (typeof jsonText !== "string" || jsonText.length === 0) {
			return null;
		}
		try {
			const parsed = JSON.parse(jsonText);
			if (!parsed || typeof parsed !== "object") {
				return null;
			}
			return parsed;
		} catch (error) {
			return null;
		}
	}

	function buildOptionLabel(scenario, entry) {
		const scenarioName = typeof scenario.name === "string" && scenario.name.length > 0 ? scenario.name : "Scenario " + scenario.index;
		if (scenario.explicit === false && (!scenario.entries || scenario.entries.length <= 1)) {
			return scenarioName;
		}
		const listIndex = Number.isInteger(entry.listIndex) ? entry.listIndex : entry.entryOrdinal - 1;
		return scenarioName + " #" + String(listIndex);
	}

	function selectionsEqual(a, b) {
		if (!a || !b) {
			return false;
		}
		return a.scenarioIndex === b.scenarioIndex && a.entryOrdinal === b.entryOrdinal;
	}

	function buildOptions(parsedCatalog) {
		const options = [];
		if (!parsedCatalog || !Array.isArray(parsedCatalog.scenarios)) {
			return options;
		}
		for (let i = 0; i < parsedCatalog.scenarios.length; ++i) {
			const scenario = parsedCatalog.scenarios[i];
			if (!scenario || !Array.isArray(scenario.entries)) {
				continue;
			}
			for (let j = 0; j < scenario.entries.length; ++j) {
				const entry = scenario.entries[j];
				if (!entry) {
					continue;
				}
				options.push({
					value: String(scenario.index) + ":" + String(entry.entryOrdinal),
					label: buildOptionLabel(scenario, entry),
					scenarioIndex: scenario.index,
					entryOrdinal: entry.entryOrdinal,
				});
			}
		}
		return options;
	}

	function selectionExists(options, selection) {
		if (!selection || !options) {
			return false;
		}
		for (let i = 0; i < options.length; ++i) {
			if (options[i].scenarioIndex === selection.scenarioIndex && options[i].entryOrdinal === selection.entryOrdinal) {
				return true;
			}
		}
		return false;
	}

	function applyPersistedSelection(options) {
		if (!persistedKey || options.length === 0) {
			return null;
		}
		const persisted = selectionFromKey(persistedKey);
		if (persisted === null) {
			return null;
		}
		for (let i = 0; i < options.length; ++i) {
			if (options[i].scenarioIndex === persisted.scenarioIndex && options[i].entryOrdinal === persisted.entryOrdinal) {
				return persisted;
			}
		}
		return null;
	}

	function updateToolbar(options) {
		if (!snapshotToolbarGroup || !snapshotScenarioSelect) {
			return;
		}
		if (!options || options.length === 0) {
			snapshotToolbarGroup.classList.add("is-hidden");
			snapshotScenarioSelect.disabled = true;
			snapshotScenarioSelect.innerHTML = "";
			return;
		}
		snapshotToolbarGroup.classList.remove("is-hidden");
		snapshotScenarioSelect.disabled = false;
		snapshotScenarioSelect.innerHTML = "";
		for (let i = 0; i < options.length; ++i) {
			const option = document.createElement("option");
			option.value = options[i].value;
			option.textContent = options[i].label;
			snapshotScenarioSelect.appendChild(option);
		}
		const key = selectionKey(activeSelection);
		if (key !== "") {
			snapshotScenarioSelect.value = key;
		} else if (options.length > 0) {
			snapshotScenarioSelect.value = options[0].value;
		}
	}

	function applyRenderResult(params) {
		const parsed = parseCatalog(params.catalogJson);
		catalog = parsed;
		if (activeSourceSignature) {
			catalogCache.set(activeSourceSignature, parsed);
		}
		const executedSelection = Number.isInteger(params.defaultScenarioIndex) && Number.isInteger(params.defaultEntryOrdinal) && params.defaultScenarioIndex >= 0 && params.defaultEntryOrdinal >= 0
			? {
				scenarioIndex: params.defaultScenarioIndex >>> 0,
				entryOrdinal: params.defaultEntryOrdinal >>> 0,
			}
			: null;
		const catalogDefault = parsed && Number.isInteger(parsed.defaultScenarioIndex) && Number.isInteger(parsed.defaultEntryOrdinal) && parsed.defaultScenarioIndex >= 0 && parsed.defaultEntryOrdinal >= 0
			? {
				scenarioIndex: parsed.defaultScenarioIndex >>> 0,
				entryOrdinal: parsed.defaultEntryOrdinal >>> 0,
			}
			: null;
		defaultSelection = executedSelection;

		const options = buildOptions(parsed);
		if (!selectionExists(options, activeSelection)) {
			activeSelection = null;
		}

		let nextSelection = activeSelection;
		if (!nextSelection && options.length > 0) {
			const persisted = applyPersistedSelection(options);
			if (persisted) {
				nextSelection = persisted;
			} else if (executedSelection && selectionExists(options, executedSelection)) {
				nextSelection = executedSelection;
			} else if (catalogDefault && selectionExists(options, catalogDefault)) {
				nextSelection = catalogDefault;
			}
		}
		activeSelection = nextSelection;
		if (!activeSelection && activeSourceSignature) {
			const cachedSelection = selectionCache.get(activeSourceSignature) || null;
			if (cachedSelection && selectionExists(options, cachedSelection)) {
				activeSelection = cachedSelection;
			}
		}
		persistedKey = selectionKey(activeSelection);
		Settings.write(STORAGE_KEYS.SNAPSHOT_SELECTION, persistedKey);
		if (activeSourceSignature) {
			selectionCache.set(activeSourceSignature, activeSelection);
		}
		const scenarioCount = parsed && Array.isArray(parsed.scenarios) ? parsed.scenarios.length : 0;
		const entriesCount = options.length;
		trace(
			"Snapshot catalog: " +
				scenarioCount +
				" scenario" +
				(scenarioCount === 1 ? "" : "s") +
				", " +
				entriesCount +
				" entr" +
				(entriesCount === 1 ? "y" : "ies"),
		);
		updateToolbar(options);
	}

	function handleSelectionChange(value) {
		const next = selectionFromKey(value);
		if (next === null) {
			return false;
		}
		if (activeSelection && selectionsEqual(activeSelection, next)) {
			return false;
		}
		activeSelection = next;
		const key = selectionKey(activeSelection);
		persistedKey = key;
		Settings.write(STORAGE_KEYS.SNAPSHOT_SELECTION, persistedKey);
		if (activeSourceSignature) {
			selectionCache.set(activeSourceSignature, activeSelection);
		}
		if (
			snapshotScenarioSelect &&
			key !== "" &&
			snapshotScenarioSelect.value !== key
		) {
			snapshotScenarioSelect.value = key;
		}
		trace(
			"Snapshot selection changed to scenario " +
				next.scenarioIndex +
			", entry " +
				next.entryOrdinal,
		);
		return true;
	}

	function prepareForRender(signature, changed) {
		const normalizedSignature = typeof signature === "string" ? signature : "";
		const effectiveChange = changed || activeSourceSignature !== normalizedSignature;
		if (!effectiveChange) {
			return;
		}
		activeSourceSignature = normalizedSignature;
		catalog = catalogCache.get(activeSourceSignature) || null;
		defaultSelection = null;
		activeSelection = selectionCache.get(activeSourceSignature) || null;
		if (activeSelection) {
			trace(
				"Snapshot source updated; reusing cached selection scenario " +
					activeSelection.scenarioIndex +
					", entry " +
					activeSelection.entryOrdinal,
			);
		} else if (activeSourceSignature) {
			trace("Snapshot source updated; awaiting catalog for signature " + activeSourceSignature + ".");
		}
	}

	function getSelectionForRender() {
		return activeSelection;
	}

	updateToolbar([]);
	return {
		applyRenderResult: applyRenderResult,
		handleSelectionChange: handleSelectionChange,
		prepareForRender: prepareForRender,
		getSelectionForRender: getSelectionForRender,
	};
})();

if (snapshotScenarioSelect !== null) {
		snapshotScenarioSelect.addEventListener("change", function handleSnapshotChange() {
				if (SnapshotController.handleSelectionChange(snapshotScenarioSelect.value)) {
						runIVG("snapshot selection changed");
				}
		});
}

let rasterizeInProgress = false;
let rerunQueuedWhileBusy = false;
let rerunQueuedReason = "";
let lastRasterizedSourceSignature = null;

const BACKGROUND_COLORS = Object.freeze([
	{ value: "black", label: "Black", preview: "#000000" },
	{ value: "white", label: "White", preview: "#ffffff" },
	{ value: "gray", label: "Gray", preview: "#808080" },
	{ value: "silver", label: "Silver", preview: "#c0c0c0" },
	{ value: "red", label: "Red", preview: "#ff0000" },
	{ value: "maroon", label: "Maroon", preview: "#800000" },
	{ value: "purple", label: "Purple", preview: "#800080" },
	{ value: "fuchsia", label: "Fuchsia", preview: "#ff00ff" },
	{ value: "blue", label: "Blue", preview: "#0000ff" },
	{ value: "navy", label: "Navy", preview: "#000080" },
	{ value: "aqua", label: "Aqua", preview: "#00ffff" },
	{ value: "teal", label: "Teal", preview: "#008080" },
	{ value: "green", label: "Green", preview: "#008000" },
	{ value: "lime", label: "Lime", preview: "#00ff00" },
	{ value: "olive", label: "Olive", preview: "#808000" },
	{ value: "yellow", label: "Yellow", preview: "#ffff00" },
	{ value: "none", label: "None", preview: "#121212" },
]);

const BACKGROUND_COLOR_MAP = (function buildBackgroundMap() {
	const lookup = Object.create(null);
	for (let index = 0; index < BACKGROUND_COLORS.length; ++index) {
		const entry = BACKGROUND_COLORS[index];
		lookup[entry.value] = entry;
	}
	return lookup;
})();

const BACKGROUND_DEFAULT = "none";

const BackgroundController = (function createBackgroundController() {
	let currentColor = BACKGROUND_DEFAULT;
	let isOpen = false;
	let lastTrigger = null;
	const bodyElement = document.body;
	const defaultBodyBackground = bodyElement ? window.getComputedStyle(bodyElement).backgroundColor : "";

	function getColorDefinition(value) {
		return Object.prototype.hasOwnProperty.call(BACKGROUND_COLOR_MAP, value) ? BACKGROUND_COLOR_MAP[value] : null;
	}

	function normalizeColor(value) {
		if (value === "transparent") {
			return "none";
		}
		const definition = getColorDefinition(value);
		if (definition === null) {
			return BACKGROUND_DEFAULT;
		}
		return definition.value;
	}

	function parseHexColor(value) {
		if (typeof value !== "string") {
			return null;
		}
		const hex = value.trim().toLowerCase();
		if (!hex.startsWith("#")) {
			return null;
		}
		if (hex.length === 4) {
			const r = parseInt(hex.charAt(1) + hex.charAt(1), 16);
			const g = parseInt(hex.charAt(2) + hex.charAt(2), 16);
			const b = parseInt(hex.charAt(3) + hex.charAt(3), 16);
			if (Number.isNaN(r) || Number.isNaN(g) || Number.isNaN(b)) {
				return null;
			}
			return { r: r, g: g, b: b };
		}
		if (hex.length !== 7) {
			return null;
		}
		const r = parseInt(hex.substr(1, 2), 16);
		const g = parseInt(hex.substr(3, 2), 16);
		const b = parseInt(hex.substr(5, 2), 16);
		if (Number.isNaN(r) || Number.isNaN(g) || Number.isNaN(b)) {
			return null;
		}
		return { r: r, g: g, b: b };
	}

	function formatHexComponent(value) {
		const clamped = Math.max(0, Math.min(255, value));
		const hex = clamped.toString(16);
		if (hex.length < 2) {
			return "0" + hex;
		}
		return hex;
	}

	function adjustOuterShade(definition) {
		if (!definition || !definition.preview) {
			return "";
		}
		const components = parseHexColor(definition.preview);
		if (components === null) {
			return definition.preview;
		}
		const luminance = (0.2126 * components.r + 0.7152 * components.g + 0.0722 * components.b) / 255;
		const lighten = luminance < 0.5;
		const factor = 0.18;
		function adjustComponent(component) {
			if (lighten) {
				return Math.round(component + (255 - component) * factor);
			}
			return Math.round(component * (1 - factor));
		}
		const adjusted = {
			r: adjustComponent(components.r),
			g: adjustComponent(components.g),
			b: adjustComponent(components.b),
		};
		return "#" + formatHexComponent(adjusted.r) + formatHexComponent(adjusted.g) + formatHexComponent(adjusted.b);
	}

	function computeOuterBackground(definition) {
		if (!definition || definition.value === "none") {
			return "";
		}
		return adjustOuterShade(definition);
	}

	function getSwatchButtons() {
		if (backgroundSwatchContainer === null) {
			return [];
		}
		const nodeList = backgroundSwatchContainer.querySelectorAll(".background-swatch");
		return Array.prototype.slice.call(nodeList);
	}

	function createSwatches() {
		if (backgroundSwatchContainer === null) {
			return;
		}
		backgroundSwatchContainer.innerHTML = "";
		const fragment = document.createDocumentFragment();
		for (let index = 0; index < BACKGROUND_COLORS.length; ++index) {
			const color = BACKGROUND_COLORS[index];
			const swatchButton = document.createElement("button");
			swatchButton.type = "button";
			swatchButton.className = "background-swatch";
			swatchButton.setAttribute("role", "option");
			swatchButton.setAttribute("aria-selected", "false");
			swatchButton.setAttribute("data-background", color.value);
			if (color.value !== "none") {
				swatchButton.style.setProperty("--swatch-color", color.preview);
			}
			const preview = document.createElement("span");
			preview.className = "background-swatch__preview";
			if (color.value === "none") {
				preview.classList.add("background-swatch__preview--transparent");
			}
			const label = document.createElement("span");
			label.className = "background-swatch__label";
			label.textContent = color.label;
			swatchButton.appendChild(preview);
			swatchButton.appendChild(label);
			fragment.appendChild(swatchButton);
		}
		backgroundSwatchContainer.appendChild(fragment);
	}

	function updateSelectionUI() {
		const buttons = getSwatchButtons();
		for (let index = 0; index < buttons.length; ++index) {
			const button = buttons[index];
			const value = button.getAttribute("data-background");
			if (value === currentColor) {
				button.setAttribute("data-selected", "true");
				button.setAttribute("aria-selected", "true");
			} else {
				button.removeAttribute("data-selected");
				button.setAttribute("aria-selected", "false");
			}
		}
	}

	function updateTriggerLabel() {
		withElement(backgroundButton, function updateButtonLabel(button) {
			const definition = getColorDefinition(currentColor);
			const labelText = definition ? definition.label : currentColor;
			button.setAttribute("aria-label", "Change canvas background (current: " + labelText + ")");
		});
	}

	function applyColorToDOM() {
		const shouldClearColor = currentColor === "none";
		const definition = getColorDefinition(currentColor);
		const paletteColor = definition && definition.preview ? definition.preview : currentColor;
		const outerColor = computeOuterBackground(definition) || paletteColor;
		withElement(rightPanelElement, function updateRightPanel(element) {
			toggleClass(element, "transparent", shouldClearColor);
			element.style.backgroundColor = shouldClearColor ? "" : outerColor;
		});
		withElement(screenElement, function updateScreen(element) {
			toggleClass(element, "transparent", shouldClearColor);
			element.style.backgroundColor = shouldClearColor ? "" : outerColor;
		});
		withElement(ivgCanvas, function updateCanvas(canvas) {
			if (shouldClearColor) {
				canvas.style.removeProperty("background-color");
				canvas.style.removeProperty("background-image");
			} else {
				canvas.style.backgroundColor = paletteColor;
				canvas.style.backgroundImage = "none";
			}
		});
		withElement(bodyElement, function updateBody(element) {
			element.style.backgroundColor = shouldClearColor ? defaultBodyBackground : outerColor;
		});
	}

	function applyColor(value, options) {
		const settings = options || {};
		const normalized = normalizeColor(value);
		if (normalized === currentColor && settings.force !== true) {
			updateSelectionUI();
			updateTriggerLabel();
			return;
		}
		currentColor = normalized;
		applyColorToDOM();
		updateSelectionUI();
		updateTriggerLabel();
		if (!settings.skipPersist) {
			Settings.write(STORAGE_KEYS.BACKGROUND_COLOR, currentColor);
		}
	}

	function focusCurrentSwatch() {
		const buttons = getSwatchButtons();
		for (let index = 0; index < buttons.length; ++index) {
			const button = buttons[index];
			if (button.getAttribute("data-background") === currentColor) {
				button.focus();
				return;
			}
		}
	}

	function focusRelativeSwatch(startButton, delta) {
		const buttons = getSwatchButtons();
		if (buttons.length === 0) {
			return;
		}
		let index = buttons.indexOf(startButton);
		if (index === -1) {
			index = 0;
		}
		index = (index + delta + buttons.length) % buttons.length;
		buttons[index].focus();
	}

	function focusFirstSwatch() {
		const buttons = getSwatchButtons();
		if (buttons.length > 0) {
			buttons[0].focus();
		}
	}

	function focusLastSwatch() {
		const buttons = getSwatchButtons();
		if (buttons.length > 0) {
			buttons[buttons.length - 1].focus();
		}
	}

	function handleSwatchClick(event) {
		const target = event.target;
		if (target === null) {
			return;
		}
		const button = target.closest ? target.closest(".background-swatch") : null;
		if (button === null) {
			return;
		}
		const value = button.getAttribute("data-background");
		if (!value) {
			return;
		}
		event.preventDefault();
		applyColor(value);
		close();
	}

	function handleSwatchKeydown(event) {
		const target = event.target;
		if (target === null || !target.classList || !target.classList.contains("background-swatch")) {
			return;
		}
		const key = event.key;
		if (key === "ArrowRight" || key === "ArrowDown") {
			event.preventDefault();
			focusRelativeSwatch(target, 1);
			return;
		}
		if (key === "ArrowLeft" || key === "ArrowUp") {
			event.preventDefault();
			focusRelativeSwatch(target, -1);
			return;
		}
		if (key === "Home") {
			event.preventDefault();
			focusFirstSwatch();
			return;
		}
		if (key === "End") {
			event.preventDefault();
			focusLastSwatch();
			return;
		}
		if (key === "Enter" || key === " ") {
			event.preventDefault();
			const value = target.getAttribute("data-background");
			if (value) {
				applyColor(value);
				close();
			}
		}
	}

	function handleOverlayKeydown(event) {
		if (!isOpen) {
			return;
		}
		if (event.key === "Escape") {
			event.preventDefault();
			close();
		}
	}

	function handleOverlayClick(event) {
		if (!isOpen || backgroundOverlay === null) {
			return;
		}
		if (event.target === backgroundOverlay) {
			event.preventDefault();
			close();
		}
	}

	function open(trigger) {
		if (backgroundOverlay === null) {
			return;
		}
		if (isOpen) {
			focusCurrentSwatch();
			return;
		}
		isOpen = true;
		lastTrigger = trigger || backgroundButton;
		toggleClass(backgroundOverlay, "is-hidden", false);
		backgroundOverlay.setAttribute("aria-hidden", "false");
		withElement(backgroundButton, function markExpanded(button) {
			button.setAttribute("aria-expanded", "true");
		});
		document.addEventListener("keydown", handleOverlayKeydown, true);
		window.setTimeout(focusCurrentSwatch, 0);
	}

	function close() {
		if (!isOpen || backgroundOverlay === null) {
			return;
		}
		isOpen = false;
		toggleClass(backgroundOverlay, "is-hidden", true);
		backgroundOverlay.setAttribute("aria-hidden", "true");
		withElement(backgroundButton, function markCollapsed(button) {
			button.setAttribute("aria-expanded", "false");
		});
		document.removeEventListener("keydown", handleOverlayKeydown, true);
		if (lastTrigger && typeof lastTrigger.focus === "function") {
			lastTrigger.focus();
		}
		lastTrigger = null;
	}

	function readInitialColor() {
		const stored = Settings.read(STORAGE_KEYS.BACKGROUND_COLOR, null);
		if (stored === null) {
			return BACKGROUND_DEFAULT;
		}
		return normalizeColor(stored);
	}

	function bindEvents() {
		if (backgroundButton !== null) {
			backgroundButton.addEventListener("click", function handleBackgroundButtonClick() {
				open(backgroundButton);
			});
		}
		if (backgroundCloseButton !== null) {
			backgroundCloseButton.addEventListener("click", function handleBackgroundCloseClick() {
				close();
			});
		}
		if (backgroundOverlay !== null) {
			backgroundOverlay.addEventListener("click", handleOverlayClick);
		}
		if (backgroundSwatchContainer !== null) {
			backgroundSwatchContainer.addEventListener("click", handleSwatchClick);
			backgroundSwatchContainer.addEventListener("keydown", handleSwatchKeydown);
		}
	}

	function init() {
		createSwatches();
		const initialColor = readInitialColor();
		applyColor(initialColor, { skipPersist: true, force: true });
		bindEvents();
	}

	return {
		init: init,
		applyColor: applyColor,
		open: open,
		close: close,
	};
})();

const ZOOM_SELECT_PRESETS = Object.freeze([0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10]);

const ZOOM_PRESETS = ZOOM_SELECT_PRESETS;

const CUSTOM_ZOOM_OPTION_VALUE = "custom";

const ZOOM_CONSTANTS = Object.freeze({
	MIN: ZOOM_PRESETS[0],
	MAX: ZOOM_PRESETS[ZOOM_PRESETS.length - 1],
	STEP: 1,
	DEFAULT: 1.0,
});

BackgroundController.init();

const ZoomController = (function createZoomController() {
	let currentZoom = ZOOM_CONSTANTS.DEFAULT;
	let displayMetrics = null;
	let baselineMetrics = null;
	let vectorScalingEnabled = false;
	let vectorScalingPreferred = false;
	let vectorScalingSuppressed = false;
	let lastRenderZoom = 1;
	let lastVectorRenderLimit = Infinity;
	let rerenderRequestPending = false;
	let pendingVectorRerenderReason = "vector rescale update";
	let bitmapFallbackQueued = false;
	let vectorBaselineReady = false;
	const ZOOM_EPSILON = 0.0001;

	function zoomToPercent(value) {
		return Math.round(value * 100);
	}

	function clampZoom(value) {
		const numeric = Number(value);
		if (!Number.isFinite(numeric)) {
			return ZOOM_CONSTANTS.DEFAULT;
		}
		return Math.min(ZOOM_CONSTANTS.MAX, Math.max(ZOOM_CONSTANTS.MIN, numeric));
	}

	function snapToPreset(value, direction) {
		const clamped = Math.min(ZOOM_CONSTANTS.MAX, Math.max(ZOOM_CONSTANTS.MIN, value));
		for (let index = 0; index < ZOOM_PRESETS.length; ++index) {
			const preset = ZOOM_PRESETS[index];
			if (Math.abs(preset - clamped) < ZOOM_EPSILON) {
				return preset;
			}
		}
		if (direction > 0) {
			for (let index = 0; index < ZOOM_PRESETS.length; ++index) {
				if (clamped < ZOOM_PRESETS[index] - ZOOM_EPSILON) {
					return ZOOM_PRESETS[index];
				}
			}
			return ZOOM_PRESETS[ZOOM_PRESETS.length - 1];
		}
		if (direction < 0) {
			for (let index = ZOOM_PRESETS.length - 1; index >= 0; --index) {
				if (clamped > ZOOM_PRESETS[index] + ZOOM_EPSILON) {
					return ZOOM_PRESETS[index];
				}
			}
			return ZOOM_PRESETS[0];
		}
		let closestPreset = ZOOM_PRESETS[0];
		let smallestDelta = Math.abs(clamped - closestPreset);
		for (let index = 1; index < ZOOM_PRESETS.length; ++index) {
			const preset = ZOOM_PRESETS[index];
			const delta = Math.abs(clamped - preset);
			if (delta < smallestDelta - ZOOM_EPSILON) {
				smallestDelta = delta;
				closestPreset = preset;
			}
		}
		return closestPreset;
	}

	function ensureCustomZoomOption() {
		if (zoomLevelSelect === null) {
			return null;
		}
		let customOption = zoomLevelSelect.querySelector('option[data-custom-zoom="true"]');
		if (customOption === null) {
			customOption = document.createElement("option");
			customOption.value = CUSTOM_ZOOM_OPTION_VALUE;
			customOption.setAttribute("data-custom-zoom", "true");
			customOption.hidden = true;
			customOption.disabled = true;
			zoomLevelSelect.appendChild(customOption);
		}
		return customOption;
	}

	function syncZoomSelectValue() {
		if (zoomLevelSelect === null) {
			return;
		}
		const percentString = String(zoomToPercent(currentZoom));
		const matchingOption = zoomLevelSelect.querySelector('option[value="' + percentString + '"]');
		const customOption = ensureCustomZoomOption();
		if (matchingOption !== null) {
			zoomLevelSelect.value = percentString;
			if (customOption !== null) {
				customOption.hidden = true;
				customOption.disabled = true;
				customOption.textContent = "";
			}
			return;
		}
		if (customOption !== null) {
			customOption.hidden = false;
			customOption.disabled = false;
			customOption.textContent = percentString + "%";
			zoomLevelSelect.value = CUSTOM_ZOOM_OPTION_VALUE;
		}
	}

	function populateZoomOptions() {
		if (zoomLevelSelect === null) {
			return;
		}
		zoomLevelSelect.innerHTML = "";
		for (let index = 0; index < ZOOM_SELECT_PRESETS.length; ++index) {
			const preset = ZOOM_SELECT_PRESETS[index];
			const option = document.createElement("option");
			const percent = zoomToPercent(preset);
			option.value = String(percent);
			option.textContent = percent + "%";
			if (Math.abs(preset - ZOOM_CONSTANTS.DEFAULT) < ZOOM_EPSILON) {
				option.defaultSelected = true;
			}
			zoomLevelSelect.appendChild(option);
		}
		ensureCustomZoomOption();
		syncZoomSelectValue();
	}

	function percentToZoom(percentString) {
		if (percentString === CUSTOM_ZOOM_OPTION_VALUE) {
			return currentZoom;
		}
		const percent = Number.parseInt(percentString, 10);
		if (!Number.isFinite(percent)) {
			return currentZoom;
		}
		return clampZoom(percent / 100);
	}

	function reflectVectorScalingState() {
		withElement(vectorScalingToggle, function updateVectorToggle(button) {
			const pressed = vectorScalingEnabled;
			button.setAttribute("aria-pressed", pressed ? "true" : "false");
			const label = pressed ? "Vector zoom" : "Bitmap zoom";
			const nextMode = pressed ? "bitmap" : "vector";
			const actionLabel = "Switch to " + nextMode + " zoom";
			button.textContent = label;
			button.setAttribute("title", actionLabel);
			button.setAttribute("aria-label", actionLabel);
		});
	}

	function reflectUIState() {
		syncZoomSelectValue();
		withElement(zoomOutButton, function disableZoomOut(button) {
			button.disabled = currentZoom <= ZOOM_CONSTANTS.MIN + ZOOM_EPSILON;
		});
		withElement(zoomInButton, function disableZoomIn(button) {
			button.disabled = currentZoom >= ZOOM_CONSTANTS.MAX - ZOOM_EPSILON;
		});
		reflectVectorScalingState();
	}

	function persistZoom() {
		Settings.write(STORAGE_KEYS.ZOOM_LEVEL, currentZoom.toFixed(2));
	}

	function persistVectorScaling() {
		Settings.write(
			STORAGE_KEYS.VECTOR_SCALING,
			vectorScalingPreferred ? "1" : "0"
		);
	}

	function scheduleVectorRerender(reason) {
		if (!vectorScalingEnabled) {
			return;
		}
		pendingVectorRerenderReason = reason || "vector rescale update";
		if (rerenderRequestPending) {
			return;
		}
		rerenderRequestPending = true;
		window.requestAnimationFrame(function dispatchVectorRerender() {
			rerenderRequestPending = false;
			runIVG(pendingVectorRerenderReason);
		});
	}

	function queueBitmapFallback(reason) {
		if (bitmapFallbackQueued) {
			return;
		}
		bitmapFallbackQueued = true;
		const fallbackReason = typeof reason === "string" && reason.length > 0 ? reason : "vector-fallback";
		window.requestAnimationFrame(function dispatchBitmapFallback() {
			bitmapFallbackQueued = false;
			runIVG(fallbackReason);
		});
	}

	function applyZoom() {
		if (ivgCanvas === null) {
			return;
		}
		ivgCanvas.setAttribute("data-scaling-mode", vectorScalingEnabled ? "vector" : "bitmap");
		const metrics = displayMetrics;
		const targetZoom = vectorScalingEnabled ? currentZoom : 1;
		ivgCanvas.style.transformOrigin = "top left";
		if (metrics === null) {
			if (vectorScalingEnabled) {
				ivgCanvas.style.transform = "translate(0px, 0px)";
			} else {
				ivgCanvas.style.transform = "scale(" + currentZoom + ")";
			}
			return;
		}
		ivgCanvas.style.width = metrics.width * targetZoom + "px";
		ivgCanvas.style.height = metrics.height * targetZoom + "px";
		if (vectorScalingEnabled) {
			ivgCanvas.style.transform = "translate(" + metrics.translateX * targetZoom + "px," + metrics.translateY * targetZoom + "px)";
			if (Math.abs(lastRenderZoom - currentZoom) > ZOOM_EPSILON) {
				const limitReached =
					lastVectorRenderLimit !== Infinity &&
					Math.abs(lastRenderZoom - lastVectorRenderLimit) < ZOOM_EPSILON &&
					lastVectorRenderLimit < currentZoom - ZOOM_EPSILON;
				if (!limitReached) {
					scheduleVectorRerender("zoom-change");
				}
			}
		} else {
			ivgCanvas.style.transform = "translate(" + metrics.translateX + "px," + metrics.translateY + "px) scale(" + currentZoom + ")";
		}
	}

	function setZoom(value, options) {
		const settings = options || {};
		const clamped = clampZoom(value);
		const snapped = snapToPreset(clamped, settings.snapDirection || 0);
		if (Math.abs(snapped - currentZoom) < ZOOM_EPSILON) {
			reflectUIState();
			return;
		}
		currentZoom = snapped;
		if (!settings.skipPersist) {
			persistZoom();
		}
		applyZoom();
		reflectUIState();
		if (vectorScalingEnabled && settings.skipVectorRefresh !== true) {
			scheduleVectorRerender("zoom-change");
		}
	}

	function incrementZoom(delta) {
		const step = Number(delta);
		if (!Number.isFinite(step) || Math.abs(step) < ZOOM_EPSILON) {
			reflectUIState();
			return;
		}
		const direction = step > 0 ? 1 : -1;
		let index = -1;
		for (let i = 0; i < ZOOM_PRESETS.length; ++i) {
			if (Math.abs(ZOOM_PRESETS[i] - currentZoom) < ZOOM_EPSILON) {
				index = i;
				break;
			}
		}
		if (index === -1) {
			currentZoom = snapToPreset(currentZoom, 0);
			for (let i = 0; i < ZOOM_PRESETS.length; ++i) {
				if (Math.abs(ZOOM_PRESETS[i] - currentZoom) < ZOOM_EPSILON) {
					index = i;
					break;
				}
			}
		}
		if (index === -1) {
			reflectUIState();
			return;
		}
		if (direction > 0 && index < ZOOM_PRESETS.length - 1) {
			setZoom(ZOOM_PRESETS[index + 1], { snapDirection: direction });
			return;
		}
		if (direction < 0 && index > 0) {
			setZoom(ZOOM_PRESETS[index - 1], { snapDirection: direction });
			return;
		}
		setZoom(ZOOM_PRESETS[index], { snapDirection: direction });
	}

	function resetZoom() {
		setZoom(ZOOM_CONSTANTS.DEFAULT);
	}

	function setVectorScalingEnabled(value, options) {
		const settings = options || {};
		const normalized = value === true;
		if (settings.skipPreferenceUpdate !== true) {
			vectorScalingPreferred = normalized;
		}
		if (normalized === vectorScalingEnabled && settings.force !== true) {
			if (normalized) {
				vectorScalingSuppressed = false;
			} else if (settings.suppressPreference === true) {
				vectorScalingSuppressed = true;
			} else if (settings.skipPreferenceUpdate !== true) {
				vectorScalingSuppressed = false;
			}
			reflectVectorScalingState();
			return;
		}
		vectorScalingEnabled = normalized;
		if (normalized) {
			vectorScalingSuppressed = false;
		} else if (settings.suppressPreference === true) {
			vectorScalingSuppressed = true;
		} else if (settings.skipPreferenceUpdate !== true) {
			vectorScalingSuppressed = false;
		}
		if (!settings.skipPersist) {
			persistVectorScaling();
		}
		reflectVectorScalingState();
		applyZoom();
		if (settings.skipRerender === true) {
			return;
		}
		if (vectorScalingEnabled) {
			scheduleVectorRerender("vector-toggle");
		} else {
			window.requestAnimationFrame(function queueBitmapReraster() {
				runIVG("vector-toggle-disabled");
			});
		}
	}

	function restorePreferredVectorScaling() {
		if (!vectorScalingPreferred) {
			vectorScalingSuppressed = false;
			return false;
		}
		if (!vectorScalingSuppressed || vectorScalingEnabled) {
			return false;
		}
		setVectorScalingEnabled(true, {
			skipPreferenceUpdate: true,
		});
		return true;
	}

	function bindUIEvents() {
		if (zoomOutButton !== null) {
			zoomOutButton.addEventListener("click", function handleZoomOutClick() {
				incrementZoom(-ZOOM_CONSTANTS.STEP);
			});
		}
		if (zoomInButton !== null) {
			zoomInButton.addEventListener("click", function handleZoomInClick() {
				incrementZoom(ZOOM_CONSTANTS.STEP);
			});
		}
		if (zoomResetButton !== null) {
			zoomResetButton.addEventListener("click", function handleZoomResetClick() {
				resetZoom();
			});
		}
		if (zoomLevelSelect !== null) {
			zoomLevelSelect.addEventListener("change", function handleZoomSelectChange(event) {
				const target = event.target;
				setZoom(percentToZoom(target.value));
			});
		}
		if (vectorScalingToggle !== null) {
			vectorScalingToggle.addEventListener("click", function handleVectorScalingToggleClick() {
				setVectorScalingEnabled(!vectorScalingEnabled);
			});
		}
		document.addEventListener("keydown", handleZoomShortcut, true);
	}

	function handleVectorRasterFailure(details) {
		if (details && Number.isFinite(details.vectorRenderLimit)) {
			lastVectorRenderLimit = details.vectorRenderLimit;
		}
		if (details && Number.isFinite(details.renderZoom)) {
			lastRenderZoom = details.renderZoom;
		}
		invalidateBaseMetrics();
		return false;
	}

	function targetBlocksShortcut(element) {
		if (element === null) {
			return false;
		}
		if (element.closest && element.closest("#editor") !== null) {
			return true;
		}
		const tagName = element.tagName;
		if (tagName === "INPUT" || tagName === "TEXTAREA" || tagName === "SELECT") {
			return true;
		}
		if (element.isContentEditable === true) {
			return true;
		}
		return false;
	}

	function handleZoomShortcut(event) {
		if (event.defaultPrevented) {
			return;
		}
		if (!(event.ctrlKey || event.metaKey) || event.altKey) {
			return;
		}
		const activeElement = document.activeElement;
		if (targetBlocksShortcut(activeElement)) {
			return;
		}
		const key = event.key;
		if (key === "+" || key === "=") {
			event.preventDefault();
			incrementZoom(ZOOM_CONSTANTS.STEP);
			return;
		}
		if (key === "-") {
			event.preventDefault();
			incrementZoom(-ZOOM_CONSTANTS.STEP);
			return;
		}
		if (key === "0") {
			event.preventDefault();
			resetZoom();
		}
	}

	function readInitialZoom() {
		const stored = Settings.read(STORAGE_KEYS.ZOOM_LEVEL, null);
		if (stored === null) {
			return;
		}
		const parsed = Number.parseFloat(stored);
		if (!Number.isFinite(parsed)) {
			return;
		}
		currentZoom = snapToPreset(parsed, 0);
	}

	function readInitialVectorScaling() {
		const stored = Settings.read(STORAGE_KEYS.VECTOR_SCALING, null);
		if (stored === null) {
			return;
		}
		setVectorScalingEnabled(stored === "1" || stored === "true", {
			skipPersist: true,
			skipRerender: true,
			force: true,
		});
	}

	function init() {
		populateZoomOptions();
		readInitialZoom();
		readInitialVectorScaling();
		bindUIEvents();
		reflectUIState();
		applyZoom();
	}

	function setCanvasMetrics(metrics) {
		if (metrics === null) {
			displayMetrics = null;
			baselineMetrics = null;
			lastVectorRenderLimit = Infinity;
			vectorBaselineReady = false;
			applyZoom();
			return;
		}
		const appliedZoom = metrics.zoomApplied || 1;
		lastRenderZoom = appliedZoom;
		lastVectorRenderLimit = Number.isFinite(metrics.vectorRenderLimit) ? metrics.vectorRenderLimit : Infinity;
		displayMetrics = {
			width: metrics.width / appliedZoom,
			height: metrics.height / appliedZoom,
			translateX: metrics.translateX / appliedZoom,
			translateY: metrics.translateY / appliedZoom,
		};
		baselineMetrics = {
			width: displayMetrics.width,
			height: displayMetrics.height,
			translateX: displayMetrics.translateX,
			translateY: displayMetrics.translateY,
		};
		vectorBaselineReady = true;
		applyZoom();
	}

	function getZoom() {
		return currentZoom;
	}

	function isVectorScalingEnabled() {
		return vectorScalingEnabled;
	}

	function isVectorScalingPreferred() {
		return vectorScalingPreferred;
	}

	function getBaseMetrics() {
		return baselineMetrics;
	}

	function invalidateBaseMetrics() {
		baselineMetrics = null;
		lastVectorRenderLimit = Infinity;
		vectorBaselineReady = false;
	}

	function isVectorBaselineReady() {
		return vectorBaselineReady;
	}

	return {
		init: init,
		setZoom: setZoom,
		incrementZoom: incrementZoom,
		resetZoom: resetZoom,
		applyZoom: applyZoom,
		setCanvasMetrics: setCanvasMetrics,
		setVectorScalingEnabled: setVectorScalingEnabled,
		restorePreferredVectorScaling: restorePreferredVectorScaling,
		getZoom: getZoom,
		isVectorScalingEnabled: isVectorScalingEnabled,
		isVectorScalingPreferred: isVectorScalingPreferred,
		getBaseMetrics: getBaseMetrics,
		handleVectorRasterFailure: handleVectorRasterFailure,
		invalidateBaseMetrics: invalidateBaseMetrics,
		isVectorBaselineReady: isVectorBaselineReady,
		queueBitmapFallback: queueBitmapFallback,
		requestVectorRerender: scheduleVectorRerender,
	};
})();

ZoomController.init();

// Mark the toolbar as hydrated so follow-up milestones can append additional controls
// while preserving the established focus order and ARIA annotations.
if (canvasToolbarElement !== null) {
	canvasToolbarElement.setAttribute("data-toolbar-ready", "true");
}

let allLogLines = "";
let traceLinesCount = 0;
let lastLogLine = null;
let repeatingLogLineCount = 0;

function clearTrace() {
	allLogLines = "";
	traceLinesCount = 0;
	lastLogLine = null;
	repeatingLogLineCount = 0;
	traceElement.textContent = "";
	traceDiv.scrollTop = 0;
}

function trace(message) {
	const allLines = traceElement.textContent;
	while (allLogLines.length > MAX_LOG_SIZE || traceLinesCount >= MAX_LOG_LINES) {
		const offset = allLogLines.indexOf("\n");
		if (offset < 0) {
			break;
		}
		allLogLines = allLogLines.substr(offset + 1);
		--traceLinesCount;
	}
	if (lastLogLine === message) {
		if (repeatingLogLineCount > 1) {
			const offset = allLogLines.lastIndexOf(" *");
			if (offset >= 0) {
				allLogLines = allLogLines.substr(0, offset);
			}
		} else {
			allLogLines = allLogLines.substr(0, allLogLines.length - 1); // remove last LF
		}
		++repeatingLogLineCount;
		allLogLines += " *" + repeatingLogLineCount + "\n";
	} else {
		allLogLines += message + "\n";
		++traceLinesCount;
		lastLogLine = message;
		repeatingLogLineCount = 1;
	}
	traceElement.textContent = allLogLines;
	traceDiv.scrollTop = traceDiv.scrollHeight;
}

// Can't use cwrap to pass very long strings, as they are placed on the stackm and not the heap.
const rasterizeIVG = function (source, scaling, scenarioIndex, entryOrdinal) {
		const size = Module.lengthBytesUTF8(source) + 1;
		const stringPointer = Module._malloc(size);
		Module.stringToUTF8(source, stringPointer, size);
		const selectedScenarioIndex = Number.isInteger(scenarioIndex) ? scenarioIndex : -1;
		const selectedEntryOrdinal = Number.isInteger(entryOrdinal) ? entryOrdinal : -1;
		const result = Module._rasterizeIVG(stringPointer, scaling, selectedScenarioIndex, selectedEntryOrdinal);
		Module._free(stringPointer);
		return result;
};
function deallocatePixels(pixelsPointer) {
	Module._deallocatePixels(pixelsPointer);
}

/**
	Run the IVG compiler, rasterize the output and push the resulting bitmap onto the canvas.
	The render lifecycle is intentionally documented so zoom/background milestones have clear
	anchor points:
		1. `runIVG` reads source from Ace, persists it, and clears trace output.
		2. We ask the WebAssembly module to rasterize at device pixel ratio, returning a buffer
			where the first four `Int32` values describe `{left, top, width, height}`.
		3. Canvas dimensions and style width/height are set before any transforms so zoom logic
			can hook into a consistent baseline.
		4. A CSS `translate(x, y)` positions the canvas relative to the artboard origin. Zoom
			will extend this by chaining `scale(...)` while keeping translation anchored.
		5. The decoded pixels are blitted via `putImageData`, completing the redraw.
*/
function runIVG(reason) {
	if (rasterizeInProgress) {
		rerunQueuedWhileBusy = true;
		if (typeof reason === "string" && reason.length > 0) {
			rerunQueuedReason = reason;
		} else if (rerunQueuedReason === "") {
			rerunQueuedReason = "queued rerun";
		}
		return;
	}
	rasterizeInProgress = true;
	rerunQueuedWhileBusy = false;
	const invocationReason = typeof reason === "string" ? reason : "";
	rerunQueuedReason = "";
	clearTrace();
	trace("Running IVG");
	if (invocationReason !== "") {
		trace("Render reason: " + invocationReason);
	}
	const linesCountWas = traceLinesCount;
	const start = Date.now();
	const sourceCode = aceEditor.getValue();
	const sourceSignature = computeSourceSignature(sourceCode);
	const sourceChanged = sourceSignature !== lastRasterizedSourceSignature;
	Settings.write(STORAGE_KEYS.SOURCE, sourceCode);
	Settings.write(STORAGE_KEYS.RUN_ON_STARTUP, "false");
	let ok = false;
	const zoomLevel = ZoomController.getZoom();
	const vectorRescaleEnabled = ZoomController.isVectorScalingEnabled();
	SnapshotController.prepareForRender(sourceSignature, sourceChanged);
	if (sourceChanged) {
		ZoomController.invalidateBaseMetrics();
	}
	const pixelRatio = window.devicePixelRatio;
	const dynamicPixelBudget = estimateVectorPixelBudget();
	let vectorPixelBudget = MAX_VECTOR_RASTER_PIXELS;
	let pixelBudgetReason = "pixel budget";
	let memoryBudgetPixels = 0;
	if (Number.isFinite(dynamicPixelBudget) && dynamicPixelBudget > 0 && dynamicPixelBudget < vectorPixelBudget) {
		vectorPixelBudget = dynamicPixelBudget;
		pixelBudgetReason = "memory budget";
		memoryBudgetPixels = dynamicPixelBudget;
	}
	const memoryBudgetBytes = memoryBudgetPixels > 0 ? memoryBudgetPixels * 4 + 16 : 0;
	let targetRenderZoom = vectorRescaleEnabled ? zoomLevel : 1;
	let renderZoom = targetRenderZoom;
	let vectorRenderLimit = Infinity;
	let clampReasons = [];
	const baseMetrics = ZoomController.getBaseMetrics();
	const baselineReady = ZoomController.isVectorBaselineReady();
	let skipVectorRaster = false;
	let preflightReasons = [];
	let basePixelWidth = 0;
	let basePixelHeight = 0;
	let basePixelArea = 0;
	let baselineRender = false;
	try {
		if (vectorRescaleEnabled) {
			if (baseMetrics !== null && baseMetrics.width > 0 && baseMetrics.height > 0) {
				basePixelWidth = baseMetrics.width * pixelRatio;
				basePixelHeight = baseMetrics.height * pixelRatio;
				basePixelArea = basePixelWidth * basePixelHeight;
				const zoomLimits = [];
				if (vectorPixelBudget > 0 && basePixelArea > 0) {
					const maxZoomByArea = Math.sqrt(vectorPixelBudget / basePixelArea);
					if (Number.isFinite(maxZoomByArea)) {
						zoomLimits.push({ value: maxZoomByArea, reason: pixelBudgetReason });
					}
				}
				if (basePixelWidth > 0) {
					const maxZoomByWidth = MAX_VECTOR_RASTER_DIMENSION / basePixelWidth;
					if (Number.isFinite(maxZoomByWidth)) {
						zoomLimits.push({ value: maxZoomByWidth, reason: "width limit" });
					}
				}
				if (basePixelHeight > 0) {
					const maxZoomByHeight = MAX_VECTOR_RASTER_DIMENSION / basePixelHeight;
					if (Number.isFinite(maxZoomByHeight)) {
						zoomLimits.push({ value: maxZoomByHeight, reason: "height limit" });
					}
				}
				if (zoomLimits.length > 0) {
					let bestLimit = Infinity;
					let appliedReasons = [];
					for (let index = 0; index < zoomLimits.length; ++index) {
						const entry = zoomLimits[index];
						if (!Number.isFinite(entry.value) || entry.value <= 0) {
							continue;
						}
						if (entry.value < bestLimit - 0.0001) {
							bestLimit = entry.value;
							appliedReasons = [entry.reason];
						} else if (Math.abs(entry.value - bestLimit) < 0.0001) {
							appliedReasons.push(entry.reason);
						}
					}
					if (bestLimit < Infinity && bestLimit < renderZoom - 0.0001) {
						renderZoom = Math.max(1, bestLimit);
						vectorRenderLimit = renderZoom;
						clampReasons = appliedReasons;
					}
				}
			} else if (!baselineReady && targetRenderZoom > 1.0001) {
				baselineRender = true;
				renderZoom = 1;
				vectorRenderLimit = Math.min(vectorRenderLimit, 1);
				if (clampReasons.indexOf("baseline sync") === -1) {
					clampReasons.push("baseline sync");
				}
			}
		}
		if (vectorRescaleEnabled && basePixelWidth > 0 && basePixelHeight > 0) {
			const targetPixelWidth = basePixelWidth * renderZoom;
			const targetPixelHeight = basePixelHeight * renderZoom;
			const targetPixelArea = targetPixelWidth * targetPixelHeight;
			if (vectorPixelBudget <= 0 || targetPixelArea > vectorPixelBudget) {
				preflightReasons.push(pixelBudgetReason);
				if (vectorPixelBudget > 0 && basePixelArea > 0) {
					const budgetZoomLimit = Math.sqrt(vectorPixelBudget / basePixelArea);
					if (Number.isFinite(budgetZoomLimit)) {
						vectorRenderLimit = Math.min(vectorRenderLimit, budgetZoomLimit);
					}
				}
			}
			if (targetPixelWidth > MAX_VECTOR_RASTER_DIMENSION) {
				preflightReasons.push("width limit");
				const maxZoomByWidth = MAX_VECTOR_RASTER_DIMENSION / basePixelWidth;
				if (Number.isFinite(maxZoomByWidth)) {
					vectorRenderLimit = Math.min(vectorRenderLimit, maxZoomByWidth);
				}
			}
			if (targetPixelHeight > MAX_VECTOR_RASTER_DIMENSION) {
				preflightReasons.push("height limit");
				const maxZoomByHeight = MAX_VECTOR_RASTER_DIMENSION / basePixelHeight;
				if (Number.isFinite(maxZoomByHeight)) {
					vectorRenderLimit = Math.min(vectorRenderLimit, maxZoomByHeight);
				}
			}
			if (preflightReasons.length > 0) {
				skipVectorRaster = true;
				vectorRenderLimit = Math.min(vectorRenderLimit, renderZoom);
			}
		}
		const rasterScale = pixelRatio * renderZoom;
		if (vectorRescaleEnabled) {
			if (skipVectorRaster) {
				const uniqueReasons = Array.from(new Set(preflightReasons));
				const reasonText = uniqueReasons.length > 0 ? " due to " + uniqueReasons.join(" & ") : "";
				const includePixelBudget = uniqueReasons.indexOf("pixel budget") !== -1;
				const includeMemoryBudget = uniqueReasons.indexOf("memory budget") !== -1;
				const includeDimensionCap = uniqueReasons.indexOf("width limit") !== -1 || uniqueReasons.indexOf("height limit") !== -1;
				const notes = [];
				if (includePixelBudget) {
					notes.push("pixel budget " + MAX_VECTOR_RASTER_PIXELS.toLocaleString("en-US") + " px");
				}
				if (includeMemoryBudget) {
					if (memoryBudgetPixels > 0) {
						const approxBytes = formatByteSize(memoryBudgetBytes);
						const memoryPixels = memoryBudgetPixels.toLocaleString("en-US");
						const bytesNote = approxBytes !== "" ? " (~ " + approxBytes + ")" : "";
						notes.push("memory budget ~ " + memoryPixels + " px" + bytesNote);
					} else {
						notes.push("memory budget");
					}
				}
				if (includeDimensionCap) {
					notes.push("dimension cap " + MAX_VECTOR_RASTER_DIMENSION + "px");
				}
				const noteText = notes.length > 0 ? " (" + notes.join(", ") + ")" : "";
				trace(
					"Vector rescale request was " +
						Math.round(targetRenderZoom * 100) +
						"% but exceeds the safe rasterization limits" +
						reasonText +
						noteText +
						". Staying in bitmap zoom.",
				);
			} else if (renderZoom < targetRenderZoom - 0.0001) {
				const reasonText = clampReasons.length > 0 ? " due to " + clampReasons.join(" & ") : "";
				const includePixelBudget = clampReasons.indexOf("pixel budget") !== -1;
				const includeMemoryBudget = clampReasons.indexOf("memory budget") !== -1;
				const includeDimensionCap = clampReasons.indexOf("width limit") !== -1 || clampReasons.indexOf("height limit") !== -1;
				const notes = [];
				if (includePixelBudget) {
					notes.push("pixel budget " + MAX_VECTOR_RASTER_PIXELS.toLocaleString("en-US") + " px");
				}
				if (includeMemoryBudget) {
					if (memoryBudgetPixels > 0) {
						const approxBytes = formatByteSize(memoryBudgetBytes);
						const memoryPixels = memoryBudgetPixels.toLocaleString("en-US");
						const bytesNote = approxBytes !== "" ? " (~ " + approxBytes + ")" : "";
						notes.push("memory budget ~ " + memoryPixels + " px" + bytesNote);
					} else {
						notes.push("memory budget");
					}
				}
				if (includeDimensionCap) {
					notes.push("dimension cap " + MAX_VECTOR_RASTER_DIMENSION + "px");
				}
				const noteText = notes.length > 0 ? " (" + notes.join(", ") + ")" : "";
				trace(
					"Vector rescale request was " +
						Math.round(targetRenderZoom * 100) +
						"% but clamped to " +
						Math.round(renderZoom * 100) +
						"%" +
						reasonText +
						noteText +
						".",
				);
			} else {
				trace(
					"Vector rescale enabled - rasterizing at " + Math.round(renderZoom * 100) + "% (" + rasterScale.toFixed(2) + "x device ratio)",
				);
			}
		} else {
			trace("Bitmap scaling active - requesting nearest-neighbor interpolation on the CSS transform.");
		}
		let rasterPointer = 0;
		let end = Date.now();
				if (!skipVectorRaster) {
						const snapshotSelection = SnapshotController.getSelectionForRender();
						const selectionScenarioIndex = snapshotSelection && Number.isInteger(snapshotSelection.scenarioIndex) ? snapshotSelection.scenarioIndex : -1;
						const selectionEntryOrdinal = snapshotSelection && Number.isInteger(snapshotSelection.entryOrdinal) ? snapshotSelection.entryOrdinal : -1;
						rasterPointer = rasterizeIVG(sourceCode, rasterScale, selectionScenarioIndex, selectionEntryOrdinal);
						end = Date.now();
				}
				if (!skipVectorRaster && rasterPointer !== 0) {
						const heapBuffer = Module.HEAPU8.buffer;
						const headerSigned = new Int32Array(heapBuffer, rasterPointer, 4);
						const left = headerSigned[0];
						const top = headerSigned[1];
						const width = headerSigned[2];
						const height = headerSigned[3];
						const header = new Uint32Array(heapBuffer, rasterPointer, 8);
						const pixelBytes = header[4];
						const catalogBytes = header[5];
						const defaultScenarioIndex = header[6];
						const defaultEntryOrdinal = header[7];
						const pixelOffset = rasterPointer + 8 * 4;
						const catalogOffset = pixelOffset + pixelBytes;
						const pixelData = new Uint8Array(heapBuffer, pixelOffset, pixelBytes);
						const snapshotCatalogJson = catalogBytes > 0 ? readUtf8FromHeap(catalogOffset, catalogBytes) : "";
						const DEFAULT_SELECTION_SENTINEL = 0xffffffff;
						const normalizedScenarioIndex = defaultScenarioIndex === DEFAULT_SELECTION_SENTINEL ? -1 : defaultScenarioIndex;
						const normalizedEntryOrdinal = defaultEntryOrdinal === DEFAULT_SELECTION_SENTINEL ? -1 : defaultEntryOrdinal;
						ivgCanvas.width = width;
						ivgCanvas.height = height;
						const imageData = ivgContext.createImageData(width, height);
						imageData.data.set(pixelData);
						deallocatePixels(rasterPointer);
						const cssWidth = width / pixelRatio;
						const cssHeight = height / pixelRatio;
						const translateX = left / pixelRatio;
						const translateY = top / pixelRatio;
						ZoomController.setCanvasMetrics({
								width: cssWidth,
								height: cssHeight,
								translateX: translateX,
								translateY: translateY,
								zoomApplied: renderZoom,
								vectorRenderLimit: vectorRenderLimit,
						});
						SnapshotController.applyRenderResult({
								catalogJson: snapshotCatalogJson,
								defaultScenarioIndex: normalizedScenarioIndex,
								defaultEntryOrdinal: normalizedEntryOrdinal,
						});
						ivgContext.putImageData(imageData, 0, 0);
			trace("Completed IVG");
			trace("Time spent: " + (end - start) + "ms");
			ok = true;
			lastRasterizedSourceSignature = sourceSignature;
			if (vectorRescaleEnabled && baselineRender && targetRenderZoom > renderZoom + 0.0001) {
				ZoomController.requestVectorRerender("vector-baseline");
			}
	} else if (!skipVectorRaster) {
		trace("Aborted IVG");
		if (vectorRescaleEnabled) {
			ZoomController.handleVectorRasterFailure({
				renderZoom: renderZoom,
				vectorRenderLimit: vectorRenderLimit,
			});
		}
	} else {
		ok = false;
		if (vectorRescaleEnabled) {
			ZoomController.handleVectorRasterFailure({
				renderZoom: renderZoom,
				vectorRenderLimit: vectorRenderLimit,
			});
			trace("Vector preflight limits exceeded; retaining current zoom mode.");
		}
	}
	Settings.write(STORAGE_KEYS.RUN_ON_STARTUP, "true");
	} catch (e) {
		trace("Rasterization crashed");
		trace(e);
		if (vectorRescaleEnabled) {
			ZoomController.handleVectorRasterFailure({
				renderZoom: renderZoom,
				vectorRenderLimit: vectorRenderLimit,
			});
		}
	}
	if (ok) {
		ZoomController.restorePreferredVectorScaling();
	}
	if (!ok) {
		ivgContext.beginPath();
		ivgContext.moveTo(0, 0);
		ivgContext.lineTo(ivgCanvas.width, ivgCanvas.height);
		ivgContext.moveTo(0, ivgCanvas.height);
		ivgContext.lineTo(ivgCanvas.width, 0);
		ivgContext.strokeStyle = "red";
		ivgContext.lineWidth = 10;
		ivgContext.stroke();
	}
	rasterizeInProgress = false;
	if (rerunQueuedWhileBusy) {
		const queuedReason = rerunQueuedReason;
		rerunQueuedWhileBusy = false;
		rerunQueuedReason = "";
		runIVG(queuedReason);
	}
}

const aceEditor = ace.edit("editor");
aceEditor.setTheme("ace/theme/twilight");
const aceSession = aceEditor.getSession();
aceSession.setUseSoftTabs(false);
aceSession.setMode("ace/mode/ivg");
let recompileTimer = null;
aceSession.on("change", function (e) {
	if (recompileTimer !== null) {
		clearTimeout(recompileTimer);
		recompileTimer = null;
	}
	recompileTimer = setTimeout(runIVG, 500);
});

if (leftPanelElement !== null) {
	const initialWidth = leftPanelElement.offsetWidth;
	if (initialWidth > 0) {
		leftPanelElement.style.flexBasis = initialWidth + "px";
	}
	leftPanelElement.style.removeProperty("width");
}

let isDragging = false;
let dragStartX = 0;
let dragStartWidth = 0;

if (leftRightSplitElement !== null) {
	leftRightSplitElement.addEventListener("mousedown", function handleSplitMouseDown(e) {
		if (leftPanelElement === null) {
			return;
		}
		isDragging = true;
		dragStartX = e.clientX;
		dragStartWidth = leftPanelElement.getBoundingClientRect().width;
		document.body.classList.add("is-resizing");
		e.preventDefault();
	});
}

document.addEventListener("mousemove", function handleSplitMouseMove(e) {
	if (!isDragging || leftPanelElement === null) {
		return;
	}
	const delta = e.clientX - dragStartX;
	const nextWidth = Math.max(MIN_LEFT_PANEL_WIDTH, dragStartWidth + delta);
	leftPanelElement.style.flexBasis = nextWidth + "px";
});

document.addEventListener("mouseup", function handleSplitMouseUp() {
	if (!isDragging) {
		return;
	}
	isDragging = false;
	document.body.classList.remove("is-resizing");
});
