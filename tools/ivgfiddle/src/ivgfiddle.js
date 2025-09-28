"use strict";

const MAX_LOG_SIZE = 64 * 1024;
const MAX_LOG_LINES = 1000;

const leftPanelElement = document.getElementById("leftPanel");
const leftRightSplitElement = document.getElementById('leftRightSplit');
const traceElement = document.getElementById('trace');
const traceDiv = document.getElementById('traceDiv');
const canvasToolbarElement = document.getElementById('canvasToolbar');
const rightPanelElement = document.getElementById('rightPanel');
const screenElement = document.getElementById('screen');
const zoomOutButton = document.getElementById('zoomOutButton');
const zoomInButton = document.getElementById('zoomInButton');
const zoomResetButton = document.getElementById('zoomResetButton');
const zoomLevelSelect = document.getElementById('zoomLevelSelect');
const backgroundButton = document.getElementById('backgroundButton');
const backgroundOverlay = document.getElementById('backgroundOverlay');
const backgroundDialog = document.getElementById('backgroundDialog');
const backgroundCloseButton = document.getElementById('backgroundCloseButton');
const backgroundSwatchContainer = document.getElementById('backgroundSwatchContainer');
const ivgCanvas = document.getElementById('ivgCanvas');
const ivgContext = ivgCanvas.getContext("2d");

const STORAGE_KEYS = Object.freeze({
        SOURCE: 'ivgSource',
        RUN_ON_STARTUP: 'runOnStartup',
        ZOOM_LEVEL: 'ivgZoomLevel',
        BACKGROUND_COLOR: 'ivgBackgroundColor'
});

const BACKGROUND_COLORS = Object.freeze([
        { value: 'transparent', label: 'Transparent', preview: 'transparent' },
        { value: 'black', label: 'Black', preview: '#000000' },
        { value: 'white', label: 'White', preview: '#ffffff' },
        { value: 'maroon', label: 'Maroon', preview: '#800000' },
        { value: 'red', label: 'Red', preview: '#ff0000' },
        { value: 'orange', label: 'Orange', preview: '#ffa500' },
        { value: 'yellow', label: 'Yellow', preview: '#ffff00' },
        { value: 'olive', label: 'Olive', preview: '#808000' },
        { value: 'green', label: 'Green', preview: '#008000' },
        { value: 'purple', label: 'Purple', preview: '#800080' },
        { value: 'fuchsia', label: 'Fuchsia', preview: '#ff00ff' },
        { value: 'lime', label: 'Lime', preview: '#00ff00' },
        { value: 'teal', label: 'Teal', preview: '#008080' },
        { value: 'aqua', label: 'Aqua', preview: '#00ffff' },
        { value: 'blue', label: 'Blue', preview: '#0000ff' },
        { value: 'navy', label: 'Navy', preview: '#000080' },
        { value: 'gray', label: 'Gray', preview: '#808080' },
        { value: 'silver', label: 'Silver', preview: '#c0c0c0' }
]);

const BACKGROUND_DEFAULT = BACKGROUND_COLORS[0].value;

