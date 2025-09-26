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
let Module = moduleConfig;
window.addEventListener('load', function() {
	if (typeof Module === 'function') {
		Module(moduleConfig).then(function(instance) {
			Module = instance;
		});
	}
});
