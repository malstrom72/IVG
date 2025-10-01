"use strict";

const MAX_LOG_SIZE = 64 * 1024;
const MAX_LOG_LINES = 1000;

const statusElement = document.getElementById("status");
const traceElement = document.getElementById("trace");
const traceDiv = document.getElementById("traceDiv");
const ivgCanvas = document.getElementById("ivgCanvas");
const ivgContext = ivgCanvas.getContext("2d");
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

const vscodeApi = typeof acquireVsCodeApi === "function" ? acquireVsCodeApi() : undefined;

const STORAGE_KEYS = Object.freeze({
	SOURCE: "ivgSource",
	ZOOM_LEVEL: "ivgZoomLevel",
	VECTOR_SCALING: "ivgVectorScaling",
	BACKGROUND_COLOR: "ivgBackgroundColor"
});

function withElement(element, callback) {
	if (element !== null && typeof callback === "function") {
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
			if (value === null || value === undefined) {
				localStorage.removeItem(key);
			} else {
				localStorage.setItem(key, value);
			}
		} catch (error) {
			// Ignore storage failures; they are non-fatal for the preview UI.
		}
	}

	return {
		read: read,
		write: write
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
	{ value: "none", label: "None", preview: "#121212" }
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
			b: adjustComponent(components.b)
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
			button.setAttribute("title", "Change canvas background");
		});
	}

	function applyColorToDOM() {
		const shouldClearColor = currentColor === "none";
		const definition = getColorDefinition(currentColor);
		const paletteColor = definition && definition.preview ? definition.preview : currentColor;
		const outerColor = computeOuterBackground(definition) || paletteColor;
		withElement(previewContainer, function updatePreview(element) {
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
			if (!defaultBodyBackground) {
				const computed = window.getComputedStyle(element);
				defaultBodyBackground = computed ? computed.backgroundColor : "";
			}
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

	function open() {
		if (isOpen || backgroundOverlay === null || backgroundDialog === null) {
			return;
		}
		isOpen = true;
		lastTrigger = document.activeElement;
		backgroundOverlay.classList.remove("is-hidden");
		backgroundOverlay.setAttribute("aria-hidden", "false");
		withElement(backgroundButton, function updateButton(button) {
			button.setAttribute("aria-expanded", "true");
		});
		window.requestAnimationFrame(function focusSwatch() {
			backgroundDialog.focus({ preventScroll: true });
			focusCurrentSwatch();
		});
	}

	function close() {
		if (!isOpen || backgroundOverlay === null) {
			return;
		}
		isOpen = false;
		backgroundOverlay.classList.add("is-hidden");
		backgroundOverlay.setAttribute("aria-hidden", "true");
		withElement(backgroundButton, function updateButton(button) {
			button.setAttribute("aria-expanded", "false");
		});
		if (lastTrigger && typeof lastTrigger.focus === "function") {
			lastTrigger.focus();
		}
		lastTrigger = null;
	}

	function init() {
		const storedColor = Settings.read(STORAGE_KEYS.BACKGROUND_COLOR, BACKGROUND_DEFAULT) || BACKGROUND_DEFAULT;
		applyColor(storedColor, { force: true, skipPersist: true });
		createSwatches();
		updateSelectionUI();
		updateTriggerLabel();
		if (backgroundButton) {
			backgroundButton.addEventListener("click", function handleBackgroundButton(event) {
				event.preventDefault();
				if (isOpen) {
					close();
				} else {
					open();
				}
			});
		}
		if (backgroundCloseButton) {
			backgroundCloseButton.addEventListener("click", function handleBackgroundClose(event) {
				event.preventDefault();
				close();
			});
		}
		if (backgroundOverlay) {
			backgroundOverlay.addEventListener("click", handleOverlayClick);
		}
		if (backgroundSwatchContainer) {
			backgroundSwatchContainer.addEventListener("click", handleSwatchClick);
			backgroundSwatchContainer.addEventListener("keydown", handleSwatchKeydown);
		}
		document.addEventListener("keydown", handleOverlayKeydown);
	}

	return {
		init: init,
		applyColor: applyColor
	};
})();

const ZOOM_PRESETS = Object.freeze([0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10]);

const CUSTOM_ZOOM_OPTION_VALUE = "custom";

const ZOOM_CONSTANTS = Object.freeze({
	MIN: ZOOM_PRESETS[0],
	MAX: ZOOM_PRESETS[ZOOM_PRESETS.length - 1],
	STEP: 1,
	DEFAULT: 1
});

const ZoomController = (function createZoomController() {
	let currentZoom = ZOOM_CONSTANTS.DEFAULT;
	let vectorScalingEnabled = false;
	let rerenderCallback = null;
	let lastMetrics = null;
	const ZOOM_EPSILON = 0.0001;

	function clampZoom(value) {
		const numeric = Number(value);
		if (!Number.isFinite(numeric)) {
			return ZOOM_CONSTANTS.DEFAULT;
		}
		return Math.min(ZOOM_CONSTANTS.MAX, Math.max(ZOOM_CONSTANTS.MIN, numeric));
	}

	function zoomToPercent(value) {
		return Math.round(value * 100);
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

	function populateZoomOptions() {
		if (zoomLevelSelect === null) {
			return;
		}
		zoomLevelSelect.innerHTML = "";
		for (let index = 0; index < ZOOM_PRESETS.length; ++index) {
			const preset = ZOOM_PRESETS[index];
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
		withElement(zoomResetButton, function disableReset(button) {
			button.disabled = Math.abs(currentZoom - ZOOM_CONSTANTS.DEFAULT) < ZOOM_EPSILON;
		});
		reflectVectorScalingState();
	}

	function persistZoom() {
		Settings.write(STORAGE_KEYS.ZOOM_LEVEL, currentZoom.toFixed(2));
	}

	function persistVectorScaling() {
		Settings.write(STORAGE_KEYS.VECTOR_SCALING, vectorScalingEnabled ? "1" : "0");
	}

	function updateCanvasDisplay() {
		if (!ivgCanvas) {
			return;
		}
		if (lastMetrics === null) {
			ivgCanvas.style.width = "";
			ivgCanvas.style.height = "";
			ivgCanvas.style.transform = "translate(0px, 0px)";
			ivgCanvas.setAttribute("data-scaling-mode", vectorScalingEnabled ? "vector" : "bitmap");
			return;
		}
		const metrics = lastMetrics;
		const displayZoom = currentZoom;
		ivgCanvas.style.width = metrics.baseWidth * displayZoom + "px";
		ivgCanvas.style.height = metrics.baseHeight * displayZoom + "px";
		ivgCanvas.style.transform = "translate(" + metrics.baseLeft * displayZoom + "px," + metrics.baseTop * displayZoom + "px)";
		ivgCanvas.setAttribute("data-scaling-mode", vectorScalingEnabled ? "vector" : "bitmap");
	}

	function clearMetrics() {
		lastMetrics = null;
		updateCanvasDisplay();
	}

	function applyRenderMetrics(metrics) {
		if (!metrics) {
			clearMetrics();
			return;
		}
		const renderZoom = metrics.renderZoom > 0 ? metrics.renderZoom : 1;
		lastMetrics = {
			baseWidth: metrics.width / metrics.pixelRatio / renderZoom,
			baseHeight: metrics.height / metrics.pixelRatio / renderZoom,
			baseLeft: metrics.left / metrics.pixelRatio / renderZoom,
			baseTop: metrics.top / metrics.pixelRatio / renderZoom,
			renderZoom: renderZoom
		};
		updateCanvasDisplay();
	}

	function requestRerender(reason) {
		if (typeof rerenderCallback === "function") {
			rerenderCallback(reason);
		}
	}

	function setZoom(value, options) {
		const clamped = clampZoom(value);
		if (Math.abs(clamped - currentZoom) < ZOOM_EPSILON) {
			reflectUIState();
			if (!vectorScalingEnabled) {
				updateCanvasDisplay();
			}
			return;
		}
		currentZoom = clamped;
		reflectUIState();
		if (!options || options.skipPersist !== true) {
			persistZoom();
		}
		if (vectorScalingEnabled) {
			requestRerender("zoom change");
		} else {
			updateCanvasDisplay();
		}
	}

	function incrementZoom(deltaSteps) {
		const direction = deltaSteps > 0 ? 1 : -1;
		const target = snapToPreset(currentZoom + direction * ZOOM_CONSTANTS.STEP, direction);
		setZoom(target);
	}

	function resetZoom() {
		setZoom(ZOOM_CONSTANTS.DEFAULT);
	}

	function setVectorScaling(enabled) {
		const next = !!enabled;
		if (next === vectorScalingEnabled) {
			return;
		}
		vectorScalingEnabled = next;
		reflectUIState();
		persistVectorScaling();
		if (vectorScalingEnabled) {
			requestRerender("vector scaling enabled");
		} else {
			updateCanvasDisplay();
		}
	}

	function toggleVectorScaling() {
		setVectorScaling(!vectorScalingEnabled);
	}

	function handleZoomSelectChange(event) {
		setZoom(percentToZoom(event.target.value));
	}

	function handleKeydown(event) {
		if (event.defaultPrevented) {
			return;
		}
		if (event.altKey || !(event.ctrlKey || event.metaKey)) {
			return;
		}
		const key = event.key;
		if (key === "+" || key === "=") {
			event.preventDefault();
			incrementZoom(1);
			return;
		}
		if (key === "-" || key === "_") {
			event.preventDefault();
			incrementZoom(-1);
			return;
		}
		if (key === "0") {
			event.preventDefault();
			resetZoom();
		}
	}

	function attachEventHandlers() {
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
			button.addEventListener("click", function handleVectorToggle(event) {
				event.preventDefault();
				toggleVectorScaling();
			});
		});
		if (zoomLevelSelect) {
			zoomLevelSelect.addEventListener("change", handleZoomSelectChange);
		}
		if (canvasToolbarElement) {
			canvasToolbarElement.addEventListener("wheel", function handleToolbarWheel(event) {
				if (!event.ctrlKey && !event.metaKey) {
					return;
				}
				event.preventDefault();
				const delta = event.deltaY > 0 ? -1 : 1;
				incrementZoom(delta);
			}, { passive: false });
		}
		window.addEventListener("keydown", handleKeydown, true);
	}

	function init(onRerender) {
		rerenderCallback = typeof onRerender === "function" ? onRerender : null;
		if (ivgCanvas) {
			ivgCanvas.style.transformOrigin = "top left";
		}
		const storedZoom = Number.parseFloat(Settings.read(STORAGE_KEYS.ZOOM_LEVEL, String(ZOOM_CONSTANTS.DEFAULT)));
		if (Number.isFinite(storedZoom)) {
			currentZoom = clampZoom(storedZoom);
		}
		vectorScalingEnabled = Settings.read(STORAGE_KEYS.VECTOR_SCALING, "0") === "1";
		populateZoomOptions();
		reflectUIState();
		attachEventHandlers();
		updateCanvasDisplay();
	}

	function getRasterScale(pixelRatio) {
		const base = vectorScalingEnabled ? currentZoom : 1;
		return pixelRatio * base;
	}

	function usesVectorScaling() {
		return vectorScalingEnabled;
	}

	function getZoom() {
		return currentZoom;
	}

	return {
		init: init,
		setZoom: setZoom,
		resetZoom: resetZoom,
		applyRenderMetrics: applyRenderMetrics,
		clearMetrics: clearMetrics,
		getRasterScale: getRasterScale,
		usesVectorScaling: usesVectorScaling,
		getZoom: getZoom
	};
})();

let allLogLines = "";
let traceLinesCount = 0;
let lastLogLine = null;
let repeatingLogLineCount = 0;
let moduleReady = false;
let currentSource = "";
let pendingSource = null;

function postStatus(level, message, options) {
	if (!vscodeApi) {
		return;
	}
	const text = typeof message === "string" ? message : "";
	if (!text) {
		return;
	}
	const payload = {
		type: "status",
		level: typeof level === "string" ? level : "info",
		message: text
	};
	if (options && typeof options.durationMs === "number" && options.durationMs >= 0) {
		payload.durationMs = options.durationMs;
	}
	vscodeApi.postMessage(payload);
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
	postStatus(opts.level, text, opts);
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
			allLogLines = allLogLines.substr(0, allLogLines.length - 1);
		}
		++repeatingLogLineCount;
		allLogLines += " *" + repeatingLogLineCount + "\n";
	} else {
		allLogLines += message + "\n";
		++traceLinesCount;
		lastLogLine = message;
		repeatingLogLineCount = 1;
	}
	if (traceElement) {
		traceElement.textContent = allLogLines;
	}
	if (traceDiv) {
		traceDiv.scrollTop = traceDiv.scrollHeight;
	}
}

