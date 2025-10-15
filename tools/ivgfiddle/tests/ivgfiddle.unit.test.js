"use strict";

const assert = require("node:assert/strict");
const test = require("node:test");

const { initializeIvfFiddleForTests } = require("./ivgfiddleTestHarness");

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

test("Snapshot toolbar hidden without catalog", () => {
				const context = setup();
				const group = context.elements.snapshotToolbarGroup;
				assert.ok(group.classList.contains("is-hidden"));
				const select = context.elements.snapshotScenarioSelect;
				assert.equal(select.disabled, true);
});

test("SnapshotController populates toolbar from catalog", () => {
const context = setup();
const controller = context.exports.SnapshotController;
const group = context.elements.snapshotToolbarGroup;
const select = context.elements.snapshotScenarioSelect;
const catalog = {
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
scenarios: [
{ index: 0, name: "Base", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
{ index: 1, name: "Alt", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
],
};
controller.applyRenderResult({
catalogJson: JSON.stringify(catalog),
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
});
assert.equal(group.classList.contains("is-hidden"), false);
assert.equal(select.disabled, false);
assert.equal(select.children.length, 2);
assert.equal(select.value, "0:1");
});

test("SnapshotController falls back when selection missing", () => {
const context = setup();
const controller = context.exports.SnapshotController;
const select = context.elements.snapshotScenarioSelect;
const initialCatalog = {
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
scenarios: [
{ index: 0, name: "Base", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
{ index: 1, name: "Variant", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
],
};
controller.applyRenderResult({
catalogJson: JSON.stringify(initialCatalog),
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
});
assert.equal(select.value, "0:1");
assert.equal(controller.handleSelectionChange("1:1"), true);
const fallbackCatalog = {
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
scenarios: [
{ index: 0, name: "Base", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
],
};
controller.applyRenderResult({
catalogJson: JSON.stringify(fallbackCatalog),
defaultScenarioIndex: 0,
defaultEntryOrdinal: 1,
});
assert.equal(select.children.length, 1);
assert.equal(select.value, "0:1");
});

test("SnapshotController caches catalog and selection per signature", () => {
	const context = setup();
	const controller = context.exports.SnapshotController;
	const select = context.elements.snapshotScenarioSelect;
	const catalog = {
		defaultScenarioIndex: 0,
		defaultEntryOrdinal: 1,
		scenarios: [
			{ index: 0, name: "Base", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
			{ index: 1, name: "Variant", explicit: true, entries: [{ entryOrdinal: 1, listIndex: 0 }] },
		],
	};
	controller.prepareForRender("10:123", true);
	controller.applyRenderResult({
		catalogJson: JSON.stringify(catalog),
		defaultScenarioIndex: 0,
		defaultEntryOrdinal: 1,
	});
	assert.equal(select.value, "0:1");
	assert.equal(controller.handleSelectionChange("1:1"), true);
	assert.equal(select.value, "1:1");
	controller.prepareForRender("10:123", false);
	controller.applyRenderResult({
		catalogJson: JSON.stringify(catalog),
		defaultScenarioIndex: 0,
		defaultEntryOrdinal: 1,
	});
	assert.equal(select.value, "1:1");
	controller.prepareForRender("11:456", true);
	assert.equal(controller.getSelectionForRender(), null);
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

test("Vector raster failure preserves zoom mode", () => {
		const context = setup();
		const zoomController = context.exports.ZoomController;
		const storageKeys = context.exports.STORAGE_KEYS;
		zoomController.setVectorScalingEnabled(true, { skipRerender: true });
		assert.equal(context.window.localStorage.getItem(storageKeys.VECTOR_SCALING), "1");
		const disabled = zoomController.handleVectorRasterFailure({ vectorRenderLimit: 1, renderZoom: 2 });
		assert.equal(disabled, false);
		assert.equal(zoomController.isVectorScalingEnabled(), true);
		assert.equal(context.window.localStorage.getItem(storageKeys.VECTOR_SCALING), "1");
});

test("Vector toggle keeps zoom after baseline reset", () => {
	const context = setup();
	const zoomController = context.exports.ZoomController;
	const canvas = context.elements.ivgCanvas;
	zoomController.setCanvasMetrics({
		width: 400,
		height: 200,
		translateX: 8,
		translateY: 16,
		zoomApplied: 1,
		vectorRenderLimit: Infinity,
	});
	zoomController.setZoom(0.25, { skipPersist: true, skipVectorRefresh: true });
	assert.equal(canvas.style.transform, "translate(8px,16px) scale(0.25)");
	zoomController.invalidateBaseMetrics();
	assert.equal(zoomController.getBaseMetrics(), null);
	assert.equal(canvas.style.transform, "translate(8px,16px) scale(0.25)");
	zoomController.setVectorScalingEnabled(true, { skipRerender: true, skipPersist: true });
	assert.equal(canvas.style.transform, "translate(2px,4px)");
	assert.equal(canvas.style.width, "100px");
	zoomController.setVectorScalingEnabled(false, { skipRerender: true, skipPersist: true });
	assert.equal(canvas.style.transform, "translate(8px,16px) scale(0.25)");
	assert.equal(canvas.style.width, "400px");
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
