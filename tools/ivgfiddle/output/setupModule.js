const moduleConfig = {
	print: function (text) {
		trace(text);
	},
	printErr: function (text) {
		trace(text);
	},
	onRuntimeInitialized: function () {
		const runtimeModule = this;
		window.ivgRuntimeModule = runtimeModule;
		window.Module = runtimeModule;
		Module = runtimeModule;
		let initSource = localStorage.getItem("ivgSource");
		if (initSource == null || initSource === "") {
			initSource = runtimeModule.FS.readFile("demoSource.ivg", { encoding: "utf8" });
		}
		ace.edit("editor").setValue(initSource);
		if (localStorage.getItem("runOnStartup") === "false") {
			trace("Last execution was terminated. Modify IVG code to run again.");
		} else {
			runIVG();
		}
	},
};
window.ivgRuntimeModule = null;
var Module = moduleConfig;
window.Module = moduleConfig;
window.addEventListener("load", function () {
	if (typeof Module === "function") {
		Module(moduleConfig).then(function (instance) {
			window.ivgRuntimeModule = instance;
			window.Module = instance;
			Module = instance;
		}).catch(function (error) {
			trace("Failed to initialize WebAssembly module");
			trace(error);
		});
	}
});
