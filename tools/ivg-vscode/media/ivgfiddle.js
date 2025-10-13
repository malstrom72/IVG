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
			message: text,
		};
		if (options && typeof options.durationMs === "number" && options.durationMs >= 0) {
			payload.durationMs = options.durationMs;
		}
		vscodeApi.postMessage(payload);
	}

	function emitTrace(message) {
		if (!vscodeApi) {
			return;
		}
		let payload = null;
		if (typeof message === "string") {
			if (message.length === 0) {
				return;
			}
			payload = { action: "append", text: message };
		} else if (message && typeof message === "object") {
			const action = typeof message.action === "string" ? message.action : "";
			if (!action) {
				return;
			}
			if (action === "reset") {
				const lines = Array.isArray(message.lines)
					? message.lines.filter((entry) => typeof entry === "string")
					: [];
				payload = { action: "reset" };
				if (lines.length > 0) {
					payload.lines = lines;
				}
			} else if (action === "clear") {
				payload = { action: "clear" };
			} else {
				const text = typeof message.text === "string" ? message.text : "";
				if (!text) {
					return;
				}
				payload = { action: action, text: text };
			}
		}
		if (!payload) {
			return;
		}
		vscodeApi.postMessage({
			type: "trace",
			message: payload,
		});
	}

	function createPreviewInstance() {
		if (!window.IVGFiddlePreview || typeof window.IVGFiddlePreview.create !== "function") {
			return null;
		}
		const preview = window.IVGFiddlePreview.create({
			notifyStatus: notifyStatus,
			emitTrace: emitTrace,
			onReady: function handleReady() {
				if (vscodeApi) {
					vscodeApi.postMessage({ type: "ready" });
				}
			},
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