const BackgroundController = (function createBackgroundController() {
        let currentColor = BACKGROUND_DEFAULT;
        let isOpen = false;
        let lastTrigger = null;
        const bodyElement = document.body;
        const defaultBodyBackground = bodyElement.style.backgroundColor;

        function getColorDefinition(value) {
                for (let index = 0; index < BACKGROUND_COLORS.length; ++index) {
                        const entry = BACKGROUND_COLORS[index];
                        if (entry.value === value) {
                                return entry;
                        }
                }
                return null;
        }

        function normalizeColor(value) {
                const definition = getColorDefinition(value);
                if (definition === null) {
                        return BACKGROUND_DEFAULT;
                }
                return definition.value;
        }

        function createSwatches() {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                backgroundSwatchContainer.innerHTML = '';
                const fragment = document.createDocumentFragment();
                for (let index = 0; index < BACKGROUND_COLORS.length; ++index) {
                        const color = BACKGROUND_COLORS[index];
                        const swatchButton = document.createElement('button');
                        swatchButton.type = 'button';
                        swatchButton.className = 'background-swatch';
                        swatchButton.setAttribute('role', 'option');
                        swatchButton.setAttribute('aria-selected', 'false');
                        swatchButton.setAttribute('data-background', color.value);
                        swatchButton.style.setProperty('--swatch-color', color.preview);
                        const preview = document.createElement('span');
                        preview.className = 'background-swatch__preview';
                        const label = document.createElement('span');
                        label.className = 'background-swatch__label';
                        label.textContent = color.label;
                        swatchButton.appendChild(preview);
                        swatchButton.appendChild(label);
                        fragment.appendChild(swatchButton);
                }
                backgroundSwatchContainer.appendChild(fragment);
        }

        function updateSelectionUI() {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                const buttons = backgroundSwatchContainer.querySelectorAll('.background-swatch');
                for (let index = 0; index < buttons.length; ++index) {
                        const button = buttons[index];
                        const value = button.getAttribute('data-background');
                        if (value === currentColor) {
                                button.setAttribute('data-selected', 'true');
                                button.setAttribute('aria-selected', 'true');
                        } else {
                                button.removeAttribute('data-selected');
                                button.setAttribute('aria-selected', 'false');
                        }
                }
        }

        function updateTriggerLabel() {
                if (backgroundButton === null) {
                        return;
                }
                const definition = getColorDefinition(currentColor);
                const labelText = definition ? definition.label : currentColor;
                backgroundButton.setAttribute('aria-label', 'Change canvas background (current: ' + labelText + ')');
        }

        function persistColor() {
                localStorage.setItem(STORAGE_KEYS.BACKGROUND_COLOR, currentColor);
        }

        function applyColorToDOM() {
                if (rightPanelElement !== null) {
                        if (currentColor === 'transparent') {
                                rightPanelElement.classList.add('transparent');
                                rightPanelElement.style.backgroundColor = '';
                        } else {
                                rightPanelElement.classList.remove('transparent');
                                rightPanelElement.style.backgroundColor = currentColor;
                        }
                }
                if (screenElement !== null) {
                        if (currentColor === 'transparent') {
                                screenElement.style.backgroundColor = '';
                        } else {
                                screenElement.style.backgroundColor = currentColor;
                        }
                }
                if (bodyElement !== null) {
                        if (currentColor === 'transparent') {
                                bodyElement.style.backgroundColor = defaultBodyBackground;
                        } else {
                                bodyElement.style.backgroundColor = currentColor;
                        }
                }
        }

        function applyColor(value, options) {
                const settings = options || {};
                const normalized = normalizeColor(value);
                if (normalized === currentColor && settings.force !== true) {
                        updateSelectionUI();
                        updateTriggerLabel();
                        return;
                }
                currentColor = normalized;
                applyColorToDOM();
                updateSelectionUI();
                updateTriggerLabel();
                if (!settings.skipPersist) {
                        persistColor();
                }
        }

        function focusCurrentSwatch() {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                const active = backgroundSwatchContainer.querySelector('.background-swatch[data-background="' + currentColor + '"]');
                if (active) {
                        active.focus();
                }
        }

        function focusRelativeSwatch(startButton, delta) {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                const buttons = backgroundSwatchContainer.querySelectorAll('.background-swatch');
                if (buttons.length === 0) {
                        return;
                }
                const buttonList = Array.prototype.slice.call(buttons);
                let index = buttonList.indexOf(startButton);
                if (index === -1) {
                        index = 0;
                }
                index = (index + delta + buttonList.length) % buttonList.length;
                buttonList[index].focus();
        }

        function focusFirstSwatch() {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                const first = backgroundSwatchContainer.querySelector('.background-swatch');
                if (first) {
                        first.focus();
                }
        }

        function focusLastSwatch() {
                if (backgroundSwatchContainer === null) {
                        return;
                }
                const buttons = backgroundSwatchContainer.querySelectorAll('.background-swatch');
                if (buttons.length > 0) {
                        buttons[buttons.length - 1].focus();
                }
        }

        function handleSwatchClick(event) {
                const target = event.target;
                if (target === null) {
                        return;
                }
                const button = target.closest ? target.closest('.background-swatch') : null;
                if (button === null) {
                        return;
                }
                const value = button.getAttribute('data-background');
                if (!value) {
                        return;
                }
                event.preventDefault();
                applyColor(value);
                close();
        }

        function handleSwatchKeydown(event) {
                const target = event.target;
                if (target === null || !target.classList || !target.classList.contains('background-swatch')) {
                        return;
                }
                if (event.key === 'ArrowRight' || event.key === 'ArrowDown') {
                        event.preventDefault();
                        focusRelativeSwatch(target, 1);
                        return;
                }
                if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') {
                        event.preventDefault();
                        focusRelativeSwatch(target, -1);
                        return;
                }
        if (event.key === 'Home') {
                event.preventDefault();
                focusFirstSwatch();
                return;
        }
        if (event.key === 'End') {
                event.preventDefault();
                focusLastSwatch();
                return;
        }
                if (event.key === 'Enter' || event.key === ' ') {
                        event.preventDefault();
                        const value = target.getAttribute('data-background');
                        if (value) {
                                applyColor(value);
                                close();
                        }
                }
        }

        function handleOverlayKeydown(event) {
                if (!isOpen) {
                        return;
                }
                if (event.key === 'Escape') {
                        event.preventDefault();
                        close();
                }
        }

        function handleOverlayClick(event) {
                if (!isOpen || backgroundOverlay === null) {
                        return;
                }
                if (event.target === backgroundOverlay) {
                        event.preventDefault();
                        close();
                }
        }

        function open(trigger) {
                if (backgroundOverlay === null) {
                        return;
                }
                if (isOpen) {
                        focusCurrentSwatch();
                        return;
                }
                isOpen = true;
                lastTrigger = trigger || backgroundButton;
                backgroundOverlay.classList.remove('is-hidden');
                backgroundOverlay.setAttribute('aria-hidden', 'false');
                if (backgroundButton !== null) {
                        backgroundButton.setAttribute('aria-expanded', 'true');
                }
                document.addEventListener('keydown', handleOverlayKeydown, true);
                window.setTimeout(focusCurrentSwatch, 0);
        }

        function close() {
                if (!isOpen || backgroundOverlay === null) {
                        return;
                }
                isOpen = false;
                backgroundOverlay.classList.add('is-hidden');
                backgroundOverlay.setAttribute('aria-hidden', 'true');
                if (backgroundButton !== null) {
                        backgroundButton.setAttribute('aria-expanded', 'false');
                }
                document.removeEventListener('keydown', handleOverlayKeydown, true);
                if (lastTrigger && typeof lastTrigger.focus === 'function') {
                        lastTrigger.focus();
                }
                lastTrigger = null;
        }

        function readInitialColor() {
                const stored = localStorage.getItem(STORAGE_KEYS.BACKGROUND_COLOR);
                if (stored === null) {
                        return BACKGROUND_DEFAULT;
                }
                return normalizeColor(stored);
        }

        function bindEvents() {
                if (backgroundButton !== null) {
                        backgroundButton.addEventListener('click', function handleBackgroundButtonClick() {
                                open(backgroundButton);
                        });
                }
                if (backgroundCloseButton !== null) {
                        backgroundCloseButton.addEventListener('click', function handleBackgroundCloseClick() {
                                close();
                        });
                }
                if (backgroundOverlay !== null) {
                        backgroundOverlay.addEventListener('click', handleOverlayClick);
                }
                if (backgroundSwatchContainer !== null) {
                        backgroundSwatchContainer.addEventListener('click', handleSwatchClick);
                        backgroundSwatchContainer.addEventListener('keydown', handleSwatchKeydown);
                }
        }

        function init() {
                createSwatches();
                const initialColor = readInitialColor();
                applyColor(initialColor, { skipPersist: true, force: true });
                bindEvents();
        }

        return {
                init: init,
                applyColor: applyColor,
                open: open,
                close: close
        };
})();

