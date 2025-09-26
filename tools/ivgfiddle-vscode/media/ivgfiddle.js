"use strict";

const MAX_LOG_SIZE = 64 * 1024;
const MAX_LOG_LINES = 1000;

const statusElement = document.getElementById("status");
const traceElement = document.getElementById("trace");
const traceDiv = document.getElementById("traceDiv");
const ivgCanvas = document.getElementById("ivgCanvas");
const ivgContext = ivgCanvas.getContext("2d");
const vscodeApi = typeof acquireVsCodeApi === "function" ? acquireVsCodeApi() : undefined;

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
	traceElement.textContent = "";
	traceDiv.scrollTop = 0;
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
	traceElement.textContent = allLogLines;
	traceDiv.scrollTop = traceDiv.scrollHeight;
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
		const rasterPointer = rasterizeIVG(currentSource, pixelRatio);
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
			ivgCanvas.style.width = width / pixelRatio + "px";
			ivgCanvas.style.height = height / pixelRatio + "px";
			ivgCanvas.style.transform = "translate(" + left / pixelRatio + "px," + top / pixelRatio + "px)";
			ivgContext.putImageData(imageData, 0, 0);
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
			localStorage.setItem("ivgSource", currentSource);
		} else {
			localStorage.removeItem("ivgSource");
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

window.addEventListener("message", function(event) {
	handleHostMessage(event.data);
});

window.ivgPreviewModuleInitialized = function(initialSource) {
	moduleReady = true;
	const stored = localStorage.getItem("ivgSource");
	if (typeof stored === "string" && stored.length > 0) {
		currentSource = stored;
	} else if (typeof initialSource === "string" && initialSource.length > 0) {
		currentSource = initialSource;
		localStorage.setItem("ivgSource", currentSource);
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

const cachedSource = localStorage.getItem("ivgSource");
if (typeof cachedSource === "string" && cachedSource.length > 0) {
	currentSource = cachedSource;
        setStatus("Loaded cached IVG source. Waiting for renderer…", { level: "info" });
} else {
        setStatus("Waiting for renderer…", { level: "info" });
}

