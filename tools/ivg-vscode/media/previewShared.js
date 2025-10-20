"use strict";

(function createIvfPreviewModule(global) {
function createHostAdapter(adapter) {
const noop = function noop() {};
const notifyStatus = adapter && typeof adapter.notifyStatus === "function" ? adapter.notifyStatus : noop;
const onReady = adapter && typeof adapter.onReady === "function" ? adapter.onReady : noop;
return {
notifyStatus: notifyStatus,
onReady: onReady,
emitTrace: adapter && typeof adapter.emitTrace === "function" ? adapter.emitTrace : noop,
};
}

	function createPreview(adapter) {
		const host = createHostAdapter(adapter);

		const MAX_LOG_SIZE = 64 * 1024;
		const MAX_LOG_LINES = 1000;

const statusElement = document.getElementById("status");
const ivgCanvas = document.getElementById("ivgCanvas");
		const ivgContext = ivgCanvas ? ivgCanvas.getContext("2d") : null;
		const screenElement = document.getElementById("screen");
		const previewContainer = document.getElementById("previewContainer");
		const canvasToolbarElement = document.getElementById("canvasToolbar");
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
		const heapTextDecoder = typeof TextDecoder !== "undefined" ? new TextDecoder("utf-8") : null;

		const STORAGE_KEYS = Object.freeze({
			SOURCE: "ivgSource",
			ZOOM_LEVEL: "ivgZoomLevel",
			VECTOR_SCALING: "ivgVectorScaling",
			BACKGROUND_COLOR: "ivgBackgroundColor",
			SNAPSHOT_SELECTION: "ivgSnapshotSelection",
		});

		function withElement(element, callback) {
			if (element !== null && typeof callback === "function") {
				callback(element);
			}
		}

		function isEditableElement(element) {
			if (!element) {
				return false;
			}
			if (element.isContentEditable) {
				return true;
			}
			const tagName = element.tagName;
			if (!tagName) {
				return false;
			}
			const normalized = tagName.toUpperCase();
			return normalized === "INPUT" || normalized === "TEXTAREA";
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

		function readUtf8FromHeap(module, offset, byteLength) {
			if (!module || !module.HEAPU8 || !Number.isInteger(offset) || !Number.isInteger(byteLength) || byteLength <= 0) {
				return "";
			}
			if (typeof module.UTF8ArrayToString === "function") {
				return module.UTF8ArrayToString(module.HEAPU8, offset, byteLength);
			}
			if (typeof UTF8ArrayToString === "function") {
				return UTF8ArrayToString(module.HEAPU8, offset, byteLength);
			}
			const heap = module.HEAPU8;
			const end = offset + byteLength;
			if (heapTextDecoder && typeof heap.subarray === "function") {
				let decodeEnd = end;
				for (let index = offset; index < end; ++index) {
					if (heap[index] === 0) {
						decodeEnd = index;
						break;
					}
				}
				return heapTextDecoder.decode(heap.subarray(offset, decodeEnd));
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
				if ((u0 & 0xe0) === 0xc0) {
					const u1 = heap[index++] & 0x3f;
					result += String.fromCharCode(((u0 & 0x1f) << 6) | u1);
					continue;
				}
				if ((u0 & 0xf0) === 0xe0) {
					const u1 = heap[index++] & 0x3f;
					const u2 = heap[index++] & 0x3f;
					result += String.fromCharCode(((u0 & 0x0f) << 12) | (u1 << 6) | u2);
					continue;
				}
				const u1 = heap[index++] & 0x3f;
				const u2 = heap[index++] & 0x3f;
				const u3 = heap[index++] & 0x3f;
				const codePoint = ((u0 & 0x07) << 18) | (u1 << 12) | (u2 << 6) | u3;
				result += String.fromCodePoint(codePoint);
			}
			return result;
		}


		const Settings = (function createSettingsAdapter() {
			function read(key, fallback) {
				try {
					const value = window.localStorage.getItem(key);
					return value === null ? fallback : value;
				} catch (error) {
					return fallback;
				}
			}

			function write(key, value) {
				try {
					if (value === null || value === undefined) {
						window.localStorage.removeItem(key);
					} else {
						window.localStorage.setItem(key, value);
					}
				} catch (error) {
					// Ignore storage failures; they are non-fatal for the preview UI.
				}
			}

			return {
				read: read,
				write: write,
			};
		})();

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

			function deriveImplicitGroupInfo(scenario, fallbackIndex) {
				const name = typeof scenario.name === "string" ? scenario.name : "";
				const patternMatch = name.match(/^(.*-\d+)-(\d+)$/);
				if (patternMatch) {
					const parsedIndex = parseInt(patternMatch[2], 10);
					return {
						key: patternMatch[1],
						listIndex: Number.isFinite(parsedIndex) ? parsedIndex - 1 : null,
					};
				}
				if (name.length > 0) {
					return { key: name, listIndex: null };
				}
				return { key: "implicit-" + String(fallbackIndex), listIndex: null };
			}

			function prepareImplicitGroups(parsedCatalog) {
				const groups = new Map();
				if (!parsedCatalog || !Array.isArray(parsedCatalog.scenarios)) {
					return groups;
				}
				for (let i = 0; i < parsedCatalog.scenarios.length; ++i) {
					const scenario = parsedCatalog.scenarios[i];
					if (!scenario || !Array.isArray(scenario.entries) || scenario.entries.length === 0) {
						continue;
					}
					const hasScenarioName = typeof scenario.name === "string" && scenario.name.length > 0;
					const scenarioIsExplicit = scenario.explicit === true;
					if (scenarioIsExplicit && hasScenarioName) {
						continue;
					}
					const fallbackIndex = Number.isInteger(scenario.index) ? scenario.index : i;
					const info = deriveImplicitGroupInfo(scenario, fallbackIndex);
					let group = groups.get(info.key);
					if (!group) {
						group = { totalEntries: 0, firstPosition: i, ordinal: 0, processedEntries: 0 };
						groups.set(info.key, group);
					}
					group.totalEntries += scenario.entries.length;
					if (i < group.firstPosition) {
						group.firstPosition = i;
					}
				}
				const orderedGroups = Array.from(groups.values());
				orderedGroups.sort((a, b) => a.firstPosition - b.firstPosition);
				for (let index = 0; index < orderedGroups.length; ++index) {
					orderedGroups[index].ordinal = index + 1;
					orderedGroups[index].processedEntries = 0;
				}
				return groups;
			}

			function buildOptionLabel(baseLabel, entryCount, listIndex) {
				if (!Number.isInteger(entryCount) || entryCount <= 1) {
					return baseLabel;
				}
				const normalizedIndex = Number.isInteger(listIndex) ? listIndex : 0;
				return baseLabel + " #" + String(normalizedIndex);
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
				const implicitGroups = prepareImplicitGroups(parsedCatalog);
				for (let i = 0; i < parsedCatalog.scenarios.length; ++i) {
					const scenario = parsedCatalog.scenarios[i];
					if (!scenario || !Array.isArray(scenario.entries) || scenario.entries.length === 0) {
						continue;
					}
					const entries = scenario.entries;
					const hasScenarioName = typeof scenario.name === "string" && scenario.name.length > 0;
					const scenarioIsExplicit = scenario.explicit === true;
					if (scenarioIsExplicit && hasScenarioName) {
						for (let j = 0; j < entries.length; ++j) {
							const entry = entries[j];
							if (!entry) {
								continue;
							}
							const explicitListIndex = Number.isInteger(entry.listIndex) ? entry.listIndex : Number.isInteger(entry.entryOrdinal) ? entry.entryOrdinal - 1 : null;
							options.push({
								value: String(scenario.index) + ":" + String(entry.entryOrdinal),
								label: buildOptionLabel(scenario.name, entries.length, explicitListIndex),
								scenarioIndex: scenario.index,
								entryOrdinal: entry.entryOrdinal,
							});
						}
						continue;
					}
					const fallbackIndex = Number.isInteger(scenario.index) ? scenario.index : i;
					const info = deriveImplicitGroupInfo(scenario, fallbackIndex);
					const group = implicitGroups.get(info.key);
					const entryCount = group && group.totalEntries > 0 ? group.totalEntries : entries.length;
					const baseLabel = group && group.ordinal > 0 ? "unlabeled-" + String(group.ordinal) : "unlabeled";
					for (let j = 0; j < entries.length; ++j) {
						const entry = entries[j];
						if (!entry) {
							continue;
						}
						let listIndex = null;
						if (entryCount > 1) {
							if (entries.length === 1 && Number.isInteger(info.listIndex)) {
								listIndex = info.listIndex;
							} else if (Number.isInteger(entry.listIndex)) {
								listIndex = entry.listIndex;
							} else if (Number.isInteger(entry.entryOrdinal)) {
								listIndex = entry.entryOrdinal - 1;
							} else if (group && group.totalEntries > 1) {
								listIndex = group.processedEntries;
							}
						}
						options.push({
							value: String(scenario.index) + ":" + String(entry.entryOrdinal),
							label: buildOptionLabel(baseLabel, entryCount, listIndex),
							scenarioIndex: scenario.index,
							entryOrdinal: entry.entryOrdinal,
						});
						if (group) {
							group.processedEntries += 1;
						}
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
				const executedSelection = Number.isInteger(params.defaultScenarioIndex) && Number.isInteger(params.defaultEntryOrdinal) && params.defaultScenarioIndex >= 0 && params.defaultEntryOrdinal >= 0 ? {
					scenarioIndex: params.defaultScenarioIndex >>> 0,
					entryOrdinal: params.defaultEntryOrdinal >>> 0,
				} : null;
				const catalogDefault = parsed && Number.isInteger(parsed.defaultScenarioIndex) && Number.isInteger(parsed.defaultEntryOrdinal) && parsed.defaultScenarioIndex >= 0 && parsed.defaultEntryOrdinal >= 0 ? {
					scenarioIndex: parsed.defaultScenarioIndex >>> 0,
					entryOrdinal: parsed.defaultEntryOrdinal >>> 0,
				} : null;
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
				trace("Snapshot catalog: " + scenarioCount + " scenario" + (scenarioCount === 1 ? "" : "s") + ", " + entriesCount + " entr" + (entriesCount === 1 ? "y" : "ies"));
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
				if (snapshotScenarioSelect && key !== "" && snapshotScenarioSelect.value !== key) {
					snapshotScenarioSelect.value = key;
				}
				trace("Snapshot selection changed to scenario " + next.scenarioIndex + ", entry " + next.entryOrdinal);
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
					trace("Snapshot source updated; reusing cached selection scenario " + activeSelection.scenarioIndex + ", entry " + activeSelection.entryOrdinal);
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
					setStatus("Re-rendering current IVG…", { level: "info", notify: false });
					renderCurrentSource();
				}
			});
		}

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
			let defaultBodyBackground = "";
			const bodyElement = document.body;

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

			function updatePreviewBackground(color) {
				const definition = getColorDefinition(color);
				if (definition === null) {
					return;
				}
				currentColor = definition.value;
				const transparent = currentColor === "none";
				const nextClass = transparent ? "transparent" : "";
				toggleClass(previewContainer, "transparent", transparent);
				toggleClass(screenElement, "transparent", transparent);
				if (screenElement !== null) {
					screenElement.style.backgroundColor = transparent ? "" : definition.value;
				}
				if (previewContainer !== null) {
					previewContainer.style.backgroundColor = transparent ? "" : definition.value;
				}
			}

			function selectBackground(color) {
				updatePreviewBackground(color);
				Settings.write(STORAGE_KEYS.BACKGROUND_COLOR, currentColor);
				refreshSelectedSwatch();
			}

			function closeOverlay(options) {
				const settings = options || {};
				if (!isOpen) {
					return;
				}
				isOpen = false;
				toggleClass(backgroundOverlay, "is-hidden", true);
				toggleClass(document.body, "dialog-open", false);
				if (bodyElement) {
					bodyElement.style.backgroundColor = defaultBodyBackground;
				}
				if (!settings.skipFocus && lastTrigger) {
					lastTrigger.focus();
				}
				lastTrigger = null;
			}

			function openOverlay(trigger) {
				if (isOpen) {
					return;
				}
				if (bodyElement) {
					defaultBodyBackground = bodyElement.style.backgroundColor || "";
				}
				isOpen = true;
				lastTrigger = trigger || null;
				toggleClass(backgroundOverlay, "is-hidden", false);
				toggleClass(document.body, "dialog-open", true);
				if (backgroundDialog) {
					backgroundDialog.focus();
				}
			}

			function buildSwatch(definition) {
				const swatch = document.createElement("button");
				swatch.type = "button";
				swatch.className = "background-swatch";
				swatch.setAttribute("role", "option");
				swatch.setAttribute("aria-selected", "false");
				swatch.dataset.background = definition.value;
				if (definition.value !== "none") {
					swatch.style.setProperty("--swatch-color", definition.preview);
				}
				const preview = document.createElement("span");
				preview.className = "background-swatch__preview";
				if (definition.value === "none") {
					preview.classList.add("background-swatch__preview--transparent");
				}
				const label = document.createElement("span");
				label.className = "background-swatch__label";
				label.textContent = definition.label;
				swatch.appendChild(preview);
				swatch.appendChild(label);
				swatch.addEventListener("click", function selectSwatch() {
					selectBackground(definition.value);
					closeOverlay();
				});
				return swatch;
			}

			function refreshSelectedSwatch() {
				if (!backgroundSwatchContainer) {
					return;
				}
				const swatches = backgroundSwatchContainer.querySelectorAll(".background-swatch");
				for (let index = 0; index < swatches.length; ++index) {
					const element = swatches[index];
					const value = element.dataset.background || "";
					const isSelected = value === currentColor;
					if (isSelected) {
						element.setAttribute("data-selected", "true");
						element.setAttribute("aria-selected", "true");
						if (document.activeElement === element) {
							element.focus();
						}
					} else {
						element.removeAttribute("data-selected");
						element.setAttribute("aria-selected", "false");
					}
				}
			}

			function populateSwatches() {
				if (!backgroundSwatchContainer) {
					return;
				}
				backgroundSwatchContainer.innerHTML = "";
				const fragment = document.createDocumentFragment();
				for (let index = 0; index < BACKGROUND_COLORS.length; ++index) {
					const definition = BACKGROUND_COLORS[index];
					fragment.appendChild(buildSwatch(definition));
				}
				backgroundSwatchContainer.appendChild(fragment);
				refreshSelectedSwatch();
			}

			function init() {
				const stored = Settings.read(STORAGE_KEYS.BACKGROUND_COLOR, BACKGROUND_DEFAULT);
				if (typeof stored === "string") {
					currentColor = normalizeColor(stored);
					updatePreviewBackground(currentColor);
				}
				withElement(backgroundButton, function attachButton(button) {
					button.addEventListener("click", function handleOpen(event) {
						event.preventDefault();
						populateSwatches();
						openOverlay(button);
					});
				});
				withElement(backgroundCloseButton, function attachClose(button) {
					button.addEventListener("click", function handleClose(event) {
						event.preventDefault();
						closeOverlay();
					});
				});
				withElement(backgroundOverlay, function attachOverlay(overlay) {
					overlay.addEventListener("click", function handleOverlayClick(event) {
						if (event.target === overlay) {
							closeOverlay();
						}
					});
				});
				withElement(backgroundDialog, function attachDialog(dialog) {
					dialog.addEventListener("keydown", function handleDialogKeydown(event) {
						if (event.key === "Escape") {
							event.preventDefault();
							closeOverlay();
						}
					});
				});
			}

			return {
				init: init,
				selectBackground: selectBackground,
				refreshSelectedSwatch: refreshSelectedSwatch,
			};
		})();

		const ZOOM_SELECT_PRESETS = Object.freeze([0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10]);
		const ZOOM_PRESETS = ZOOM_SELECT_PRESETS;
		const CUSTOM_ZOOM_OPTION_VALUE = "custom";
		const FALLBACK_CANVAS_SIZE = Object.freeze({
			width: 320,
			height: 320,
		});

		const ZOOM_CONSTANTS = Object.freeze({
			MIN: ZOOM_PRESETS[0],
			MAX: ZOOM_PRESETS[ZOOM_PRESETS.length - 1],
			STEP: 1,
			DEFAULT: 1.0,
		});

		const ZoomController = (function createZoomController() {
			let currentZoom = ZOOM_CONSTANTS.DEFAULT;
			let baseMetrics = null;
			let vectorScalingEnabled = false;
			let vectorScalingPreferred = false;
			let vectorScalingSuppressed = false;
			const ZOOM_EPSILON = 0.0001;
			let rerenderCallback = null;

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
				const fragment = document.createDocumentFragment();
				for (let index = 0; index < ZOOM_SELECT_PRESETS.length; ++index) {
					const preset = ZOOM_SELECT_PRESETS[index];
					const option = document.createElement("option");
					option.value = String(zoomToPercent(preset));
					option.textContent = zoomToPercent(preset) + "%";
					if (Math.abs(preset - ZOOM_CONSTANTS.DEFAULT) < ZOOM_EPSILON) {
						option.selected = true;
					}
					fragment.appendChild(option);
				}
				zoomLevelSelect.appendChild(fragment);
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

			function setVectorScalingEnabled(nextState, options) {
				const settings = options || {};
				const normalized = nextState === true;
				if (settings.skipPreferenceUpdate !== true) {
					vectorScalingPreferred = normalized;
				}
				const shouldPersist = settings.skipPersist === true ? false : settings.persist !== false;
				const shouldNotify = settings.notify !== false;
				if (vectorScalingEnabled === normalized && settings.force !== true) {
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
				if (shouldPersist) {
					persistVectorScaling();
				}
				reflectVectorScalingState();
				applyZoom();
				if (rerenderCallback && shouldNotify) {
					rerenderCallback("vector-toggle");
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

			function applyZoom() {
				if (ivgCanvas === null) {
					return;
				}
				ivgCanvas.setAttribute("data-scaling-mode", vectorScalingEnabled ? "vector" : "bitmap");
				const targetZoom = vectorScalingEnabled ? currentZoom : 1;
				if (baseMetrics === null) {
					const fallbackWidth = FALLBACK_CANVAS_SIZE.width;
					const fallbackHeight = FALLBACK_CANVAS_SIZE.height;
					ivgCanvas.width = fallbackWidth;
					ivgCanvas.height = fallbackHeight;
					ivgCanvas.style.width = fallbackWidth * targetZoom + "px";
					ivgCanvas.style.height = fallbackHeight * targetZoom + "px";
					ivgCanvas.style.transformOrigin = "top left";
					if (vectorScalingEnabled) {
						ivgCanvas.style.transform = "translate(0px, 0px)";
					} else {
						ivgCanvas.style.transform = "scale(" + currentZoom + ")";
					}
					return;
				}
				const metrics = baseMetrics;
				ivgCanvas.style.width = metrics.width * targetZoom + "px";
				ivgCanvas.style.height = metrics.height * targetZoom + "px";
				ivgCanvas.style.transformOrigin = "top left";
				if (vectorScalingEnabled) {
					ivgCanvas.style.transform = "translate(" + metrics.translateX * targetZoom + "px," + metrics.translateY * targetZoom + "px)";
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
				if (vectorScalingEnabled && rerenderCallback && settings.skipVectorRefresh !== true) {
					rerenderCallback("zoom-change");
				}
			}

			function findPresetIndex(value) {
				for (let index = 0; index < ZOOM_PRESETS.length; ++index) {
					if (Math.abs(ZOOM_PRESETS[index] - value) < ZOOM_EPSILON) {
						return index;
					}
				}
				return -1;
			}

			function getAdjacentPreset(direction) {
				if (direction === 0) {
					return currentZoom;
				}
				const matchIndex = findPresetIndex(currentZoom);
				if (matchIndex >= 0) {
					const nextIndex = Math.min(ZOOM_PRESETS.length - 1, Math.max(0, matchIndex + direction));
					return ZOOM_PRESETS[nextIndex];
				}
				if (direction > 0) {
					for (let index = 0; index < ZOOM_PRESETS.length; ++index) {
						if (currentZoom < ZOOM_PRESETS[index] - ZOOM_EPSILON) {
							return ZOOM_PRESETS[index];
						}
					}
					return ZOOM_PRESETS[ZOOM_PRESETS.length - 1];
				}
				for (let index = ZOOM_PRESETS.length - 1; index >= 0; --index) {
					if (currentZoom > ZOOM_PRESETS[index] + ZOOM_EPSILON) {
						return ZOOM_PRESETS[index];
					}
				}
				return ZOOM_PRESETS[0];
			}

			function incrementZoom(deltaSteps) {
				if (deltaSteps === 0) {
					return;
				}
				const direction = deltaSteps > 0 ? 1 : -1;
				const target = getAdjacentPreset(direction);
				setZoom(target, { snapDirection: 0 });
			}

			function resetZoom() {
				setZoom(ZOOM_CONSTANTS.DEFAULT);
			}

			function handleZoomSelectChange(event) {
				const nextZoom = percentToZoom(event.target.value);
				setZoom(nextZoom, { skipPersist: false });
			}

			function init(callback) {
				rerenderCallback = typeof callback === "function" ? callback : null;
				const storedZoom = Number.parseFloat(Settings.read(STORAGE_KEYS.ZOOM_LEVEL, String(ZOOM_CONSTANTS.DEFAULT)));
				if (Number.isFinite(storedZoom)) {
					currentZoom = clampZoom(storedZoom);
				}
				const storedVector = Settings.read(STORAGE_KEYS.VECTOR_SCALING, "0");
				setVectorScalingEnabled(storedVector === "1", { persist: false, notify: false });
				populateZoomOptions();
				applyZoom();
				withElement(zoomOutButton, function attachZoomOut(button) {
					button.addEventListener("click", function handleZoomOut(event) {
						event.preventDefault();
						incrementZoom(-1);
					});
				});
				withElement(zoomInButton, function attachZoomIn(button) {
					button.addEventListener("click", function handleZoomIn(event) {
						event.preventDefault();
						incrementZoom(1);
					});
				});
				withElement(zoomResetButton, function attachZoomReset(button) {
					button.addEventListener("click", function handleZoomReset(event) {
						event.preventDefault();
						resetZoom();
					});
				});
				withElement(vectorScalingToggle, function attachVectorToggle(button) {
					button.addEventListener("click", function toggleVectorScaling(event) {
						event.preventDefault();
						setVectorScalingEnabled(!vectorScalingEnabled);
					});
				});
				withElement(zoomLevelSelect, function attachZoomSelect(select) {
					select.addEventListener("change", handleZoomSelectChange);
				});
				window.addEventListener("keydown", function handleZoomKeydown(event) {
					const key = event.key;
					if (!key || isEditableElement(document.activeElement)) {
						return;
					}
					const zoomInKey = key === "=" || key === "+" || key === "Add" || key === "NumpadAdd";
					const zoomOutKey = key === "-" || key === "_" || key === "Subtract" || key === "NumpadSubtract";
					if (event.ctrlKey || event.metaKey) {
						if (zoomInKey) {
							event.preventDefault();
							incrementZoom(1);
						} else if (zoomOutKey) {
							event.preventDefault();
							incrementZoom(-1);
						} else if (key === "0") {
							event.preventDefault();
							resetZoom();
						}
						return;
					}
					if (event.altKey) {
						return;
					}
					if (zoomInKey) {
						event.preventDefault();
						incrementZoom(1);
					} else if (zoomOutKey) {
						event.preventDefault();
						incrementZoom(-1);
					}
				});
				reflectUIState();
			}

			function setBaseMetrics(metrics) {
				if (!metrics) {
					baseMetrics = null;
					return;
				}
				const rasterScale = typeof metrics.rasterScale === "number" && metrics.rasterScale > 0 ? metrics.rasterScale : 1;
				const renderZoom = typeof metrics.renderZoom === "number" && metrics.renderZoom > 0 ? metrics.renderZoom : 1;
				const scaleDivisor = rasterScale;
				baseMetrics = {
					width: metrics.width / scaleDivisor,
					height: metrics.height / scaleDivisor,
					translateX: metrics.left / scaleDivisor,
					translateY: metrics.top / scaleDivisor,
					renderZoom: renderZoom,
					rasterScale: rasterScale,
				};
				applyZoom();
			}

			function clearMetrics() {
				baseMetrics = null;
				applyZoom();
			}

			function getBaseMetrics() {
				return baseMetrics;
			}

			function getRasterScale(pixelRatio) {
				if (vectorScalingEnabled) {
					return 1;
				}
				return Math.max(1, Math.round(pixelRatio));
			}

			function usesVectorScaling() {
				return vectorScalingEnabled;
			}

			function isVectorScalingPreferred() {
				return vectorScalingPreferred;
			}

			function getZoom() {
				return currentZoom;
			}

			return {
				init: init,
				setZoom: setZoom,
				resetZoom: resetZoom,
				applyRenderMetrics: setBaseMetrics,
				clearMetrics: clearMetrics,
				getRasterScale: getRasterScale,
				usesVectorScaling: usesVectorScaling,
				isVectorScalingPreferred: isVectorScalingPreferred,
				getZoom: getZoom,
				setVectorScalingEnabled: setVectorScalingEnabled,
				restorePreferredVectorScaling: restorePreferredVectorScaling,
				getBaseMetrics: getBaseMetrics,
			};
		})();

let allLogLines = "";
let traceLinesCount = 0;
let traceDisplayLines = [];
		let lastLogLine = null;
		let repeatingLogLineCount = 0;
		let moduleReady = false;
		let currentSource = "";
		let pendingSource = null;
		let pendingUri = null;
		let hasSuccessfulRender = false;
		let lastRasterizedSourceSignature = "";
		let currentDocumentUri = null;
		let lastSuccessfulRenderUri = null;
		let moduleRecoveryPromise = null;
		let suppressFailureStatus = false;

		function notifyHost(level, message, options) {
			const text = typeof message === "string" ? message : "";
			if (!text) {
				return;
			}
			host.notifyStatus(level, text, options || {});
		}

		function setStatus(message, options) {
			const text = typeof message === "string" ? message : "";
			if (statusElement) {
				statusElement.textContent = text;
			}
			const opts = options || {};
			if (opts.notify === false) {
				return;
			}
			const level = typeof opts.level === "string" ? opts.level : "info";
			const notifyOptions = {};
			if (typeof opts.durationMs === "number" && opts.durationMs >= 0) {
				notifyOptions.durationMs = opts.durationMs;
			}
			notifyHost(level, text, notifyOptions);
		}

		function describeError(error) {
			if (error instanceof Error && typeof error.message === "string") {
				return error.message;
			}
			if (error && typeof error === "object" && typeof error.message === "string") {
				return error.message;
			}
			if (typeof error === "string") {
				return error;
			}
			try {
				return JSON.stringify(error);
			} catch (serializationError) {
				return String(error);
			}
		}

		function isOutOfMemoryError(message) {
			if (typeof message !== "string") {
				return false;
			}
			const normalized = message.toLowerCase();
			return normalized.includes("oom") || normalized.includes("out of memory");
		}

		function recoverFromOutOfMemory(message) {
			const reloadModule = typeof global.__ivgReloadModule === "function" ? global.__ivgReloadModule : null;
			if (moduleRecoveryPromise) {
				return;
			}
			if (!reloadModule) {
				setStatus("Renderer ran out of memory. Reload the preview to continue.", { level: "error" });
				return;
			}
			suppressFailureStatus = true;
			moduleReady = false;
			pendingSource = currentSource;
			pendingUri = currentDocumentUri;
			if (typeof ZoomController.setVectorScalingEnabled === "function") {
				ZoomController.setVectorScalingEnabled(false, {
					notify: false,
					skipPersist: true,
					skipPreferenceUpdate: true,
					suppressPreference: true,
				});
			}
			setStatus("Renderer ran out of memory. Restarting…", { level: "error" });
			moduleRecoveryPromise = reloadModule()
				.then(function handleRecoverySuccess() {
					moduleRecoveryPromise = null;
				})
				.catch(function handleRecoveryFailure(error) {
					moduleRecoveryPromise = null;
					suppressFailureStatus = false;
					trace("Failed to restart IVG rasterizer module.");
					trace(describeError(error));
					setStatus("Failed to restart renderer after running out of memory.", { level: "error" });
				});
		}

		function clearTrace() {
			allLogLines = "";
			traceLinesCount = 0;
			lastLogLine = null;
			repeatingLogLineCount = 0;
			traceDisplayLines = [];
			host.emitTrace({ action: "clear" });
		}

		function trace(message) {
			const line = typeof message === "string" ? message : String(message);
			if (!line) {
				return;
			}
			let trimmed = false;
			while (allLogLines.length > MAX_LOG_SIZE || traceLinesCount >= MAX_LOG_LINES) {
				const offset = allLogLines.indexOf("\n");
				if (offset < 0) {
					allLogLines = "";
					traceLinesCount = 0;
					lastLogLine = null;
					repeatingLogLineCount = 0;
					traceDisplayLines = [];
					trimmed = true;
					break;
				}
				allLogLines = allLogLines.substr(offset + 1);
				--traceLinesCount;
				if (traceDisplayLines.length > 0) {
					traceDisplayLines.shift();
				}
				trimmed = true;
			}
			let displayLine = line;
			let replaceLast = false;
			if (lastLogLine === line && traceDisplayLines.length > 0) {
				if (repeatingLogLineCount > 1) {
					const offset = allLogLines.lastIndexOf(" *");
					if (offset >= 0) {
						allLogLines = allLogLines.substr(0, offset);
					}
				} else if (allLogLines.length > 0) {
					allLogLines = allLogLines.substr(0, allLogLines.length - 1);
				}
				++repeatingLogLineCount;
				displayLine = line + " *" + repeatingLogLineCount;
				allLogLines += " *" + repeatingLogLineCount + "\n";
				traceDisplayLines[traceDisplayLines.length - 1] = displayLine;
				replaceLast = true;
			} else {
				allLogLines += line + "\n";
				++traceLinesCount;
				lastLogLine = line;
				repeatingLogLineCount = 1;
				traceDisplayLines.push(displayLine);
			}
			if (trimmed) {
				host.emitTrace({ action: "reset", lines: traceDisplayLines.slice() });
				return;
			}
			host.emitTrace({ action: replaceLast ? "replace" : "append", text: displayLine });
		}

		function bindGlobalTrace() {
			if (!global || (typeof global !== "object" && typeof global !== "function")) {
				return;
			}
			const bufferedLines = Array.isArray(global.__ivgPreviewTraceBuffer) ? global.__ivgPreviewTraceBuffer.slice() : [];
			global.__ivgPreviewTraceBuffer = [];
			global.trace = function previewTraceBridge(message) {
				trace(typeof message === "string" ? message : String(message));
			};
			if (bufferedLines.length > 0) {
				for (let index = 0; index < bufferedLines.length; ++index) {
					trace(bufferedLines[index]);
				}
			}
		}

		bindGlobalTrace();

		function normalizeDocumentUri(value) {
			if (typeof value !== "string") {
				return null;
			}
			return value.length > 0 ? value : null;
		}

		function isSameDocumentUri(first, second) {
			if (first === second) {
				return true;
			}
			return first === null && second === null;
		}

		function drawFailureCross(options) {
			if (!ivgContext || !ivgCanvas) {
				return;
			}
			const settings = options || {};
			const preserveImage = settings.preserveImage === true;
			if (!preserveImage) {
				ivgContext.clearRect(0, 0, ivgCanvas.width, ivgCanvas.height);
			}
			ivgContext.save();
			ivgContext.beginPath();
			ivgContext.moveTo(0, 0);
			ivgContext.lineTo(ivgCanvas.width, ivgCanvas.height);
			ivgContext.moveTo(0, ivgCanvas.height);
			ivgContext.lineTo(ivgCanvas.width, 0);
			ivgContext.strokeStyle = "red";
			ivgContext.lineWidth = 6;
			ivgContext.lineCap = "round";
			ivgContext.lineJoin = "round";
			ivgContext.stroke();
			ivgContext.restore();
		}

		function getHeapView(module, key) {
			if (module && module[key]) {
				return module[key];
			}
			return null;
		}

		function readInt32(module, pointer) {
			const heap32 = getHeapView(module, "HEAP32");
			if (heap32) {
				return heap32[pointer >> 2];
			}
			const heapU32 = getHeapView(module, "HEAPU32");
			if (heapU32) {
				return heapU32[pointer >> 2] | 0;
			}
			if (module && typeof module.getValue === "function") {
				return module.getValue(pointer, "i32");
			}
			throw new Error("WebAssembly heap view missing");
		}

		const rasterizeIVG = function rasterizeIVG(source, scaling, scenarioIndex, entryOrdinal) {
			const size = global.Module.lengthBytesUTF8(source) + 1;
			const stringPointer = global.Module._malloc(size);
			global.Module.stringToUTF8(source, stringPointer, size);
			const selectedScenarioIndex = Number.isInteger(scenarioIndex) ? scenarioIndex : -1;
			const selectedEntryOrdinal = Number.isInteger(entryOrdinal) ? entryOrdinal : -1;
			const result = global.Module._rasterizeIVG(stringPointer, scaling, selectedScenarioIndex, selectedEntryOrdinal);
			global.Module._free(stringPointer);
			return result;
		};

		function renderCurrentSource() {
			if (!global.Module || !ivgCanvas || !ivgContext) {
				return;
			}
			clearTrace();
			const start = window.performance.now();
			let ok = false;
			try {
				const module = global.Module;
				const devicePixelRatio = window.devicePixelRatio || 1;
				const usesVectorScaling = ZoomController.usesVectorScaling();
				const currentZoom = ZoomController.getZoom();
				const rasterScale = usesVectorScaling ? currentZoom * devicePixelRatio : ZoomController.getRasterScale(devicePixelRatio);
				const sourceSignature = computeSourceSignature(currentSource);
				const sourceChanged = sourceSignature !== lastRasterizedSourceSignature;
				SnapshotController.prepareForRender(sourceSignature, sourceChanged);
				const snapshotSelection = SnapshotController.getSelectionForRender();
				const selectionScenarioIndex =
						snapshotSelection && Number.isInteger(snapshotSelection.scenarioIndex)
							? snapshotSelection.scenarioIndex
							: -1;
				const selectionEntryOrdinal =
						snapshotSelection && Number.isInteger(snapshotSelection.entryOrdinal)
							? snapshotSelection.entryOrdinal
							: -1;
				const result = rasterizeIVG(currentSource, rasterScale, selectionScenarioIndex, selectionEntryOrdinal);
				if (result !== 0) {
					const heapU8 = getHeapView(module, "HEAPU8");
					if (!heapU8) {
						module._deallocatePixels(result);
						throw new Error("WebAssembly heap unavailable");
					}
					const heapBuffer = heapU8.buffer;
					const headerSigned = new Int32Array(heapBuffer, result, 4);
					const left = headerSigned[0];
					const top = headerSigned[1];
					const width = headerSigned[2];
					const height = headerSigned[3];
					const header = new Uint32Array(heapBuffer, result, 8);
					const pixelBytes = header[4];
					const catalogBytes = header[5];
					const defaultScenarioIndex = header[6];
					const defaultEntryOrdinal = header[7];
					const pixelOffset = result + 8 * 4;
					const catalogOffset = pixelOffset + pixelBytes;
					const pixelSlice = heapU8.subarray(pixelOffset, pixelOffset + pixelBytes);
					const pixelData = new Uint8ClampedArray(pixelSlice);
					const snapshotCatalogJson =
							catalogBytes > 0 ? readUtf8FromHeap(module, catalogOffset, catalogBytes) : "";
					module._deallocatePixels(result);
					if (width > 0 && height > 0 && pixelData.length === pixelBytes) {
						if (ivgCanvas.width !== width || ivgCanvas.height !== height) {
							ivgCanvas.width = width;
							ivgCanvas.height = height;
						}
						const imageData = ivgContext.createImageData(width, height);
						imageData.data.set(pixelData);
						ZoomController.applyRenderMetrics({
							width: width,
							height: height,
							left: left,
							top: top,
							rasterScale: rasterScale,
							renderZoom: usesVectorScaling ? currentZoom : 1,
						});
						const DEFAULT_SELECTION_SENTINEL = 0xffffffff;
						const normalizedScenarioIndex =
							defaultScenarioIndex === DEFAULT_SELECTION_SENTINEL ? -1 : defaultScenarioIndex;
						const normalizedEntryOrdinal =
							defaultEntryOrdinal === DEFAULT_SELECTION_SENTINEL ? -1 : defaultEntryOrdinal;
						SnapshotController.applyRenderResult({
							catalogJson: snapshotCatalogJson,
							defaultScenarioIndex: normalizedScenarioIndex,
							defaultEntryOrdinal: normalizedEntryOrdinal,
						});
						ivgContext.putImageData(imageData, 0, 0);
						const end = window.performance.now();
						const durationMs = end - start;
						trace("--- IVG completed in " + durationMs + "ms ---");
						setStatus("Preview updated in " + durationMs + " ms.", {
							level: "info",
							durationMs: durationMs,
						});
						ok = true;
						hasSuccessfulRender = true;
						lastSuccessfulRenderUri = currentDocumentUri;
						lastRasterizedSourceSignature = sourceSignature;
					} else {
						trace("Rasterization returned no data");
					}
				}
			} catch (error) {
				trace("Rasterization crashed");
				const errorMessage = describeError(error);
				trace(errorMessage);
				if (isOutOfMemoryError(errorMessage)) {
					recoverFromOutOfMemory(errorMessage);
				}
			}
			if (ok) {
				ZoomController.restorePreferredVectorScaling();
			}
			if (!ok) {
				const preserveImage = hasSuccessfulRender && isSameDocumentUri(currentDocumentUri, lastSuccessfulRenderUri);
				if (!preserveImage) {
					ZoomController.clearMetrics();
				}
				drawFailureCross({ preserveImage: preserveImage });
				if (!suppressFailureStatus) {
					setStatus("Rendering failed. Check trace output for details.", {
						level: "error",
					});
				}
			}
		}

		function setSource(newSource, options) {
			const opts = options || {};
			const persist = opts.persist !== false;
			const nextUri = normalizeDocumentUri(opts.uri);
			if (!isSameDocumentUri(nextUri, currentDocumentUri)) {
				currentDocumentUri = nextUri;
				hasSuccessfulRender = false;
			}
			currentSource = typeof newSource === "string" ? newSource : "";
			if (persist) {
				if (currentSource) {
					Settings.write(STORAGE_KEYS.SOURCE, currentSource);
				} else {
					Settings.write(STORAGE_KEYS.SOURCE, null);
				}
			}
			renderCurrentSource();
		}

		function handleHostCommand(message) {
			if (!message || typeof message !== "object") {
				return;
			}
			switch (message.type) {
				case "setSource":
					if (!moduleReady) {
						pendingSource = typeof message.source === "string" ? message.source : "";
						pendingUri = normalizeDocumentUri(message.uri);
					} else {
						setSource(message.source, { persist: false, uri: normalizeDocumentUri(message.uri) });
					}
					if (typeof message.status === "string") {
						setStatus(message.status, { notify: false });
					} else if (moduleReady) {
						setStatus("Preview updated.", { notify: false });
					}
					break;
				case "clearTrace":
					clearTrace();
					setStatus("Preview trace cleared.", { level: "info" });
					break;
				case "rerender":
					if (moduleReady) {
						setStatus("Re-rendering current IVG…", { level: "info" });
						renderCurrentSource();
					}
					break;
				default:
					break;
			}
		}

		function initialize() {
			BackgroundController.init();
			ZoomController.init(function onZoomRerender(reason) {
				if (!moduleReady) {
					return;
				}
				if (typeof reason === "string") {
					setStatus("Re-rendering current IVG…", { level: "info", notify: false });
				}
				renderCurrentSource();
			});
			const cachedSource = Settings.read(STORAGE_KEYS.SOURCE, "");
			if (typeof cachedSource === "string" && cachedSource.length > 0) {
				currentSource = cachedSource;
				setStatus("Loaded cached IVG source. Waiting for renderer…", { level: "info" });
			} else {
				setStatus("Waiting for renderer…", { level: "info" });
			}
		}

		function handleModuleInitialized(initialSource) {
			moduleReady = true;
			suppressFailureStatus = false;
			moduleRecoveryPromise = null;
			const stored = Settings.read(STORAGE_KEYS.SOURCE, "");
			if (typeof stored === "string" && stored.length > 0) {
				currentSource = stored;
			} else if (typeof initialSource === "string" && initialSource.length > 0) {
				currentSource = initialSource;
				Settings.write(STORAGE_KEYS.SOURCE, currentSource);
			}
			const queuedSource = pendingSource;
			const queuedUri = pendingUri;
			pendingSource = null;
			pendingUri = null;
			setStatus("Renderer ready.", { level: "info" });
			if (typeof queuedSource === "string") {
				setSource(queuedSource, { persist: false, uri: queuedUri });
			} else {
				renderCurrentSource();
			}
			host.onReady();
		}

		return {
			initialize: initialize,
			handleModuleInitialized: handleModuleInitialized,
			handleHostCommand: handleHostCommand,
			setSource: setSource,
			clearTrace: clearTrace,
			renderCurrentSource: renderCurrentSource,
		};
	}

	global.IVGFiddlePreview = {
		create: createPreview,
	};
})(typeof window !== "undefined" ? window : this);
