"use strict";

(function createIvfPreviewModule(global) {
function createHostAdapter(adapter) {
const noop = function noop() {};
const notifyStatus = adapter && typeof adapter.notifyStatus === "function" ? adapter.notifyStatus : noop;
const onReady = adapter && typeof adapter.onReady === "function" ? adapter.onReady : noop;
return {
notifyStatus: notifyStatus,
onReady: onReady
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

function rgbToHex(rgb) {
if (!rgb || typeof rgb !== "object") {
return null;
}
const r = Math.max(0, Math.min(255, Math.round(rgb.r))) | 0;
const g = Math.max(0, Math.min(255, Math.round(rgb.g))) | 0;
const b = Math.max(0, Math.min(255, Math.round(rgb.b))) | 0;
return (
"#" +
(r.toString(16).padStart(2, "0")) +
(g.toString(16).padStart(2, "0")) +
(b.toString(16).padStart(2, "0"))
);
}

function formatCustomColorOption(rgb) {
const hex = rgbToHex(rgb);
if (hex === null) {
return {
value: "custom",
label: "Custom",
preview: "#121212"
};
}
return {
value: "custom",
label: hex.toUpperCase(),
preview: hex
};
}

function readComputedBackground(element) {
if (element === null) {
return null;
}
const styles = window.getComputedStyle(element);
const backgroundColor = styles ? styles.getPropertyValue("background-color") : "";
if (!backgroundColor) {
return null;
}
const rgbRegex = /^rgba?\((\d+),\s*(\d+),\s*(\d+)/i;
const match = backgroundColor.match(rgbRegex);
if (!match) {
return null;
}
return {
r: Number.parseInt(match[1], 10) || 0,
g: Number.parseInt(match[2], 10) || 0,
b: Number.parseInt(match[3], 10) || 0
};
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
swatch.setAttribute("aria-label", definition.label);
swatch.dataset.value = definition.value;
swatch.innerHTML =
'<span class="background-swatch__color" aria-hidden="true" style="background:' +
definition.preview + '"></span>' +
'<span class="background-swatch__label">' + definition.label + "</span>";
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
const value = element.dataset.value || "";
const isSelected = value === currentColor;
if (isSelected) {
element.classList.add("is-selected");
element.setAttribute("aria-selected", "true");
if (document.activeElement === element) {
element.focus();
}
} else {
element.classList.remove("is-selected");
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
const currentBackground = readComputedBackground(screenElement) || readComputedBackground(previewContainer);
if (currentBackground !== null) {
fragment.appendChild(buildSwatch(formatCustomColorOption(currentBackground)));
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
refreshSelectedSwatch: refreshSelectedSwatch
};
})();

const ZOOM_SELECT_PRESETS = Object.freeze([0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8, 10]);
const ZOOM_PRESETS = ZOOM_SELECT_PRESETS;
const CUSTOM_ZOOM_OPTION_VALUE = "custom";
const ZOOM_CONSTANTS = Object.freeze({
MIN: ZOOM_PRESETS[0],
MAX: ZOOM_PRESETS[ZOOM_PRESETS.length - 1],
STEP: 1,
DEFAULT: 1.0
});

const ZoomController = (function createZoomController() {
let currentZoom = ZOOM_CONSTANTS.DEFAULT;
let baseMetrics = null;
let vectorScalingEnabled = false;
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
Settings.write(STORAGE_KEYS.VECTOR_SCALING, vectorScalingEnabled ? "1" : "0");
}

function applyZoom() {
if (ivgCanvas === null) {
return;
}
ivgCanvas.setAttribute("data-scaling-mode", vectorScalingEnabled ? "vector" : "bitmap");
if (baseMetrics === null) {
ivgCanvas.style.transformOrigin = "top left";
if (vectorScalingEnabled) {
ivgCanvas.style.transform = "translate(0px, 0px)";
} else {
ivgCanvas.style.transform = "scale(" + currentZoom + ")";
}
return;
}
const metrics = baseMetrics;
const targetZoom = vectorScalingEnabled ? currentZoom : 1;
ivgCanvas.style.width = metrics.width * targetZoom + "px";
ivgCanvas.style.height = metrics.height * targetZoom + "px";
ivgCanvas.style.transformOrigin = "top left";
if (vectorScalingEnabled) {
ivgCanvas.style.transform = "translate(" + metrics.translateX * targetZoom + "px," +
metrics.translateY * targetZoom + "px)";
} else {
ivgCanvas.style.transform = "translate(" + metrics.translateX + "px," + metrics.translateY +
"px) scale(" + currentZoom + ")";
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

function incrementZoom(deltaSteps) {
const direction = deltaSteps > 0 ? 1 : -1;
const target = snapToPreset(currentZoom + direction * ZOOM_CONSTANTS.STEP, direction);
setZoom(target);
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
vectorScalingEnabled = storedVector === "1";
reflectVectorScalingState();
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
vectorScalingEnabled = !vectorScalingEnabled;
persistVectorScaling();
reflectVectorScalingState();
if (rerenderCallback) {
rerenderCallback("vector-toggle");
}
});
});
withElement(zoomLevelSelect, function attachZoomSelect(select) {
select.addEventListener("change", handleZoomSelectChange);
});
window.addEventListener("keydown", function handleZoomKeydown(event) {
if (!event.ctrlKey && !event.metaKey) {
return;
}
if (event.key === "=") {
event.preventDefault();
incrementZoom(1);
} else if (event.key === "-") {
event.preventDefault();
incrementZoom(-1);
} else if (event.key === "0") {
event.preventDefault();
resetZoom();
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
rasterScale: rasterScale
};
applyZoom();
}

function clearMetrics() {
baseMetrics = null;
}

function getRasterScale(pixelRatio) {
if (vectorScalingEnabled) {
return 1;
}
return Math.max(1, Math.round(currentZoom * pixelRatio));
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
applyRenderMetrics: setBaseMetrics,
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
if (!ivgContext || !ivgCanvas) {
return;
}
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
trace("Rasterizing IVG");
const start = window.performance.now();
let ok = false;
try {
const devicePixelRatio = window.devicePixelRatio || 1;
const usesVectorScaling = ZoomController.usesVectorScaling();
const currentZoom = ZoomController.getZoom();
const rasterScale = usesVectorScaling
? currentZoom * devicePixelRatio
: ZoomController.getRasterScale(devicePixelRatio);
const result = rasterizeIVG(currentSource, rasterScale);
if (result === 0) {
trace("Rasterization returned 0");
} else {
const width = global.Module.getValue(result + 0, "i32");
const height = global.Module.getValue(result + 4, "i32");
const left = global.Module.getValue(result + 8, "i32");
const top = global.Module.getValue(result + 12, "i32");
const pixelDataPointer = global.Module.getValue(result + 16, "i32");
const byteLength = width * height * 4;
const pixelData = global.Module.HEAPU8.slice(pixelDataPointer, pixelDataPointer + byteLength);
global.Module._deallocatePixels(result);
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
renderZoom: usesVectorScaling ? currentZoom : 1
});
trace("Completed IVG");
const end = window.performance.now();
trace("Time spent: " + (end - start) + "ms");
setStatus("Preview updated in " + (end - start) + " ms.", {
level: "info",
durationMs: end - start
});
ok = true;
} else {
trace("Rasterization returned no data");
}
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

function handleHostCommand(message) {
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
host.onReady();
}

return {
initialize: initialize,
handleModuleInitialized: handleModuleInitialized,
handleHostCommand: handleHostCommand,
setSource: setSource,
clearTrace: clearTrace,
renderCurrentSource: renderCurrentSource
};
}

global.IVGFiddlePreview = {
create: createPreview
};
})(typeof window !== "undefined" ? window : this);