const ZOOM_CONSTANTS = Object.freeze({
        MIN: 0.25,
        MAX: 4.0,
        STEP: 0.25,
        DEFAULT: 1.0
});

BackgroundController.init();

const ZoomController = (function createZoomController() {
	let currentZoom = ZOOM_CONSTANTS.DEFAULT;
	let baseMetrics = null;

	function zoomToPercent(value) {
		return Math.round(value * 100);
	}

	function clampZoom(value) {
		const numeric = Number(value);
		if (!Number.isFinite(numeric)) {
			return ZOOM_CONSTANTS.DEFAULT;
		}
		return Math.min(ZOOM_CONSTANTS.MAX, Math.max(ZOOM_CONSTANTS.MIN, numeric));
	}

	function percentToZoom(percentString) {
		const percent = Number.parseInt(percentString, 10);
		if (!Number.isFinite(percent)) {
			return currentZoom;
		}
		return clampZoom(percent / 100);
	}

	function reflectUIState() {
		if (zoomLevelSelect !== null) {
			zoomLevelSelect.value = String(zoomToPercent(currentZoom));
		}
		if (zoomOutButton !== null) {
			zoomOutButton.disabled = currentZoom <= ZOOM_CONSTANTS.MIN + 0.0001;
		}
		if (zoomInButton !== null) {
			zoomInButton.disabled = currentZoom >= ZOOM_CONSTANTS.MAX - 0.0001;
		}
	}

	function persistZoom() {
		localStorage.setItem(STORAGE_KEYS.ZOOM_LEVEL, currentZoom.toFixed(2));
	}

	function applyZoom() {
		if (ivgCanvas === null) {
			return;
		}
		if (baseMetrics === null) {
			ivgCanvas.style.transformOrigin = 'top left';
			ivgCanvas.style.transform = 'scale(' + currentZoom + ')';
			return;
		}
		const metrics = baseMetrics;
		ivgCanvas.style.width = metrics.width + 'px';
		ivgCanvas.style.height = metrics.height + 'px';
		ivgCanvas.style.transformOrigin = 'top left';
		ivgCanvas.style.transform = 'translate(' + metrics.translateX + 'px,' + metrics.translateY + 'px) scale(' + currentZoom + ')';
	}

	function setZoom(value, options) {
		const settings = options || {};
		const clamped = clampZoom(value);
		if (Math.abs(clamped - currentZoom) < 0.0001) {
			reflectUIState();
			return;
		}
		currentZoom = clamped;
		if (!settings.skipPersist) {
			persistZoom();
		}
		applyZoom();
		reflectUIState();
	}

	function incrementZoom(delta) {
		setZoom(currentZoom + delta);
	}

	function resetZoom() {
		setZoom(ZOOM_CONSTANTS.DEFAULT);
	}

	function bindUIEvents() {
		if (zoomOutButton !== null) {
			zoomOutButton.addEventListener('click', function handleZoomOutClick() {
				incrementZoom(-ZOOM_CONSTANTS.STEP);
			});
		}
		if (zoomInButton !== null) {
			zoomInButton.addEventListener('click', function handleZoomInClick() {
				incrementZoom(ZOOM_CONSTANTS.STEP);
			});
		}
		if (zoomResetButton !== null) {
			zoomResetButton.addEventListener('click', function handleZoomResetClick() {
				resetZoom();
			});
		}
		if (zoomLevelSelect !== null) {
			zoomLevelSelect.addEventListener('change', function handleZoomSelectChange(event) {
				const target = event.target;
				setZoom(percentToZoom(target.value));
			});
		}
		document.addEventListener('keydown', handleZoomShortcut, true);
	}

	function targetBlocksShortcut(element) {
		if (element === null) {
			return false;
		}
		if (element.closest && element.closest('#editor') !== null) {
			return true;
		}
		const tagName = element.tagName;
		if (tagName === 'INPUT' || tagName === 'TEXTAREA' || tagName === 'SELECT') {
			return true;
		}
		if (element.isContentEditable === true) {
			return true;
		}
		return false;
	}

	function handleZoomShortcut(event) {
		if (event.defaultPrevented) {
			return;
		}
		if (!(event.ctrlKey || event.metaKey) || event.altKey) {
			return;
		}
		const activeElement = document.activeElement;
		if (targetBlocksShortcut(activeElement)) {
			return;
		}
		const key = event.key;
		if (key === '+' || key === '=') {
			event.preventDefault();
			incrementZoom(ZOOM_CONSTANTS.STEP);
			return;
		}
		if (key === '-') {
			event.preventDefault();
			incrementZoom(-ZOOM_CONSTANTS.STEP);
			return;
		}
		if (key === '0') {
			event.preventDefault();
			resetZoom();
		}
	}

	function readInitialZoom() {
		const stored = localStorage.getItem(STORAGE_KEYS.ZOOM_LEVEL);
		if (stored === null) {
			return;
		}
		const parsed = Number.parseFloat(stored);
		if (!Number.isFinite(parsed)) {
			return;
		}
		currentZoom = clampZoom(parsed);
	}

	function init() {
		readInitialZoom();
		bindUIEvents();
		reflectUIState();
	}

	function setCanvasMetrics(metrics) {
		if (metrics === null) {
			baseMetrics = null;
			applyZoom();
			return;
		}
		baseMetrics = {
			width: metrics.width,
			height: metrics.height,
			translateX: metrics.translateX,
			translateY: metrics.translateY
		};
		applyZoom();
	}

	return {
		init: init,
		setZoom: setZoom,
		incrementZoom: incrementZoom,
		resetZoom: resetZoom,
		applyZoom: applyZoom,
		setCanvasMetrics: setCanvasMetrics
	};
})();

