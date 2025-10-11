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
