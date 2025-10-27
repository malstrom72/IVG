"use strict";

(function createIvfPreviewModule(global) {
	function createHostAdapter(adapter) {
		const noop = function noop() {};
		const notifyStatus = adapter && typeof adapter.notifyStatus === "function" ? adapter.notifyStatus : noop;
		const onReady = adapter && typeof adapter.onReady === "function" ? adapter.onReady : noop;
		return {
			notifyStatus: notifyStatus,
			onReady: onReady,
		};
	}

	function createPreview(adapter) {
		const host = createHostAdapter(adapter);

		const MAX_LOG_SIZE = 64 * 1024;
		const MAX_LOG_LINES = 1000;

		const statusElement = document.getElementById("status");
		const traceElement = document.getElementById("trace");
		const traceDiv = document.getElementById("traceDiv");
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

		const STORAGE_KEYS = Object.freeze({
			SOURCE: "ivgSource",
			ZOOM_LEVEL: "ivgZoomLevel",
			VECTOR_SCALING: "ivgVectorScaling",
			BACKGROUND_COLOR: "ivgBackgroundColor",
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
		let lastLogLine = null;
		let repeatingLogLineCount = 0;
		let moduleReady = false;
		let currentSource = "";
		let pendingSource = null;
		let pendingUri = null;
		let hasSuccessfulRender = false;
		let currentDocumentUri = null;
		let lastSuccessfulRenderUri = null;
let moduleRecoveryPromise = null;
let suppressFailureStatus = false;
let renderInProgress = false;
let rerenderQueued = false;

const IncludeBundleController = (function createIncludeBundleController() {
const INCLUDE_ROOT = "/__ivg/includes";
const includeState = {
staged: null,
mountedRevision: null,
mountedRelativePaths: [],
mountError: null,
missingWarnings: new Set(),
};

function stageBundle(message) {
const bundle = normalizeBundle(message);
includeState.staged = bundle;
includeState.mountError = null;
includeState.missingWarnings.clear();
return { revision: bundle.revision, entryCount: bundle.entries.length };
}

function ensureMounted(module) {
if (!module || !module.FS) {
throw new Error("IVG renderer is not ready to mount include bundles.");
}
const fs = module.FS;
let changed = false;
if (!includeState.staged) {
if (includeState.mountedRelativePaths.length > 0) {
removeMountedPaths(fs, includeState.mountedRelativePaths);
includeState.mountedRelativePaths = [];
includeState.mountedRevision = null;
changed = true;
}
includeState.mountError = null;
removeIncludeRootIfEmpty(fs);
return { changed, revision: includeState.mountedRevision, entryCount: 0 };
}
if (
includeState.mountedRevision === includeState.staged.revision &&
includeState.mountedRelativePaths.length === includeState.staged.entries.length
) {
includeState.mountError = null;
return {
changed: false,
revision: includeState.mountedRevision,
entryCount: includeState.staged.entries.length,
};
}
removeMountedPaths(fs, includeState.mountedRelativePaths);
ensureIncludeRoot(fs);
const createdDirs = new Set();
for (let index = 0; index < includeState.staged.entries.length; ++index) {
const entry = includeState.staged.entries[index];
const targetPath = buildIncludePath(entry.relativePath);
ensureDirectory(fs, parentDirectory(targetPath), createdDirs);
fs.writeFile(targetPath, entry.data, { canOwn: true });
try {
fs.chmod(targetPath, 0o444);
} catch (error) {
// Ignore inability to change permissions in the standalone preview.
}
}
includeState.mountedRelativePaths = includeState.staged.entries.map((entry) => entry.relativePath);
includeState.mountedRevision = includeState.staged.revision;
includeState.mountError = null;
includeState.missingWarnings.clear();
changed = true;
return {
changed: true,
revision: includeState.mountedRevision,
entryCount: includeState.staged.entries.length,
};
}

function handleTraceLine(line) {
const prefix = "[IVGFiddle] Include missing:";
if (typeof line !== "string" || line.indexOf(prefix) !== 0) {
return;
}
const revision = includeState.mountedRevision || "unknown";
const key = revision + ":" + line;
if (includeState.missingWarnings.has(key)) {
return;
}
includeState.missingWarnings.add(key);
setStatus(line, { level: "warning" });
}

function recordMountFailure(message) {
includeState.mountError = typeof message === "string" ? message : String(message || "");
}

function getDiagnostics() {
return {
stagedRevision: includeState.staged ? includeState.staged.revision : null,
stagedEntryCount: includeState.staged ? includeState.staged.entries.length : 0,
mountedRevision: includeState.mountedRevision,
mountedEntryCount: includeState.mountedRelativePaths.length,
mountError: includeState.mountError || null,
};
}

function normalizeBundle(message) {
if (!message || typeof message !== "object") {
throw new Error("Include bundle payload missing.");
}
const revision =
typeof message.revision === "string" && message.revision.length > 0 ? message.revision : "rev-unknown";
const manifest = message.manifest;
if (!manifest || typeof manifest !== "object" || !Array.isArray(manifest.entries)) {
throw new Error("Include bundle manifest is missing entries.");
}
const assets = Array.isArray(message.assets) ? message.assets : [];
const assetMap = new Map();
for (let index = 0; index < assets.length; ++index) {
const asset = assets[index];
if (!asset || typeof asset !== "object") {
continue;
}
const relativePath = normalizeRelativePath(asset.path);
assetMap.set(relativePath, asset);
}
const normalizedEntries = [];
const entries = manifest.entries;
for (let index = 0; index < entries.length; ++index) {
const entry = entries[index];
if (!entry || typeof entry !== "object") {
continue;
}
const relativePath = normalizeRelativePath(entry.mountPath);
const asset = assetMap.get(relativePath);
if (!asset) {
throw new Error(`Include bundle is missing data for ${relativePath}.`);
}
const data = decodeAssetData(asset.data);
const byteLength = Number(entry.byteLength) || 0;
if (byteLength !== data.byteLength) {
throw new Error(
`Include asset ${relativePath} byte length mismatch (expected ${byteLength}, received ${data.byteLength}).`,
);
}
normalizedEntries.push({
relativePath: relativePath,
mountPath: "/" + relativePath,
byteLength: byteLength,
checksum: typeof entry.checksum === "string" ? entry.checksum : "",
mimeType: typeof entry.mimeType === "string" ? entry.mimeType : "application/octet-stream",
data: data,
});
}
normalizedEntries.sort((a, b) => a.relativePath.localeCompare(b.relativePath));
return {
revision: revision,
entries: normalizedEntries,
};
}

function normalizeRelativePath(candidate) {
if (typeof candidate !== "string") {
throw new Error("Include manifest entry is missing a mount path.");
}
let normalized = candidate.replace(/\\/g, "/");
normalized = normalized.replace(/\/+/g, "/");
if (normalized.startsWith("/")) {
normalized = normalized.slice(1);
}
const parts = normalized.split("/");
const segments = [];
for (let index = 0; index < parts.length; ++index) {
const part = parts[index];
if (!part || part === ".") {
continue;
}
if (part === "..") {
throw new Error(`Include path ${candidate} escapes the workspace root.`);
}
segments.push(part);
}
if (segments.length === 0) {
throw new Error("Include manifest entry resolved to an empty path.");
}
return segments.join("/");
}

function decodeAssetData(data) {
const base64 = typeof data === "string" ? data : "";
if (!base64) {
return new Uint8Array(0);
}
if (typeof atob === "function") {
const binary = atob(base64);
const buffer = new Uint8Array(binary.length);
for (let index = 0; index < binary.length; ++index) {
buffer[index] = binary.charCodeAt(index);
}
return buffer;
}
if (typeof Buffer === "function") {
const buffer = Buffer.from(base64, "base64");
return new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
}
throw new Error("Include bundles cannot be decoded: base64 helper unavailable.");
}

function ensureIncludeRoot(fs) {
	if (!fs) {
		return;
	}
	if (typeof fs.mkdirTree === "function") {
		try {
			fs.mkdirTree(INCLUDE_ROOT);
			return;
		} catch (error) {
			if (typeof fs.analyzePath === "function") {
				try {
					const info = fs.analyzePath(INCLUDE_ROOT);
					if (info && info.exists) {
						return;
					}
				} catch (analysisError) {
					// Fall through to manual directory creation.
				}
			}
		}
	}
	const segments = INCLUDE_ROOT.split("/").filter(Boolean);
	let current = "";
	for (let index = 0; index < segments.length; ++index) {
		const segment = segments[index];
		current = current ? `${current}/${segment}` : `/${segment}`;
		try {
			fs.mkdir(current);
			continue;
		} catch (error) {
			if (typeof fs.analyzePath === "function") {
				try {
					const info = fs.analyzePath(current);
					if (info && info.exists) {
						continue;
					}
				} catch (analysisError) {
					// Ignore analyzePath failures and rethrow the original error.
				}
			}
			throw error;
		}
	}
}
function buildIncludePath(relativePath) {
return `${INCLUDE_ROOT}/${relativePath}`;
}

function removeMountedPaths(fs, relativePaths) {
if (!relativePaths || relativePaths.length === 0) {
return;
}
const directories = new Set();
for (let index = 0; index < relativePaths.length; ++index) {
const relative = relativePaths[index];
if (typeof relative !== "string" || relative.length === 0) {
continue;
}
const target = buildIncludePath(relative);
try {
fs.unlink(target);
} catch (error) {
try {
fs.rmdir(target);
} catch (removeError) {
// Ignore cleanup failures; paths will be recreated if needed.
}
}
const dir = parentDirectory(target);
if (dir && dir !== INCLUDE_ROOT) {
directories.add(dir);
}
}
if (directories.size === 0) {
return;
}
const sorted = Array.from(directories).sort((a, b) => b.length - a.length);
for (let index = 0; index < sorted.length; ++index) {
const dir = sorted[index];
try {
if (isDirectoryEmpty(fs, dir)) {
fs.rmdir(dir);
}
} catch (error) {
// Ignore directory cleanup failures.
}
}
}

function removeIncludeRootIfEmpty(fs) {
try {
if (isDirectoryEmpty(fs, INCLUDE_ROOT)) {
fs.rmdir(INCLUDE_ROOT);
}
} catch (error) {
// Ignore cleanup failures.
}
}

function ensureDirectory(fs, directory, created) {
if (!directory || directory === "/" || directory === INCLUDE_ROOT) {
return;
}
const segments = directory.split("/");
let current = "";
for (let index = 0; index < segments.length; ++index) {
const segment = segments[index];
if (!segment) {
continue;
}
current = current ? `${current}/${segment}` : `/${segment}`;
if (current === INCLUDE_ROOT) {
continue;
}
if (created.has(current)) {
continue;
}
try {
const info = fs.analyzePath(current);
if (!info || !info.exists) {
fs.mkdir(current);
}
} catch (error) {
fs.mkdir(current);
}
created.add(current);
}
}

function parentDirectory(path) {
if (typeof path !== "string" || path.length === 0) {
return null;
}
const index = path.lastIndexOf("/");
if (index <= 0) {
return "/";
}
return path.slice(0, index);
}

function isDirectoryEmpty(fs, directory) {
try {
const entries = fs.readdir(directory);
if (!entries) {
return true;
}
for (let index = 0; index < entries.length; ++index) {
const entry = entries[index];
if (entry === "." || entry === "..") {
continue;
}
return false;
}
return true;
} catch (error) {
return false;
}
}

return {
stageBundle: stageBundle,
ensureMounted: ensureMounted,
handleTraceLine: handleTraceLine,
recordMountFailure: recordMountFailure,
getDiagnostics: getDiagnostics,
};
})();

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
			if (traceElement) {
				traceElement.textContent = "";
			}
			if (traceDiv) {
				traceDiv.scrollTop = 0;
			}
		}

function trace(message) {
const line = typeof message === "string" ? message : String(message);
if (!line) {
return;
}
IncludeBundleController.handleTraceLine(line);
while (allLogLines.length > MAX_LOG_SIZE || traceLinesCount >= MAX_LOG_LINES) {
				const offset = allLogLines.indexOf("\n");
				if (offset < 0) {
					break;
				}
				allLogLines = allLogLines.substr(offset + 1);
				--traceLinesCount;
			}
			if (lastLogLine === line) {
				if (repeatingLogLineCount > 1) {
					const offset = allLogLines.lastIndexOf(" *");
					if (offset >= 0) {
						allLogLines = allLogLines.substr(0, offset);
					}
				} else {
					allLogLines = allLogLines.substr(0, allLogLines.length - 1);
				}
				++repeatingLogLineCount;
				allLogLines += " *" + repeatingLogLineCount + "\n";
			} else {
				allLogLines += line + "\n";
				++traceLinesCount;
				lastLogLine = line;
				repeatingLogLineCount = 1;
			}
			if (traceElement) {
				traceElement.textContent = allLogLines;
			}
			if (traceDiv) {
				traceDiv.scrollTop = traceDiv.scrollHeight;
			}
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

		const rasterizeIVG = function rasterizeIVG(source, scaling) {
			const size = global.Module.lengthBytesUTF8(source) + 1;
			const stringPointer = global.Module._malloc(size);
			global.Module.stringToUTF8(source, stringPointer, size);
			const result = global.Module._rasterizeIVG(stringPointer, scaling);
			global.Module._free(stringPointer);
			return result;
		};

function renderCurrentSource() {
if (!global.Module || !ivgCanvas || !ivgContext) {
return;
}
if (renderInProgress) {
rerenderQueued = true;
return;
}
renderInProgress = true;
rerenderQueued = false;
let ok = false;
let start = 0;
try {
const module = global.Module;
let includeResult = null;
try {
includeResult = IncludeBundleController.ensureMounted(module);
} catch (includeError) {
const includeMessage = describeError(includeError);
IncludeBundleController.recordMountFailure(includeMessage);
trace("Failed to mount include bundle: " + includeMessage);
setStatus("Failed to mount include bundle: " + includeMessage, { level: "error" });
throw includeError;
}
if (includeResult && includeResult.changed) {
const count = includeResult.entryCount;
const revision = includeResult.revision || "unknown";
const summary = count === 1 ? "1 asset" : count + " assets";
trace("Include bundle revision " + revision + " mounted (" + summary + ").");
}
clearTrace();
trace("Rasterizing IVG");
const devicePixelRatio = window.devicePixelRatio || 1;
const usesVectorScaling = ZoomController.usesVectorScaling();
const currentZoom = ZoomController.getZoom();
const rasterScale = usesVectorScaling ? currentZoom * devicePixelRatio : ZoomController.getRasterScale(devicePixelRatio);
start = window.performance.now();
const result = rasterizeIVG(currentSource, rasterScale);
if (result === 0) {
trace("Rasterization returned 0");
} else {
const left = readInt32(module, result + 0);
const top = readInt32(module, result + 4);
const width = readInt32(module, result + 8);
const height = readInt32(module, result + 12);
const byteLength = width * height * 4;
const heapU8 = getHeapView(module, "HEAPU8");
if (!heapU8) {
throw new Error("WebAssembly heap unavailable");
}
const pixelOffset = result + 16;
const pixelData = heapU8.slice(pixelOffset, pixelOffset + byteLength);
module._deallocatePixels(result);
if (width > 0 && height > 0 && pixelData.length === byteLength) {
if (ivgCanvas.width !== width || ivgCanvas.height !== height) {
ivgCanvas.width = width;
ivgCanvas.height = height;
}
const imageData = ivgContext.createImageData(width, height);
imageData.data.set(pixelData);
ivgContext.putImageData(imageData, 0, 0);
ZoomController.applyRenderMetrics({
width: width,
height: height,
left: left,
top: top,
rasterScale: rasterScale,
renderZoom: usesVectorScaling ? currentZoom : 1,
});
trace("Completed IVG");
const end = window.performance.now();
trace("Time spent: " + (end - start) + "ms");
setStatus("Preview updated in " + (end - start) + " ms.", {
level: "info",
durationMs: end - start,
});
ok = true;
hasSuccessfulRender = true;
lastSuccessfulRenderUri = currentDocumentUri;
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
} finally {
renderInProgress = false;
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
if (rerenderQueued) {
const shouldRerender = rerenderQueued;
rerenderQueued = false;
if (shouldRerender) {
renderCurrentSource();
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
case "setIncludeBundle": {
let summary;
try {
summary = IncludeBundleController.stageBundle(message);
} catch (stageError) {
const stageMessage = describeError(stageError);
IncludeBundleController.recordMountFailure(stageMessage);
trace("Failed to process include bundle: " + stageMessage);
setStatus("Failed to process include bundle: " + stageMessage, { level: "error" });
break;
}
const stagedCount = summary.entryCount;
const stagedRevision = summary.revision || "unknown";
const stagedSummary = stagedCount === 1 ? "1 asset" : stagedCount + " assets";
trace("Include bundle revision " + stagedRevision + " staged (" + stagedSummary + ").");
if (moduleReady && global.Module && global.Module.FS) {
try {
const mountResult = IncludeBundleController.ensureMounted(global.Module);
if (mountResult && mountResult.changed) {
const mountCount = mountResult.entryCount;
const mountRevision = mountResult.revision || stagedRevision;
const mountSummary = mountCount === 1 ? "1 asset" : mountCount + " assets";
trace("Include bundle revision " + mountRevision + " mounted (" + mountSummary + ").");
}
renderCurrentSource();
} catch (mountError) {
const mountMessage = describeError(mountError);
IncludeBundleController.recordMountFailure(mountMessage);
trace("Failed to mount include bundle: " + mountMessage);
setStatus("Failed to mount include bundle: " + mountMessage, { level: "error" });
}
} else {
trace(
"Renderer not ready; include bundle revision " +
stagedRevision +
" will mount when the preview reconnects.",
);
}
break;
}
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
getIncludeDiagnostics: IncludeBundleController.getDiagnostics,
};
}

	global.IVGFiddlePreview = {
		create: createPreview,
	};
})(typeof window !== "undefined" ? window : this);
