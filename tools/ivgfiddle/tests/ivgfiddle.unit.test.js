"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");
const vm = require("node:vm");

const { initializeIvfFiddleForTests, flushAnimationFrames } = require("./ivgfiddleTestHarness");

function setup() {
	return initializeIvfFiddleForTests();
}

function createEvent(type, target, extra) {
	const event = Object.assign(
		{
			type: type,
			target: target,
			defaultPrevented: false,
			preventDefault: function preventDefault() {
				this.defaultPrevented = true;
			},
		},
		extra || {},
	);
	if (!event.target) {
		event.target = target;
	}
	return event;
}

function dispatchElementEvent(element, type, extra) {
	const event = createEvent(type, element, extra);
	element.dispatchEvent(event);
	return event;
}

function dispatchDocumentEvent(context, type, extra) {
	const event = createEvent(type, context.document, extra);
	context.document.dispatchEvent(event);
	return event;
}

test("setupModule assigns runtime before startup run", async () => {
	let loadHandler = null;
	let editorValue = "";
	let didRun = false;
	const runtimeModule = {
		FS: {
			readFile: function readFile() {
				return "demo source";
			},
		},
	};
	const contextObject = {
		console: console,
		localStorage: {
			getItem: function getItem() {
				return null;
			},
		},
		ace: {
			edit: function edit() {
				return {
					setValue: function setValue(value) {
						editorValue = value;
					},
				};
			},
		},
		trace: function trace() {},
		runIVG: function runIVG() {
			didRun = true;
		},
		addEventListener: function addEventListener(type, handler) {
			if (type === "load") {
				loadHandler = handler;
			}
		},
	};
	contextObject.window = contextObject;
	const context = vm.createContext(contextObject);
	const sourcePath = path.resolve(__dirname, "..", "src", "setupModule.js");
	const source = fs.readFileSync(sourcePath, "utf8");
	vm.runInContext(source, context, { filename: "setupModule.js" });
	context.Module = function createModule(moduleConfig) {
		moduleConfig.onRuntimeInitialized.call(runtimeModule);
		return Promise.resolve(runtimeModule);
	};
	assert.equal(typeof loadHandler, "function");
	loadHandler();
	await Promise.resolve();
	assert.equal(context.ivgRuntimeModule, runtimeModule);
	assert.equal(context.Module, runtimeModule);
	assert.equal(editorValue, "demo source");
	assert.equal(didRun, true);
});

test("ZoomController increments to next preset", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const storageKeys = context.exports.STORAGE_KEYS;
	zoomController.incrementZoom(1);
	assert.equal(zoomController.getZoom(), 1.5);
	assert.equal(context.elements.zoomLevelSelect.value, "150");
	assert.equal(context.elements.ivgCanvas.style.transform, "scale(1.5)");
	assert.equal(context.window.localStorage.getItem(storageKeys.ZOOM_LEVEL), "1.50");
});

test("Toolbar controls exist and respond to clicks", () => {
	const context = setup();
	const elements = context.elements;
	const zoomController = context.exports.ZoomController;
	assert.equal(elements.canvasToolbar.getAttribute("data-toolbar-ready"), "true");
	assert.ok(elements.zoomOutButton);
	assert.ok(elements.zoomInButton);
	assert.ok(elements.zoomResetButton);
	assert.ok(elements.zoomLevelSelect);
	assert.ok(elements.vectorScalingToggle);
	assert.ok(elements.backgroundButton);
	assert.ok(elements.zoomLevelSelect.children.length >= 11);
	dispatchElementEvent(elements.zoomInButton, "click");
	assert.equal(zoomController.getZoom(), 1.5);
	dispatchElementEvent(elements.zoomOutButton, "click");
	assert.equal(zoomController.getZoom(), 1);
	elements.zoomLevelSelect.value = "300";
	dispatchElementEvent(elements.zoomLevelSelect, "change");
	assert.equal(zoomController.getZoom(), 3);
	dispatchElementEvent(elements.zoomResetButton, "click");
	assert.equal(zoomController.getZoom(), 1);
	dispatchElementEvent(elements.vectorScalingToggle, "click");
	assert.equal(zoomController.isVectorScalingEnabled(), true);
});