ZoomController.init();

// Mark the toolbar as hydrated so follow-up milestones can append additional controls
// while preserving the established focus order and ARIA annotations.
if (canvasToolbarElement !== null) {
	canvasToolbarElement.setAttribute('data-toolbar-ready', 'true');
}

let allLogLines = '';
let traceLinesCount = 0;
let lastLogLine = null;
let repeatingLogLineCount = 0;

function clearTrace() {
	allLogLines = '';
	traceLinesCount = 0;
	lastLogLine = null;
	repeatingLogLineCount = 0;
	traceElement.textContent = '';
	traceDiv.scrollTop = 0;
}

function trace(message) {
	const allLines = traceElement.textContent;
	while (allLogLines.length > MAX_LOG_SIZE || traceLinesCount >= MAX_LOG_LINES) {
		const offset = allLogLines.indexOf("\n");
		if (offset < 0) {
			break;
		}
		allLogLines = allLogLines.substr(offset + 1);
		--traceLinesCount;
	}
	if (lastLogLine === message) {
		if (repeatingLogLineCount > 1) {
			const offset = allLogLines.lastIndexOf(' *');
			if (offset >= 0) {
				allLogLines = allLogLines.substr(0, offset);
			}
		} else {
			allLogLines = allLogLines.substr(0, allLogLines.length - 1); // remove last LF
		}
		++repeatingLogLineCount;
		allLogLines += " *" + repeatingLogLineCount + "\n";
	} else {
		allLogLines += message + "\n";
		++traceLinesCount;
		lastLogLine = message;
		repeatingLogLineCount = 1;
	}
	traceElement.textContent = allLogLines;
	traceDiv.scrollTop = traceDiv.scrollHeight;
}

