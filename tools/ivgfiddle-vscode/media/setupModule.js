const moduleConfig = {
	print: function(text) { trace(text); },
	printErr: function(text) { trace(text); },
	onRuntimeInitialized: function() {
		let initSource = localStorage.getItem("ivgSource");
		if (initSource == null || initSource === '') {
			initSource = Module.FS.readFile('demoSource.ivg', { encoding: 'utf8' });
		}
		if (typeof window.ivgPreviewModuleInitialized === 'function') {
			window.ivgPreviewModuleInitialized(initSource);
		}
	}
};
//
// Emscripten expects a global `Module` variable to exist before the generated
// runtime loads so configuration can be merged. Use `var` (not `let`/`const`)
// to allow the generated bundle to reassign `Module` to the module factory.
//
var Module = moduleConfig;
window.addEventListener('load', function() {
	if (typeof Module === 'function') {
		Module(moduleConfig).then(function(instance) {
			Module = instance;
		});
	}
});
