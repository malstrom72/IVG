"use strict";

const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

function toCamelCase(name) {
	return String(name).replace(/-([a-z])/g, function convert(match, char) {
		return char.toUpperCase();
	});
}

function createStyle() {
	const props = Object.create(null);
	function setMirroredProperty(name, value) {
		const camel = toCamelCase(name);
		props[name] = value;
		if (camel !== name) {
			props[camel] = value;
		}
	}
	function removeMirroredProperty(name) {
		const camel = toCamelCase(name);
		delete props[name];
		if (camel !== name) {
			delete props[camel];
		}
	}
	const style = {
		setProperty: function setProperty(name, value) {
			setMirroredProperty(name, value);
		},
		getPropertyValue: function getPropertyValue(name) {
			const camel = toCamelCase(name);
			if (Object.prototype.hasOwnProperty.call(props, name)) {
				return props[name];
			}
			if (Object.prototype.hasOwnProperty.call(props, camel)) {
				return props[camel];
			}
			return "";
		},
		removeProperty: function removeProperty(name) {
			removeMirroredProperty(name);
		},
	};
	const styleProps = [
		"backgroundColor",
		"backgroundImage",
		"transform",
		"transformOrigin",
		"width",
		"height",
		"flexBasis",
	];
	for (let index = 0; index < styleProps.length; ++index) {
		const prop = styleProps[index];
		Object.defineProperty(style, prop, {
			get: function get() {
				return style.getPropertyValue(prop);
			},
			set: function set(value) {
				style.setProperty(prop, value);
			},
			enumerable: true,
			configurable: true,
		});
	}
	return style;
}

function createClassList(element) {
	const tokens = new Set();
	function syncAttribute() {
		if (tokens.size === 0) {
			delete element._attributes.class;
			return;
		}
		element._attributes.class = Array.from(tokens).join(" ");
	}
	return {
		add: function add() {
			for (let index = 0; index < arguments.length; ++index) {
				const value = String(arguments[index]);
				if (value.length > 0) {
					tokens.add(value);
				}
			}
			syncAttribute();
		},
		remove: function remove() {
			for (let index = 0; index < arguments.length; ++index) {
				tokens.delete(String(arguments[index]));
			}
			syncAttribute();
		},
		contains: function contains(token) {
			return tokens.has(String(token));
		},
		toggle: function toggle(token, force) {
			const normalized = String(token);
			if (force === true) {
				tokens.add(normalized);
				syncAttribute();
				return true;
			}
			if (force === false) {
				tokens.delete(normalized);
				syncAttribute();
				return false;
			}
			if (tokens.has(normalized)) {
				tokens.delete(normalized);
				syncAttribute();
				return false;
			}
			tokens.add(normalized);
			syncAttribute();
			return true;
		},
		toString: function toString() {
			return Array.from(tokens).join(" ");
		},
		_setFromString: function setFromString(value) {
			tokens.clear();
			const parts = String(value)
				.split(/\s+/)
				.filter(function filter(part) {
					return part.length > 0;
				});
			for (let index = 0; index < parts.length; ++index) {
				tokens.add(parts[index]);
			}
			syncAttribute();
		},
	};
}
function createDocumentFragmentNode(document) {
	const fragment = {
		ownerDocument: document,
		isFragment: true,
		children: [],
		appendChild: function appendChild(child) {
			if (child && child.isFragment) {
				const nodes = child.children.slice();
				child.children.length = 0;
				for (let index = 0; index < nodes.length; ++index) {
					fragment.appendChild(nodes[index]);
				}
				return child;
			}
			child.parentNode = fragment;
			fragment.children.push(child);
			return child;
		},
	};
	return fragment;
}

function querySelectorAllFrom(node, selector) {
	const results = [];
	function visit(current) {
		if (!current) {
			return;
		}
		if (!current.isFragment && current.tagName && matchesSelector(current, selector)) {
			results.push(current);
		}
		const children = current.children || [];
		for (let index = 0; index < children.length; ++index) {
			visit(children[index]);
		}
	}
	if (node.isFragment) {
		const nodes = node.children || [];
		for (let index = 0; index < nodes.length; ++index) {
			visit(nodes[index]);
		}
		return results;
	}
	visit(node);
	return results;
}

