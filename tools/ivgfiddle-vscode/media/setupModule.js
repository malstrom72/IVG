const globalObject = typeof globalThis !== "undefined" ? globalThis : window;

const moduleConfig = {
	print: function(text) { trace(text); },
	printErr: function(text) { trace(text); },
	onRuntimeInitialized: function() {
		const activeModule = globalObject.Module;
		let initSource = localStorage.getItem("ivgSource");
		if (initSource == null || initSource === "") {
			initSource = activeModule.FS.readFile("demoSource.ivg", { encoding: "utf8" });
		}
		if (typeof window.ivgPreviewModuleInitialized === "function") {
			window.ivgPreviewModuleInitialized(initSource);
		}
	}
};
//
// Emscripten expects a global `Module` variable to exist before the generated
// runtime loads so configuration can be merged. Assign via `globalThis` so the
// generated bundle's `var Module = …` declaration can proceed without a
// redeclaration syntax error.
//
globalObject.Module = moduleConfig;
window.addEventListener("load", function() {
	const currentModule = globalObject.Module;
	if (typeof currentModule === "function") {
		currentModule(moduleConfig).then(function(instance) {
			globalObject.Module = instance;
		});
	}
});
