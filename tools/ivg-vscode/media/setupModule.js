const globalObject = typeof globalThis !== "undefined" ? globalThis : window;

const TRACE_STUB_LIMIT = 1024;
const traceStubBuffer = Array.isArray(globalObject.__ivgPreviewTraceBuffer) ? globalObject.__ivgPreviewTraceBuffer : [];
globalObject.__ivgPreviewTraceBuffer = traceStubBuffer;

if (typeof globalObject.trace !== "function") {
	globalObject.trace = function traceStub(message) {
		const text = typeof message === "string" ? message : String(message);
		if (traceStubBuffer.length >= TRACE_STUB_LIMIT) {
			traceStubBuffer.shift();
		}
		traceStubBuffer.push(text);
		if (typeof console !== "undefined" && typeof console.log === "function") {
			console.log(text);
		}
	};
}

const moduleConfig = {
	print: function (text) {
		trace(text);
	},
	printErr: function (text) {
		trace(text);
	},
	onRuntimeInitialized: function () {
		moduleConfig.__runtimeReady = true;
	},
};
//
// Emscripten expects a global `Module` variable to exist before the generated
// runtime loads so configuration can be merged. Assign via `globalThis` so the
// generated bundle's `var Module = ...` declaration can proceed without a
// redeclaration syntax error.
//
globalObject.Module = moduleConfig;

function notifyPreviewReady(moduleInstance) {
	let initSource = localStorage.getItem("ivgSource");
	if (initSource == null || initSource === "") {
		initSource = moduleInstance.FS.readFile("demoSource.ivg", { encoding: "utf8" });
	}
	if (typeof window.ivgPreviewModuleInitialized === "function") {
		window.ivgPreviewModuleInitialized(initSource);
	}
}

window.addEventListener("load", function () {
	const currentModule = globalObject.Module;
	if (typeof currentModule === "function") {
		currentModule(moduleConfig).then(function (instance) {
			globalObject.Module = instance;
			notifyPreviewReady(instance);
		});
	} else if (moduleConfig.__runtimeReady && currentModule && currentModule.FS) {
		notifyPreviewReady(currentModule);
	}
});