function matchesSelector(element, selector) {
	const trimmed = String(selector).trim();
	if (trimmed.length === 0) {
		return false;
	}
	let base = trimmed;
	let attributeName = null;
	let attributeValue = null;
	const attributeMatch = trimmed.match(/^(.*)\[([^=\]]+)=\"([^\"]*)\"\]$/);
	if (attributeMatch) {
		base = attributeMatch[1].trim();
		attributeName = attributeMatch[2];
		attributeValue = attributeMatch[3];
	}
	if (base.length > 0) {
		if (base.charAt(0) === ".") {
			const className = base.slice(1);
			if (!element.classList.contains(className)) {
				return false;
			}
		} else if (base !== "*") {
			if (element.tagName && element.tagName.toLowerCase() !== base.toLowerCase()) {
				return false;
			}
		}
	}
	if (attributeName !== null) {
		const value = element.getAttribute(attributeName);
		if (value === null || value !== attributeValue) {
			return false;
		}
	}
	return true;
}
function createElementNode(document, tagName) {
	const element = {
		ownerDocument: document,
		tagName: String(tagName).toUpperCase(),
		children: [],
		childNodes: null,
		parentNode: null,
		_attributes: Object.create(null),
		style: createStyle(),
		classList: null,
		textContent: "",
		hidden: false,
		disabled: false,
		defaultSelected: false,
		scrollTop: 0,
		scrollHeight: 0,
		_innerHTML: "",
		_value: "",
		_width: 320,
		_height: 200,
		_listeners: Object.create(null),
		appendChild: function appendChild(child) {
			if (child && child.isFragment) {
				const nodes = child.children.slice();
				child.children.length = 0;
				for (let index = 0; index < nodes.length; ++index) {
					this.appendChild(nodes[index]);
				}
				return child;
			}
			if (child && child.parentNode && child.parentNode !== this) {
				const siblings = child.parentNode.children || [];
				const siblingIndex = siblings.indexOf(child);
				if (siblingIndex >= 0) {
					siblings.splice(siblingIndex, 1);
				}
			}
			child.parentNode = this;
			child.ownerDocument = document;
			this.children.push(child);
			return child;
		},
		removeChild: function removeChild(child) {
			const index = this.children.indexOf(child);
			if (index >= 0) {
				this.children.splice(index, 1);
				child.parentNode = null;
			}
			return child;
		},
		addEventListener: function addEventListener(type, handler) {
			const list = this._listeners[type] || (this._listeners[type] = []);
			list.push(handler);
		},
		removeEventListener: function removeEventListener(type, handler) {
			const list = this._listeners[type];
			if (!list) {
				return;
			}
			const index = list.indexOf(handler);
			if (index >= 0) {
				list.splice(index, 1);
			}
		},
		dispatchEvent: function dispatchEvent(event) {
			const list = this._listeners[event.type] || [];
			for (let index = 0; index < list.length; ++index) {
				list[index].call(this, event);
			}
		},
		setAttribute: function setAttribute(name, value) {
			const normalizedName = String(name);
			const lower = normalizedName.toLowerCase();
			const stringValue = String(value);
			if (lower === "id") {
				if (this.id) {
					this.ownerDocument._unregisterElement(this.id);
				}
				this.id = stringValue;
				this.ownerDocument._registerElement(this.id, this);
				this._attributes.id = stringValue;
				return;
			}
			if (lower === "class") {
				this.classList._setFromString(stringValue);
				return;
			}
			if (lower === "value") {
				this._value = stringValue;
				this._attributes.value = stringValue;
				return;
			}
			this._attributes[normalizedName] = stringValue;
		},
		getAttribute: function getAttribute(name) {
			const normalizedName = String(name);
			const lower = normalizedName.toLowerCase();
			if (lower === "id") {
				return this.id || null;
			}
			if (lower === "class") {
				return this.classList.toString();
			}
			if (lower === "value") {
				return this._value;
			}
			if (Object.prototype.hasOwnProperty.call(this._attributes, normalizedName)) {
				return this._attributes[normalizedName];
			}
			return null;
		},
		removeAttribute: function removeAttribute(name) {
			const normalizedName = String(name);
			const lower = normalizedName.toLowerCase();
			if (lower === "id") {
				if (this.id) {
					this.ownerDocument._unregisterElement(this.id);
				}
				delete this.id;
				delete this._attributes.id;
				return;
			}
			if (lower === "class") {
				this.classList._setFromString("");
				return;
			}
			if (lower === "value") {
				this._value = "";
				delete this._attributes.value;
				return;
			}
			delete this._attributes[normalizedName];
		},
		querySelectorAll: function querySelectorAll(selector) {
			return querySelectorAllFrom(this, selector);
		},
		querySelector: function querySelector(selector) {
			const matches = this.querySelectorAll(selector);
			return matches.length > 0 ? matches[0] : null;
		},
		closest: function closest(selector) {
			let current = this;
			while (current) {
				if (current.tagName && matchesSelector(current, selector)) {
					return current;
				}
				current = current.parentNode;
			}
			return null;
		},
		focus: function focus() {
			this.ownerDocument.activeElement = this;
		},
		getBoundingClientRect: function getBoundingClientRect() {
			return {
				width: this._width,
				height: this._height,
				left: 0,
				top: 0,
			};
		},
		setBoundingClientRect: function setBoundingClientRect(width, height) {
			this._width = Number(width) || 0;
			this._height = Number(height) || 0;
		},
	};
	element.childNodes = element.children;
	element.classList = createClassList(element);
	Object.defineProperty(element, "innerHTML", {
		get: function get() {
			return element._innerHTML || "";
		},
		set: function set(value) {
			if (!value) {
				element.children.length = 0;
			}
			element._innerHTML = String(value || "");
		},
	});
	Object.defineProperty(element, "value", {
		get: function get() {
			return element._value;
		},
		set: function set(newValue) {
			element._value = String(newValue);
			element._attributes.value = element._value;
		},
	});
	Object.defineProperty(element, "className", {
		get: function get() {
			return element.classList.toString();
		},
		set: function set(value) {
			element.classList._setFromString(value);
		},
	});
	Object.defineProperty(element, "offsetWidth", {
		get: function get() {
			return element._width;
		},
	});
	Object.defineProperty(element, "offsetHeight", {
		get: function get() {
			return element._height;
		},
	});
	if (element.tagName === "CANVAS") {
		const context = {
			createImageData: function createImageData(width, height) {
				return { data: new Uint8ClampedArray(width * height * 4) };
			},
			putImageData: function putImageData() {},
			beginPath: function beginPath() {},
			moveTo: function moveTo() {},
			lineTo: function lineTo() {},
			stroke: function stroke() {},
		};
		element.getContext = function getContext(type) {
			if (type === "2d") {
				return context;
			}
			return null;
		};
	}
	return element;
}
function createDocument(windowObject) {
	const document = {
		defaultView: windowObject,
		_elementsById: Object.create(null),
		_listeners: Object.create(null),
		activeElement: null,
	};
	document._registerElement = function registerElement(id, element) {
		if (typeof id !== "string" || id.length === 0) {
			return;
		}
		this._elementsById[id] = element;
	};
	document._unregisterElement = function unregisterElement(id) {
		if (typeof id !== "string" || id.length === 0) {
			return;
		}
		delete this._elementsById[id];
	};
	document.createElement = function createElement(tagName) {
		return createElementNode(document, tagName);
	};
	document.createDocumentFragment = function createDocumentFragment() {
		return createDocumentFragmentNode(document);
	};
	document.getElementById = function getElementById(id) {
		return this._elementsById[id] || null;
	};
	document.addEventListener = function addEventListener(type, handler) {
		const list = this._listeners[type] || (this._listeners[type] = []);
		list.push(handler);
	};
	document.removeEventListener = function removeEventListener(type, handler) {
		const list = this._listeners[type];
		if (!list) {
			return;
		}
		const index = list.indexOf(handler);
		if (index >= 0) {
			list.splice(index, 1);
		}
	};
	document.dispatchEvent = function dispatchEvent(event) {
		const list = this._listeners[event.type] || [];
		for (let index = 0; index < list.length; ++index) {
			list[index].call(this, event);
		}
	};
	document.querySelectorAll = function querySelectorAll(selector) {
		return querySelectorAllFrom(this.documentElement, selector);
	};
	document.querySelector = function querySelector(selector) {
		const matches = this.querySelectorAll(selector);
		return matches.length > 0 ? matches[0] : null;
	};
	document.documentElement = createElementNode(document, "html");
	document.body = createElementNode(document, "body");
	document.documentElement.appendChild(document.body);
	document.childNodes = [document.documentElement];
	document.children = document.childNodes;
	return document;
}

function createStorage() {
	const map = Object.create(null);
	return {
		getItem: function getItem(key) {
			const value = map[key];
			return typeof value === "string" ? value : null;
		},
		setItem: function setItem(key, value) {
			map[key] = String(value);
		},
		removeItem: function removeItem(key) {
			delete map[key];
		},
		clear: function clear() {
			for (const name in map) {
				if (Object.prototype.hasOwnProperty.call(map, name)) {
					delete map[name];
				}
			}
		},
	};
}

function createAceSessionStub() {
	const handlers = Object.create(null);
	return {
		setUseSoftTabs: function setUseSoftTabs() {},
		setMode: function setMode() {},
		on: function on(eventName, handler) {
			handlers[eventName] = handler;
		},
		_trigger: function trigger(eventName, payload) {
			const handler = handlers[eventName];
			if (typeof handler === "function") {
				handler(payload);
			}
		},
	};
}

function createAceEditorStub(session) {
	return {
		_value: "",
		setTheme: function setTheme() {},
		getSession: function getSession() {
			return session;
		},
		getValue: function getValue() {
			return this._value;
		},
		setValue: function setValue(value) {
			this._value = String(value);
			session._trigger("change", {});
		},
		focus: function focus() {},
		resize: function resize() {},
	};
}
function buildDomStructure(document) {
	const elements = Object.create(null);
	function register(element, id) {
		element.setAttribute("id", id);
		elements[id] = element;
		return element;
	}
	const body = document.body;
	const leftPanel = register(document.createElement("div"), "leftPanel");
	leftPanel.setBoundingClientRect(320, 600);
	body.appendChild(leftPanel);
	const editor = register(document.createElement("div"), "editor");
	leftPanel.appendChild(editor);
	const leftRightSplit = register(document.createElement("div"), "leftRightSplit");
	body.appendChild(leftRightSplit);
	const traceDiv = register(document.createElement("div"), "traceDiv");
	const trace = register(document.createElement("pre"), "trace");
	traceDiv.appendChild(trace);
	body.appendChild(traceDiv);
	const rightPanel = register(document.createElement("div"), "rightPanel");
	body.appendChild(rightPanel);
	const canvasToolbar = register(document.createElement("div"), "canvasToolbar");
	rightPanel.appendChild(canvasToolbar);
	const zoomOutButton = register(document.createElement("button"), "zoomOutButton");
	const zoomInButton = register(document.createElement("button"), "zoomInButton");
	const zoomResetButton = register(document.createElement("button"), "zoomResetButton");
	const zoomLevelSelect = register(document.createElement("select"), "zoomLevelSelect");
	const vectorScalingToggle = register(document.createElement("button"), "vectorScalingToggle");
	const backgroundButton = register(document.createElement("button"), "backgroundButton");
	canvasToolbar.appendChild(zoomOutButton);
	canvasToolbar.appendChild(zoomInButton);
	canvasToolbar.appendChild(zoomResetButton);
	canvasToolbar.appendChild(zoomLevelSelect);
	canvasToolbar.appendChild(vectorScalingToggle);
	canvasToolbar.appendChild(backgroundButton);
	const screen = register(document.createElement("div"), "screen");
	rightPanel.appendChild(screen);
	const ivgCanvas = register(document.createElement("canvas"), "ivgCanvas");
	screen.appendChild(ivgCanvas);
	const backgroundOverlay = register(document.createElement("div"), "backgroundOverlay");
	backgroundOverlay.classList.add("is-hidden");
	body.appendChild(backgroundOverlay);
	const backgroundDialog = register(document.createElement("div"), "backgroundDialog");
	backgroundOverlay.appendChild(backgroundDialog);
	const backgroundCloseButton = register(document.createElement("button"), "backgroundCloseButton");
	backgroundDialog.appendChild(backgroundCloseButton);
	const backgroundSwatchContainer = register(document.createElement("div"), "backgroundSwatchContainer");
	backgroundDialog.appendChild(backgroundSwatchContainer);
	return elements;
}
function createTestWindow() {
	const windowObject = Object.create(null);
	windowObject.window = windowObject;
	windowObject.self = windowObject;
	windowObject.globalThis = windowObject;
	windowObject.console = console;
	windowObject.Math = Math;
	windowObject.Date = Date;
	windowObject.setTimeout = setTimeout.bind(globalThis);
	windowObject.clearTimeout = clearTimeout.bind(globalThis);
	windowObject.setInterval = setInterval.bind(globalThis);
	windowObject.clearInterval = clearInterval.bind(globalThis);
	windowObject.__rafQueue = [];
	windowObject.requestAnimationFrame = function requestAnimationFrame(callback) {
		if (typeof callback !== "function") {
			throw new TypeError("requestAnimationFrame callback must be a function");
		}
		windowObject.__rafQueue.push(callback);
		return windowObject.__rafQueue.length;
	};
	windowObject.cancelAnimationFrame = function cancelAnimationFrame(id) {
		const index = Number(id) - 1;
		if (index >= 0 && index < windowObject.__rafQueue.length) {
			windowObject.__rafQueue[index] = null;
		}
	};
	windowObject.devicePixelRatio = 1;
	windowObject.navigator = { userAgent: "node" };
	windowObject.performance = { now: function now() { return Date.now(); } };
	windowObject.getComputedStyle = function getComputedStyle(element) {
		return {
			backgroundColor: element.style.backgroundColor || "",
		};
	};
	return windowObject;
}

function flushAnimationFrames(windowObject) {
	const queue = windowObject.__rafQueue || [];
	windowObject.__rafQueue = [];
	for (let index = 0; index < queue.length; ++index) {
		const callback = queue[index];
		if (typeof callback === "function") {
			callback(windowObject.performance.now());
		}
	}
}

function initializeIvfFiddleForTests() {
	const windowObject = createTestWindow();
	const document = createDocument(windowObject);
	windowObject.document = document;
	document.defaultView = windowObject;
	document.body.style.backgroundColor = "#1a1a1a";
	const elements = buildDomStructure(document);
	windowObject.localStorage = createStorage();
	windowObject.sessionStorage = createStorage();
	const aceSession = createAceSessionStub();
	const aceEditor = createAceEditorStub(aceSession);
	windowObject.ace = {
		edit: function edit() {
			return aceEditor;
		},
	};
	const context = vm.createContext(windowObject);
	const sourcePath = path.resolve(__dirname, "..", "src", "ivgfiddle.js");
	const source = fs.readFileSync(sourcePath, "utf8");
	const exportFooter = "\nwindow.__ivgTestExports = {\n\tZoomController: ZoomController,\n\tBackgroundController: BackgroundController,\n\tSTORAGE_KEYS: STORAGE_KEYS,\n\testimateVectorPixelBudget: estimateVectorPixelBudget,\n\tcomputeSourceSignature: computeSourceSignature\n};\n";
	vm.runInContext(source + exportFooter, context, { filename: "ivgfiddle.js" });
	return {
		window: windowObject,
		document: document,
		elements: elements,
		exports: windowObject.__ivgTestExports,
	};
}

module.exports = {
	initializeIvfFiddleForTests: initializeIvfFiddleForTests,
	flushAnimationFrames: flushAnimationFrames,
};
