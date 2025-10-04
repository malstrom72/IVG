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

let moduleFactory = null;
let moduleInstantiation = null;

function obtainModuleFactory() {
        if (typeof moduleFactory === "function") {
                return moduleFactory;
        }
        const current = globalObject.Module;
        if (typeof current === "function") {
                moduleFactory = current;
                return moduleFactory;
        }
        return null;
}

function instantiateModule() {
        if (moduleInstantiation) {
                return moduleInstantiation;
        }
        const factory = obtainModuleFactory();
        if (!factory) {
                if (moduleConfig.__runtimeReady && globalObject.Module && globalObject.Module.FS) {
                        notifyPreviewReady(globalObject.Module);
                        return Promise.resolve(globalObject.Module);
                }
                return Promise.reject(new Error("IVG renderer factory unavailable."));
        }
        moduleConfig.__runtimeReady = false;
        moduleInstantiation = factory(moduleConfig)
                .then(function (instance) {
                        globalObject.Module = instance;
                        notifyPreviewReady(instance);
                        return instance;
                })
                .finally(function () {
                        moduleInstantiation = null;
                });
        return moduleInstantiation;
}

globalObject.__ivgPreviewReloadModule = function reloadModule() {
        return instantiateModule();
};

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
        instantiateModule().catch(function (error) {
                trace("Failed to initialize IVG renderer: " + error);
        });
});