function drawFailureCross() {
	ivgContext.clearRect(0, 0, ivgCanvas.width, ivgCanvas.height);
	ivgContext.beginPath();
	ivgContext.moveTo(0, 0);
	ivgContext.lineTo(ivgCanvas.width, ivgCanvas.height);
	ivgContext.moveTo(0, ivgCanvas.height);
	ivgContext.lineTo(ivgCanvas.width, 0);
	ivgContext.strokeStyle = "red";
	ivgContext.lineWidth = 6;
	ivgContext.stroke();
}

const rasterizeIVG = function(source, scaling) {
	const size = Module.lengthBytesUTF8(source) + 1;
	const stringPointer = Module._malloc(size);
	Module.stringToUTF8(source, stringPointer, size);
	const result = Module._rasterizeIVG(stringPointer, scaling);
	Module._free(stringPointer);
	return result;
};

function deallocatePixels(pixelsPointer) {
	Module._deallocatePixels(pixelsPointer);
}

function heapU32(moduleInstance) {
	if (moduleInstance.HEAPU32) {
		return moduleInstance.HEAPU32;
	}
	const mem =
	moduleInstance.wasmMemory ||
	(moduleInstance.asm && moduleInstance.asm.memory) ||
	moduleInstance.memory;
	if (!mem) {
		throw new Error("No wasm memory found on Module");
	}
	return new Uint32Array(mem.buffer);
}

