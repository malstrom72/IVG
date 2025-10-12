"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const { initializeIvfFiddleForTests, flushAnimationFrames } = require("./ivgfiddleTestHarness");

function setup() {
	return initializeIvfFiddleForTests();
}

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
	// TODO: expand to assert trace strings like "Vector rescale was disabled" once the harness exposes trace output.
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const storageKeys = context.exports.STORAGE_KEYS;
	zoomController.setVectorScalingEnabled(true, { skipRerender: true });
	assert.equal(context.window.localStorage.getItem(storageKeys.VECTOR_SCALING), "1");
	const disabled = zoomController.handleVectorRasterFailure({ vectorRenderLimit: 1, renderZoom: 2 });
	assert.equal(disabled, true);
	assert.equal(zoomController.isVectorScalingEnabled(), false);
	assert.equal(context.window.localStorage.getItem(storageKeys.VECTOR_SCALING), "1");
	const restored = zoomController.restorePreferredVectorScaling();
	assert.equal(restored, true);
	assert.equal(zoomController.isVectorScalingEnabled(), true);
	flushAnimationFrames(context.window);
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
