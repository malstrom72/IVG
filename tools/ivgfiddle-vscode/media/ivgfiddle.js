"use strict";

(function initializeVSCodePreview() {
	const vscodeApi = typeof acquireVsCodeApi === "function" ? acquireVsCodeApi() : undefined;

	function notifyStatus(level, message, options) {
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

	function createPreviewInstance() {
		if (!window.IVGFiddlePreview || typeof window.IVGFiddlePreview.create !== "function") {
			return null;
		}
		const preview = window.IVGFiddlePreview.create({
			notifyStatus: notifyStatus,
			onReady: function handleReady() {
				if (vscodeApi) {
					vscodeApi.postMessage({ type: "ready" });
				}
			}
		});
		preview.initialize();
		return preview;
	}

	const preview = createPreviewInstance();
	if (!preview) {
		return;
	}

	window.addEventListener("message", function receiveHostMessage(event) {
		preview.handleHostCommand(event.data);
	});

	window.ivgPreviewModuleInitialized = function handleModuleInitialized(initialSource) {
		preview.handleModuleInitialized(initialSource);
	};
})();