test("ZoomController clamps to maximum preset", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	zoomController.setZoom(42);
	assert.equal(zoomController.getZoom(), 10);
	assert.equal(context.elements.zoomLevelSelect.value, "1000");
	assert.equal(context.elements.zoomInButton.disabled, true);
});

test("Vector scaling toggle updates label and state", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const toggle = context.elements.vectorScalingToggle;
	assert.equal(toggle.textContent, "Bitmap zoom");
	zoomController.setVectorScalingEnabled(true, { skipRerender: true });
	assert.equal(toggle.textContent, "Vector zoom");
	assert.equal(toggle.getAttribute("aria-pressed"), "true");
	zoomController.setVectorScalingEnabled(false, { skipRerender: true });
	assert.equal(toggle.textContent, "Bitmap zoom");
	assert.equal(toggle.getAttribute("aria-pressed"), "false");
});

test("Zoom keyboard shortcuts use toolbar fallbacks and skip editor focus", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const zoomInEvent = dispatchDocumentEvent(context, "keydown", {
		ctrlKey: true,
		metaKey: false,
		altKey: false,
		key: "+",
	});
	assert.equal(zoomInEvent.defaultPrevented, true);
	assert.equal(zoomController.getZoom(), 1.5);
	context.elements.editor.focus();
	const editorEvent = dispatchDocumentEvent(context, "keydown", {
		ctrlKey: true,
		metaKey: false,
		altKey: false,
		key: "+",
	});
	assert.equal(editorEvent.defaultPrevented, false);
	assert.equal(zoomController.getZoom(), 1.5);
	context.elements.zoomLevelSelect.focus();
	const selectEvent = dispatchDocumentEvent(context, "keydown", {
		ctrlKey: true,
		metaKey: false,
		altKey: false,
		key: "-",
	});
	assert.equal(selectEvent.defaultPrevented, false);
	assert.equal(zoomController.getZoom(), 1.5);
});

test("BackgroundController applies palette colors", () => {
	const context = setup();
	const backgroundController = context.exports.BackgroundController;
	const storageKeys = context.exports.STORAGE_KEYS;
	backgroundController.applyColor("black");
	assert.equal(context.window.localStorage.getItem(storageKeys.BACKGROUND_COLOR), "black");
	assert.equal(context.elements.rightPanel.classList.contains("transparent"), false);
	assert.equal(context.elements.screen.classList.contains("transparent"), false);
	assert.equal(context.elements.ivgCanvas.style.backgroundColor, "#000000");
	assert.notEqual(context.elements.rightPanel.style.backgroundColor, context.elements.ivgCanvas.style.backgroundColor);
});

test("Background popup opens and swatches respond to clicks", () => {
	const context = setup();
	const elements = context.elements;
	const storageKeys = context.exports.STORAGE_KEYS;
	dispatchElementEvent(elements.backgroundButton, "click");
	assert.equal(elements.backgroundOverlay.classList.contains("is-hidden"), false);
	assert.equal(elements.backgroundOverlay.getAttribute("aria-hidden"), "false");
	assert.equal(elements.backgroundButton.getAttribute("aria-expanded"), "true");
	const blackSwatch = elements.backgroundSwatchContainer.querySelector('[data-background="black"]');
	assert.ok(blackSwatch);
	dispatchElementEvent(elements.backgroundSwatchContainer, "click", { target: blackSwatch });
	assert.equal(elements.backgroundOverlay.classList.contains("is-hidden"), true);
	assert.equal(elements.backgroundOverlay.getAttribute("aria-hidden"), "true");
	assert.equal(elements.backgroundButton.getAttribute("aria-expanded"), "false");
	assert.equal(context.window.localStorage.getItem(storageKeys.BACKGROUND_COLOR), "black");
	assert.equal(elements.ivgCanvas.style.backgroundColor, "#000000");
});

test("BackgroundController resets to none", () => {
	const context = setup();
	const backgroundController = context.exports.BackgroundController;
	const storageKeys = context.exports.STORAGE_KEYS;
	backgroundController.applyColor("black");
	backgroundController.applyColor("none");
	assert.equal(context.window.localStorage.getItem(storageKeys.BACKGROUND_COLOR), "none");
	assert.equal(context.elements.rightPanel.classList.contains("transparent"), true);
	assert.equal(context.elements.screen.classList.contains("transparent"), true);
	assert.equal(context.elements.ivgCanvas.style.backgroundColor || "", "");
	assert.equal(context.elements.backgroundButton.getAttribute("aria-label"), "Change canvas background (current: None)");
});

