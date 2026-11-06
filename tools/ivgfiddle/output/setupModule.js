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
		let initSource = Settings.read(STORAGE_KEYS.SOURCE, "");
		if (initSource === "") {
			initSource = runtimeModule.FS.readFile("demoSource.ivg", { encoding: "utf8" });
		}
		ace.edit("editor").setValue(initSource);
		if (Settings.read(STORAGE_KEYS.RUN_ON_STARTUP, "") === "false") {
			trace("Last execution was terminated. Modify IVG code to run again.");
		} else {
			runIVG();
		}
	},
};
window.ivgRuntimeModule = null;
var Module = moduleConfig;
window.Module = moduleConfig;
