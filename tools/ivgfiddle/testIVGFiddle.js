#!/usr/bin/env node
"use strict";
const fs = require("node:fs");
const path = require("node:path");
const here = __dirname;
const outDir = path.resolve(process.argv[2] || path.join(here, "..", "..", "output", "ivgfiddle"));

function fail(msg){ console.error(msg); process.exit(1); }

function heapU32(Module){
	if (Module.HEAPU32) return Module.HEAPU32;
	const mem =
		Module.wasmMemory ||
		(Module.asm && Module.asm.memory) ||
		Module.memory;
	if (!mem) throw new Error("No wasm memory found on Module");
	return new Uint32Array(mem.buffer);
}

const createModule = require(path.join(outDir, "rasterizeIVG.js"));
createModule().then(Module => {
	try {
		const src = fs.readFileSync(path.join(here, "src", "demoSource.ivg"), "utf8");
		const size = Module.lengthBytesUTF8(src) + 1;
		const ptr = Module._malloc(size);
		Module.stringToUTF8(src, ptr, size);
		const raster = Module._rasterizeIVG(ptr, 1);
		Module._free(ptr);
		if (!raster) fail("rasterizeIVG returned 0");
		const dims = new Int32Array(heapU32(Module).buffer, raster, 4);
		if (dims[2] <= 0 || dims[3] <= 0) fail("invalid dimensions");
		Module._deallocatePixels(raster);
		console.log("IVGFiddle rasterization ok");
	} catch(e) {
		fail(e.message);
	}
}).catch(err => fail(err.message));