// Can't use cwrap to pass very long strings, as they are placed on the stackm and not the heap.
const rasterizeIVG = function(source, scaling) {
	const size = Module.lengthBytesUTF8(source) + 1;
	const stringPointer = Module._malloc(size);
	Module.stringToUTF8(source, stringPointer, size);
	const result = Module._rasterizeIVG(stringPointer, scaling);
	Module._free(stringPointer);
	return result;
};
function deallocatePixels(pixelsPointer) {
	Module._deallocatePixels(pixelsPointer);
}

function heapU32(Module) {
	if (Module.HEAPU32) return Module.HEAPU32;
	const mem =
		Module.wasmMemory ||
		(Module.asm && Module.asm.memory) ||
		Module.memory;
	if (!mem) throw new Error("No wasm memory found on Module");
	return new Uint32Array(mem.buffer);
}

/**
	Run the IVG compiler, rasterize the output and push the resulting bitmap onto the canvas.
	The render lifecycle is intentionally documented so zoom/background milestones have clear
	anchor points:
		1. `runIVG` reads source from Ace, persists it, and clears trace output.
		2. We ask the WebAssembly module to rasterize at device pixel ratio, returning a buffer
			where the first four `Int32` values describe `{left, top, width, height}`.
		3. Canvas dimensions and style width/height are set before any transforms so zoom logic
			can hook into a consistent baseline.
		4. A CSS `translate(x, y)` positions the canvas relative to the artboard origin. Zoom
			will extend this by chaining `scale(...)` while keeping translation anchored.
		5. The decoded pixels are blitted via `putImageData`, completing the redraw.
*/
function runIVG() {
	clearTrace();
	trace("Running IVG");
	const linesCountWas = traceLinesCount;
	const start = Date.now();
	const sourceCode = aceEditor.getValue();
	localStorage.setItem(STORAGE_KEYS.SOURCE, sourceCode);
	localStorage.setItem(STORAGE_KEYS.RUN_ON_STARTUP, false);
	let ok = false;
	try {
		const pixelRatio = window.devicePixelRatio;
		const rasterPointer = rasterizeIVG(sourceCode, pixelRatio);
		const end = Date.now();
		if (rasterPointer !== 0) {
			const heap = heapU32(Module).buffer;
			let dimensions = new Int32Array(heap, rasterPointer, 4);
			const left = dimensions[0];
			const top = dimensions[1];
			const width = dimensions[2];
			const height = dimensions[3];
			dimensions = null;
			let pixelData = new Uint8Array(heap, rasterPointer + 4 * 4, width * height * 4);
			deallocatePixels(rasterPointer);
			ivgCanvas.width = width;
			ivgCanvas.height = height;
			const imageData = ivgContext.createImageData(width, height);
			imageData.data.set(pixelData);
			pixelData = null;
			const cssWidth = width / pixelRatio;
			const cssHeight = height / pixelRatio;
			const translateX = left / pixelRatio;
			const translateY = top / pixelRatio;
			ZoomController.setCanvasMetrics({
				width: cssWidth,
				height: cssHeight,
				translateX: translateX,
				translateY: translateY
			});
			ivgContext.putImageData(imageData, 0, 0);
			trace("Completed IVG");
			trace("Time spent: " + (end - start) + "ms");
			ok = true;
		} else {
			trace("Aborted IVG");
		}
		localStorage.setItem(STORAGE_KEYS.RUN_ON_STARTUP, true);
	}
	catch (e) {
		trace("Rasterization crashed");
		trace(e);
	}
	if (!ok) {
		ivgContext.beginPath();
		ivgContext.moveTo(0, 0);
		ivgContext.lineTo(ivgCanvas.width, ivgCanvas.height);
		ivgContext.moveTo(0, ivgCanvas.height);
		ivgContext.lineTo(ivgCanvas.width, 0);
		ivgContext.strokeStyle = "red";
		ivgContext.lineWidth = 10;
		ivgContext.stroke();
	}
}

const aceEditor = ace.edit("editor");
aceEditor.setTheme("ace/theme/twilight");
const aceSession = aceEditor.getSession();
aceSession.setUseSoftTabs(false);
aceSession.setMode("ace/mode/ivg");
let recompileTimer = null;
aceSession.on('change', function(e) {
	if (recompileTimer !== null) {
		clearTimeout(recompileTimer);
		recompileTimer = null;
	};
	recompileTimer = setTimeout(runIVG, 500);
});

let isDragging = false;
let currentX;
let currentPanelWidth;
leftRightSplitElement.addEventListener('mousedown', function(e) {
  isDragging = true;
  currentX = e.clientX;
  currentPanelWidth = leftPanelElement.offsetWidth;
  e.preventDefault();
});
document.addEventListener('mousemove', function(e) {
  if (isDragging) {
	leftPanelElement.style.width = (currentPanelWidth + e.clientX - currentX) + 'px';
  }
});
document.addEventListener('mouseup', function(e) {
  isDragging = false;
});