function renderCurrentSource() {
	if (!moduleReady) {
		return;
	}
	clearTrace();
	if (!currentSource) {
		ivgContext.clearRect(0, 0, ivgCanvas.width, ivgCanvas.height);
		ZoomController.clearMetrics();
		setStatus("Renderer ready. Waiting for IVG data…", {
			level: "info"
		});
		return;
	}
	trace("Running IVG");
	const start = Date.now();
	let ok = false;
	try {
		const pixelRatio = window.devicePixelRatio || 1;
		const rasterScale = ZoomController.getRasterScale(pixelRatio);
		const rasterPointer = rasterizeIVG(currentSource, rasterScale);
		const end = Date.now();
		if (rasterPointer !== 0) {
			const heap = heapU32(Module).buffer;
			let dimensions = new Int32Array(heap, rasterPointer, 4);
			const left = dimensions[0];
			const top = dimensions[1];
			const width = dimensions[2];
			const height = dimensions[3];
			dimensions = null;
			let pixelData = new Uint8Array(heap, rasterPointer + 16, width * height * 4);
			deallocatePixels(rasterPointer);
			ivgCanvas.width = width;
			ivgCanvas.height = height;
			const imageData = ivgContext.createImageData(width, height);
			imageData.data.set(pixelData);
			pixelData = null;
			ivgContext.putImageData(imageData, 0, 0);
			ZoomController.applyRenderMetrics({
				width: width,
				height: height,
				left: left,
				top: top,
				pixelRatio: pixelRatio,
				renderZoom: ZoomController.usesVectorScaling() ? ZoomController.getZoom() : 1
			});
			trace("Completed IVG");
			trace("Time spent: " + (end - start) + "ms");
			setStatus("Preview updated in " + (end - start) + " ms.", {
				level: "info",
				durationMs: end - start
			});
			ok = true;
		} else {
			trace("Rasterization returned no data");
		}
	} catch (error) {
		trace("Rasterization crashed");
		trace(String(error));
	}
	if (!ok) {
		ZoomController.clearMetrics();
		drawFailureCross();
		setStatus("Rendering failed. Check trace output for details.", {
			level: "error"
		});
	}
}