test("Vector scaling updates canvas attributes", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const canvas = context.elements.ivgCanvas;
	zoomController.setZoom(2, { skipPersist: true });
	zoomController.setVectorScalingEnabled(true, { skipRerender: true });
	assert.equal(zoomController.isVectorScalingEnabled(), true);
	assert.equal(canvas.getAttribute("data-scaling-mode"), "vector");
	zoomController.setVectorScalingEnabled(false, { skipRerender: true });
	assert.equal(zoomController.isVectorScalingEnabled(), false);
	assert.equal(canvas.getAttribute("data-scaling-mode"), "bitmap");
	assert.ok(String(canvas.style.transform).includes("scale(2"));
});

test("Vector raster failure queues bitmap fallback", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	zoomController.setVectorScalingEnabled(true, { skipRerender: true });
	const disabled = zoomController.handleVectorRasterFailure({ vectorRenderLimit: 1, renderZoom: 2 });
	assert.equal(disabled, true);
	assert.equal(zoomController.isVectorScalingEnabled(), false);
	flushAnimationFrames(context.window);
});

test("Vector raster failure trace explains bitmap fallback", () => {
	const context = setup();
	context.window.Module = {
		HEAPU8: { buffer: new ArrayBuffer(64 * 1024 * 1024) },
		lengthBytesUTF8: function lengthBytesUTF8(source) {
			return String(source).length;
		},
		_malloc: function malloc() {
			return 1;
		},
		stringToUTF8: function stringToUTF8() {},
		_rasterizeIVG: function rasterizeIVG() {
			return 0;
		},
		_free: function free() {},
		_deallocatePixels: function deallocatePixels() {},
	};
	const zoomController = context.exports.ZoomController;
	zoomController.setZoom(2, { skipPersist: true });
	zoomController.setVectorScalingEnabled(true, { skipRerender: true });
	context.exports.runIVG("unit-vector-failure");
	assert.match(context.exports.getTraceText(), /Vector rescale was disabled after a failed rasterization - falling back to bitmap zoom\./);
	assert.equal(zoomController.isVectorScalingEnabled(), false);
});

test("runIVG uses captured runtime when Module is factory", () => {
	const context = setup();
	let didRasterize = false;
	context.window.Module = function createModule() {};
	context.window.ivgRuntimeModule = {
		HEAPU8: { buffer: new ArrayBuffer(64 * 1024 * 1024) },
		lengthBytesUTF8: function lengthBytesUTF8(source) {
			return String(source).length;
		},
		_malloc: function malloc() {
			return 1;
		},
		stringToUTF8: function stringToUTF8() {},
		_rasterizeIVG: function rasterizeIVG() {
			didRasterize = true;
			return 0;
		},
		_free: function free() {},
		_deallocatePixels: function deallocatePixels() {},
	};
	context.exports.runIVG("unit-runtime-module");
	assert.equal(didRasterize, true);
	assert.match(context.exports.getTraceText(), /Aborted IVG/);
	assert.doesNotMatch(context.exports.getTraceText(), /WebAssembly rasterizer is not initialized|Rasterization crashed/);
});

test("estimateVectorPixelBudget honors heap reserve", () => {
	const context = setup();
	context.window.Module = {
		HEAPU8: { buffer: { byteLength: 64 * 1024 * 1024 } },
	};
	const defaultPixels = context.exports.estimateVectorPixelBudget();
	assert.equal(defaultPixels, Math.floor((64 * 1024 * 1024 - 12 * 1024 * 1024) / 4));
	context.window.Module._getFreeHeapBytes = function () {
		return 8 * 1024 * 1024;
	};
	assert.equal(context.exports.estimateVectorPixelBudget(), defaultPixels);
	context.window.Module._getFreeHeapBytes = function () {
		return 20 * 1024 * 1024;
	};
	assert.equal(context.exports.estimateVectorPixelBudget(), Math.floor((20 * 1024 * 1024 - 12 * 1024 * 1024) / 4));
});
