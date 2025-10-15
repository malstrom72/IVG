#!/usr/bin/env node
"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const test = require("node:test");

const here = __dirname;
const outDir = path.resolve(process.argv[2] || path.join(here, "output"));

require("./tests/ivgfiddle.unit.test.js");

function heapU32(Module) {
	if (Module.HEAPU32) {
		return Module.HEAPU32;
	}
	const memory = Module.wasmMemory || (Module.asm && Module.asm.memory) || Module.memory;
	if (!memory) {
		throw new Error("No wasm memory found on Module");
	}
	return new Uint32Array(memory.buffer);
}

const createModule = require(path.join(outDir, "rasterizeIVG.js"));

function decodeRasterResult(Module, rasterPtr) {
	const headerView = new Uint32Array(heapU32(Module).buffer, rasterPtr, 8);
	const headerBytes = 8 * Uint32Array.BYTES_PER_ELEMENT;
	const pixelBytes = headerView[4];
	const catalogBytes = headerView[5];
	const selectedScenarioIndex = headerView[6];
	const selectedEntryOrdinal = headerView[7];
	const pixelStart = rasterPtr + headerBytes;
	const pixelSlice = Module.HEAPU8.subarray(pixelStart, pixelStart + pixelBytes);
	const pixels = new Uint8Array(pixelSlice.length);
	pixels.set(pixelSlice);
	const firstPixel = [pixels[0], pixels[1], pixels[2], pixels[3]];
	const catalogStart = pixelStart + pixelBytes;
	const catalogSlice = Module.HEAPU8.subarray(catalogStart, catalogStart + catalogBytes);
	let jsonLength = catalogSlice.indexOf(0);
	if (jsonLength === -1) {
		jsonLength = catalogSlice.length;
	}
	const catalogJson = Buffer.from(catalogSlice.subarray(0, jsonLength)).toString("utf8");
	return {
		width: headerView[2],
		height: headerView[3],
		selectedScenarioIndex,
		selectedEntryOrdinal,
		firstPixel,
		catalogJson,
		catalog: JSON.parse(catalogJson),
	};
}

test("WebAssembly rasterization smoke test", async () => {
	const Module = await createModule();
	const src = fs.readFileSync(path.join(here, "src", "demoSource.ivg"), "utf8");
	const size = Module.lengthBytesUTF8(src) + 1;
	const ptr = Module._malloc(size);
	Module.stringToUTF8(src, ptr, size);
	const raster = Module._rasterizeIVG(ptr, 1);
	Module._free(ptr);
	assert.notEqual(raster, 0, "rasterizeIVG returned 0");
	const dims = new Int32Array(heapU32(Module).buffer, raster, 4);
	assert.ok(dims[2] > 0 && dims[3] > 0, "invalid dimensions");
	Module._deallocatePixels(raster);
});

test("WebAssembly snapshot catalog playback honors selections", async () => {
	const Module = await createModule();
	const source = [
		"FORMAT IVG-2 requires:IMPD-1",
		"bounds 0,0,1,1",
		"meta snapshot scenario:Colors list:[",
		"\t[",
		"\t\tfill #FF0000",
		"\t\tRECT 0,0,1,1",
		"\t]",
		"\t[",
		"\t\tfill #00FF00",
		"\t\tRECT 0,0,1,1",
		"\t]",
		"]",
		""
	].join("\n");
	const size = Module.lengthBytesUTF8(source) + 1;
	const ptr = Module._malloc(size);
	Module.stringToUTF8(source, ptr, size);
	const defaultRasterPtr = Module._rasterizeIVG(ptr, 1);
	assert.notEqual(defaultRasterPtr, 0, "default raster failed");
	const defaultResult = decodeRasterResult(Module, defaultRasterPtr);
	Module._deallocatePixels(defaultRasterPtr);
	assert.deepEqual(defaultResult.firstPixel, [0xFF, 0x00, 0x00, 0xFF], "default pixel should be red");
	assert.equal(defaultResult.selectedScenarioIndex, 0);
	assert.equal(defaultResult.selectedEntryOrdinal, 1);
	assert.equal(defaultResult.catalog.defaultScenarioIndex, 0);
	assert.equal(defaultResult.catalog.defaultEntryOrdinal, 1);
	assert.equal(defaultResult.catalog.scenarios.length, 1);
	const scenario = defaultResult.catalog.scenarios[0];
	assert.equal(scenario.entries.length, 2);
	assert.equal(scenario.entries[0].entryOrdinal, 1);
	assert.equal(scenario.entries[0].listIndex, 0);
	assert.equal(scenario.entries[1].entryOrdinal, 2);
	assert.equal(scenario.entries[1].listIndex, 1);
	const variantRasterPtr = Module._rasterizeIVG(ptr, 1, 0, 2);
	assert.notEqual(variantRasterPtr, 0, "variant raster failed");
	const variantResult = decodeRasterResult(Module, variantRasterPtr);
	Module._deallocatePixels(variantRasterPtr);
	Module._free(ptr);
	assert.deepEqual(variantResult.firstPixel, [0x00, 0xFF, 0x00, 0xFF], "variant pixel should be green");
	assert.equal(variantResult.selectedScenarioIndex, 0);
	assert.equal(variantResult.selectedEntryOrdinal, 2);
});