function setSource(newSource, options) {
	const opts = options || {};
	const persist = opts.persist !== false;
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

function handleHostMessage(message) {
	if (!message || typeof message !== "object") {
		return;
	}
	switch (message.type) {
		case "setSource":
		if (!moduleReady) {
			pendingSource = typeof message.source === "string" ? message.source : "";
		} else {
			setSource(message.source, { persist: false });
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

window.addEventListener("message", function receiveHostMessage(event) {
	handleHostMessage(event.data);
});

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

window.ivgPreviewModuleInitialized = function(initialSource) {
	moduleReady = true;
	const stored = Settings.read(STORAGE_KEYS.SOURCE, "");
	if (typeof stored === "string" && stored.length > 0) {
		currentSource = stored;
	} else if (typeof initialSource === "string" && initialSource.length > 0) {
		currentSource = initialSource;
		Settings.write(STORAGE_KEYS.SOURCE, currentSource);
	}
	const queuedSource = pendingSource;
	pendingSource = null;
	setStatus("Renderer ready.", { level: "info" });
	if (typeof queuedSource === "string") {
		setSource(queuedSource, { persist: false });
	} else {
		renderCurrentSource();
	}
	if (vscodeApi) {
		vscodeApi.postMessage({ type: "ready" });
	}
};

const cachedSource = Settings.read(STORAGE_KEYS.SOURCE, "");
if (typeof cachedSource === "string" && cachedSource.length > 0) {
	currentSource = cachedSource;
	setStatus("Loaded cached IVG source. Waiting for renderer…", { level: "info" });
} else {
	setStatus("Waiting for renderer…", { level: "info" });
}
