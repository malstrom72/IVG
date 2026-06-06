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
	onAbort: function (reason) {
		moduleConfig.__runtimeReady = false;
		trace("Renderer aborted: " + String(reason));
	},
};

let moduleFactory = typeof globalObject.__ivgModuleFactory === "function" ? globalObject.__ivgModuleFactory : null;
let moduleInstantiation = null;
let moduleActivation = null;
//
// Emscripten expects a global `Module` variable to exist before the generated
// runtime loads so configuration can be merged. Assign via `globalThis` so the
// generated bundle's `var Module = ...` declaration can proceed without a
// redeclaration syntax error.
//
globalObject.Module = moduleConfig;

function instantiateModule() {
	if (moduleInstantiation) {
		return moduleInstantiation;
	}
	if (typeof moduleFactory !== "function") {
		return Promise.reject(new Error("IVG rasterizer module is not available."));
	}
	globalObject.Module = moduleConfig;
	moduleConfig.__runtimeReady = false;
	moduleInstantiation = moduleFactory(moduleConfig)
		.then(function handleModuleInstance(instance) {
			globalObject.Module = instance;
			return instance;
		})
		.catch(function handleModuleFailure(error) {
			trace("Failed to instantiate IVG rasterizer module.");
			trace(String(error));
			throw error;
		})
		.finally(function clearInstantiationState() {
			moduleInstantiation = null;
		});
	return moduleInstantiation;
}

function activateModule() {
	if (moduleActivation) {
		return moduleActivation;
	}
	moduleActivation = instantiateModule()
		.then(function notifyModuleReady(instance) {
			notifyPreviewReady(instance);
			return instance;
		})
		.finally(function clearActivationState() {
			moduleActivation = null;
		});
	return moduleActivation;
}

function logActivationFailure(error) {
	trace("Failed to activate IVG rasterizer module.");
	trace(String(error));
}

globalObject.__ivgReloadModule = function reloadModule() {
	return activateModule();
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

function ensureModuleFactory(candidate) {
	if (typeof candidate === "function") {
		moduleFactory = candidate;
		globalObject.__ivgModuleFactory = candidate;
	}
}

window.addEventListener("load", function () {
	const currentModule = globalObject.Module;
	if (typeof currentModule === "function") {
		ensureModuleFactory(currentModule);
		activateModule().catch(logActivationFailure);
		return;
	}
	ensureModuleFactory(globalObject.__ivgModuleFactory);
	if (moduleConfig.__runtimeReady && currentModule && currentModule.FS) {
		notifyPreviewReady(currentModule);
		return;
	}
	if (typeof moduleFactory === "function") {
		activateModule().catch(logActivationFailure);
	}
});
