#!/usr/bin/env node
'use strict';

const fs = require('fs');

let outputString = '';
let indent = 0;
function output(line) {
	if (line === ']') {
		indent--;
	}
	outputString += '\t'.repeat(indent) + line + '\n';
	if (line.endsWith('[')) {
		indent++;
	}
}

function warning(msg) {
	console.error('Warning! ' + msg);
}

function parseRect(str) {
	const nums = str.trim().split(/[ ,]+/);
	if (nums.length !== 4 || nums.some(n => isNaN(parseFloat(n)))) {
		throw new Error('Invalid rect: ' + str);
	}
	return {
		left: parseFloat(nums[0]),
		top: parseFloat(nums[1]),
		width: parseFloat(nums[2]),
		height: parseFloat(nums[3])
	};
}

function checkRequiredAttributes(attribs, ...names) {
	for (const name of names) {
		if (!(name in attribs)) {
			throw new Error('Missing required attribute: ' + name);
		}
	}
}

const KNOWN_PRESENTATION_ATTRIBUTES = [
'stroke',
'stroke-width',
'stroke-linejoin',
'stroke-linecap',
'stroke-miterlimit',
'fill'
];

const IVG_COLORS = new Set([
'none','aqua','black','blue','fuchsia','gray','green','lime','maroon','navy','olive','purple','red','silver','teal','white','yellow'
]);

const SVG_COLORS = {
	aliceblue:"#f0f8ff",antiquewhite:"#faebd7",aquamarine:"#7fffd4",azure:"#f0ffff",beige:"#f5f5dc",bisque:"#ffe4c4",blanchedalmond:"#ffebcd",blueviolet:"#8a2be2",brown:"#a52a2a",burlywood:"#deb887",cadetblue:"#5f9ea0",chartreuse:"#7fff00",chocolate:"#d2691e",coral:"#ff7f50",cornflowerblue:"#6495ed",cornsilk:"#fff8dc",crimson:"#dc143c",cyan:"#00ffff",darkblue:"#00008b",darkcyan:"#008b8b",darkgoldenrod:"#b8860b",darkgray:"#a9a9a9",darkgreen:"#006400",darkgrey:"#a9a9a9",darkkhaki:"#bdb76b",darkmagenta:"#8b008b",darkolivegreen:"#556b2f",darkorange:"#ff8c00",darkorchid:"#9932cc",darkred:"#8b0000",darksalmon:"#e9967a",darkseagreen:"#8fbc8f",darkslateblue:"#483d8b",darkslategray:"#2f4f4f",darkslategrey:"#2f4f4f",darkturquoise:"#00ced1",darkviolet:"#9400d3",deeppink:"#ff1493",deepskyblue:"#00bfff",dimgray:"#696969",dimgrey:"#696969",dodgerblue:"#1e90ff",firebrick:"#b22222",floralwhite:"#fffaf0",forestgreen:"#228b22",gainsboro:"#dcdcdc",ghostwhite:"#f8f8ff",gold:"#ffd700",goldenrod:"#daa520",grey:"#808080",greenyellow:"#adff2f",honeydew:"#f0fff0",hotpink:"#ff69b4",indianred:"#cd5c5c",indigo:"#4b0082",ivory:"#fffff0",khaki:"#f0e68c",lavender:"#e6e6fa",lavenderblush:"#fff0f5",lawngreen:"#7cfc00",lemonchiffon:"#fffacd",lightblue:"#add8e6",lightcoral:"#f08080",lightcyan:"#e0ffff",lightgoldenrodyellow:"#fafad2",lightgray:"#d3d3d3",lightgreen:"#90ee90",lightgrey:"#d3d3d3",lightpink:"#ffb6c1",lightsalmon:"#ffa07a",lightseagreen:"#20b2aa",lightskyblue:"#87cefa",lightslategray:"#778899",lightslategrey:"#778899",lightsteelblue:"#b0c4de",lightyellow:"#ffffe0",limegreen:"#32cd32",linen:"#faf0e6",magenta:"#ff00ff",mediumaquamarine:"#66cdaa",mediumblue:"#0000cd",mediumorchid:"#ba55d3",mediumpurple:"#9370db",mediumseagreen:"#3cb371",mediumslateblue:"#7b68ee",mediumspringgreen:"#00fa9a",mediumturquoise:"#48d1cc",mediumvioletred:"#c71585",midnightblue:"#191970",mintcream:"#f5fffa",mistyrose:"#ffe4e1",moccasin:"#ffe4b5",navajowhite:"#ffdead",oldlace:"#fdf5e6",olivedrab:"#6b8e23",orange:"#ffa500",orangered:"#ff4500",orchid:"#da70d6",palegoldenrod:"#eee8aa",palegreen:"#98fb98",paleturquoise:"#afeeee",palevioletred:"#db7093",papayawhip:"#ffefd5",peachpuff:"#ffdab9",peru:"#cd853f",pink:"#ffc0cb",plum:"#dda0dd",powderblue:"#b0e0e6",rosybrown:"#bc8f8f",royalblue:"#4169e1",saddlebrown:"#8b4513",salmon:"#fa8072",sandybrown:"#f4a460",seagreen:"#2e8b57",seashell:"#fff5ee",sienna:"#a0522d",skyblue:"#87ceeb",slateblue:"#6a5acd",slategray:"#708090",slategrey:"#708090",snow:"#fffafa",springgreen:"#00ff7f",steelblue:"#4682b4",tan:"#d2b48c",thistle:"#d8bfd8",tomato:"#ff6347",turquoise:"#40e0d0",violet:"#ee82ee",wheat:"#f5deb3",whitesmoke:"#f5f5f5",yellowgreen:"#9acd32"
};

const gradients = {};

function convertPaint(sourcePaint) {
	sourcePaint = sourcePaint.trim().toLowerCase();
	if (sourcePaint.startsWith('url(')) {
		let id = sourcePaint.slice(4, -1).trim();
		if (id.startsWith('#')) {
			id = id.slice(1);
		}
		if (!(id in gradients)) {
			throw new Error('Unrecognized paint reference: ' + sourcePaint);
		}
		return buildGradient(gradients[id]);
	}
	if (/^#[0-9a-f]{6}$/.test(sourcePaint)) {
		return sourcePaint;
	}
	const rgb3 = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i;
	const m = sourcePaint.match(rgb3);
	if (m) {
		return '#' + m[1] + m[1] + m[2] + m[2] + m[3] + m[3];
	}
	if (IVG_COLORS.has(sourcePaint)) {
		return sourcePaint;
	}
	if (sourcePaint in SVG_COLORS) {
		return SVG_COLORS[sourcePaint];
	}
	throw new Error('Unrecognized color: ' + sourcePaint);
}


function convertUnits(value) {
	const str = value.trim().toLowerCase();
	const match = str.match(/^([+-]?\d*\.?\d+)([a-z%]*)$/);
	if (!match) {
		throw new Error('Invalid unit: ' + value);
	}
	const num = parseFloat(match[1]);
	switch (match[2]) {
	case '':
	case 'px':
		return num;
	case 'cm':
		return num * 96 / 2.54;
	case 'mm':
		return num * 96 / 25.4;
	case 'in':
		return num * 96;
	case 'pt':
		return num * 96 / 72;
	case 'pc':
		return num * 16;
	default:
		warning('Unsupported unit: ' + match[2]);
		return num;
	}
}

function convertOpacity(value) {
	const percentage = value.trim().endsWith('%');
	let num = parseFloat(value);
	if (percentage) {
		num /= 100;
	}
	if (num < 0 || num > 1) {
		throw new Error('Invalid opacity: ' + num);
	}
	return num;
}

function parsePoints(str) {
	const nums = str.trim().split(/[ ,]+/);
	if (nums.length < 2 || nums.length % 2 !== 0) {
		throw new Error('Invalid points: ' + str);
	}
	const points = [];
	for (let i = 0; i < nums.length; i += 2) {
		points.push([
			convertUnits(nums[i]),
			convertUnits(nums[i + 1])
		]);
	}
	return points;
}

function parseGradientCoord(value) {
	value = value.trim();
	if (value.endsWith('%')) {
		return parseFloat(value) / 100;
	}
	return convertUnits(value);
}

function parseGradientStops(element) {
	const stops = [];
	for (const item of element.contents || []) {
		if (item.element && item.element.type === 'stop') {
			const a = item.element.attributes;
			if (!('offset' in a) || !('stop-color' in a)) continue;
			let o = a.offset.trim();
			o = o.endsWith('%') ? parseFloat(o) / 100 : parseFloat(o);
			stops.push({offset: o, color: convertPaint(a['stop-color'])});
		}
	}
	return stops;
}

function buildGradient(g) {
	let s;
	if (g.type === 'linear') {
		s = `gradient:[linear ${g.x1},${g.y1},${g.x2},${g.y2}`;
	} else {
		s = `gradient:[radial ${g.cx},${g.cy},${g.r}`;
	}
	if (g.stops.length === 2 && g.stops[0].offset === 0 && g.stops[1].offset === 1) {
		s += ` from:${g.stops[0].color} to:${g.stops[1].color}`;
	} else if (g.stops.length) {
		s += ' stops:[' + g.stops.map(st => `${st.offset},${st.color}`).join(',') + ']';
	}
	s += ']';
	if (g.relative) s += ' relative:yes';
	return s;
}

function outputTransforms(str) {
	const re = /(translate|scale|rotate|skewX|skewY|matrix)\(([^)]*)\)/g;
	let m;
	while ((m = re.exec(str)) !== null) {
		const type = m[1];
		const params = m[2].trim().split(/[ ,]+/).filter(p => p.length);
		switch (type) {
			case 'translate': {
				const tx = convertUnits(params[0] || '0');
				const ty = params.length > 1 ? convertUnits(params[1]) : 0;
				output(`offset ${tx},${ty}`);
				break;
			}
			case 'scale': {
				const sx = parseFloat(params[0] || '1');
				const sy = params.length > 1 ? parseFloat(params[1]) : sx;
				if (params.length > 1) {
					output(`scale ${sx},${sy}`);
				} else {
					output(`scale ${sx}`);
				}
				break;
			}
			case 'rotate': {
				const angle = parseFloat(params[0] || '0');
				let cmd = `rotate ${angle}`;
				if (params.length > 2) {
					const cx = convertUnits(params[1]);
					const cy = convertUnits(params[2]);
					cmd += ` anchor:${cx},${cy}`;
				}
				output(cmd);
				break;
			}
			case 'skewX': {
				const angle = parseFloat(params[0] || '0');
				const sx = Math.tan(angle * Math.PI / 180);
				output(`shear ${sx},0`);
				break;
			}
			case 'skewY': {
				const angle = parseFloat(params[0] || '0');
				const sy = Math.tan(angle * Math.PI / 180);
				output(`shear 0,${sy}`);
				break;
			}
			case 'matrix': {
				if (params.length !== 6) {
					warning('matrix requires 6 parameters');
					break;
				}
				const a = parseFloat(params[0]);
				const b = parseFloat(params[1]);
				const c = parseFloat(params[2]);
				const d = parseFloat(params[3]);
				const e = convertUnits(params[4]);
				const f = convertUnits(params[5]);
				output(`matrix ${a},${b},${c},${d},${e},${f}`);
				break;
			}
			default:
			warning('Unsupported transform: ' + type);
		}
	}
}


const LINEJOINS_TO_JOINTS = {
	bevel: 'bevel',
	round: 'curve',
	miter: 'miter',
	'miter-clip': 'miter',
	arcs: 'miter'
};

const SUPPORTED_LINECAPS = new Set(['butt', 'round', 'square']);

function outputPresentationAttributes(attribs) {
	const hasStroke = 'stroke' in attribs;
	const hasStrokeWidth = 'stroke-width' in attribs;
	const hasStrokeLineJoin = 'stroke-linejoin' in attribs;
	const hasStrokeLineCap = 'stroke-linecap' in attribs;
	const hasStrokeMiterlimit = 'stroke-miterlimit' in attribs;
	let baseOpacity = 1.0;
	if ('opacity' in attribs) {
		baseOpacity = convertOpacity(attribs.opacity);
	}
	if (hasStroke || hasStrokeWidth || hasStrokeLineJoin || hasStrokeMiterlimit) {
		let s = 'pen';
		if (hasStroke) {
			s += ' ' + convertPaint(attribs.stroke);
		}
		if (hasStrokeWidth) {
			s += ' width:' + convertUnits(attribs['stroke-width']);
		}
		if (hasStrokeLineJoin) {
			const lj = attribs['stroke-linejoin'];
			if (!(lj in LINEJOINS_TO_JOINTS)) {
				throw new Error('Unrecognized stroke-linejoin: ' + lj);
			}
			s += ' joints:' + LINEJOINS_TO_JOINTS[lj];
		}
		if (hasStrokeLineCap) {
			const lc = attribs['stroke-linecap'];
			if (!SUPPORTED_LINECAPS.has(lc)) {
				throw new Error('Unrecognized stroke-linecap: ' + lc);
			}
			s += ' caps:' + lc;
		}
		if (hasStrokeMiterlimit) {
			s += ' miter-limit:' + attribs['stroke-miterlimit'];
		}
		output(s);
	}
	if ('fill' in attribs) {
		output('fill ' + convertPaint(attribs.fill));
	}
}

function gotKnownPresentationAttributes(attribs) {
	return KNOWN_PRESENTATION_ATTRIBUTES.some(attr => attr in attribs);
}

function createContextMaybe(attribs) {
	const needs = gotKnownPresentationAttributes(attribs) || 'transform' in attribs;
	if (needs) {
		output('context [');
		if ('transform' in attribs) {
			outputTransforms(attribs.transform);
		}
		outputPresentationAttributes(attribs);
	}
	return needs;
}

const converters = {};
let firstSVG = true;

converters.svg = function(element, attribs) {
	let width, height;
	if ('width' in attribs) {
		width = convertUnits(attribs.width);
	} else {
		warning("Missing 'width' attribute. Assuming a width of 800.");
		width = 800;
	}
	if ('height' in attribs) {
		height = convertUnits(attribs.height);
	} else {
		warning("Missing 'height' attribute. Assuming a height of 800.");
		height = 800;
	}
	if (!firstSVG) {
		output('reset');
	}
	firstSVG = false;
	output(`bounds 0,0,${width},${height}`);
	output('fill black');
	output('pen miter-limit:4');
	if ('transform' in attribs) {
		outputTransforms(attribs.transform);
	}
	if ('viewBox' in attribs) {
		const vb = parseRect(attribs.viewBox);
		if (vb.left !== 0 || vb.top !== 0) {
			output(`offset -${vb.left},-${vb.top}`);
		}
		output(`scale ${Math.min(width / vb.width, height / vb.height)}`);
	}
	convertSVGContainer(element);
};

converters.g = function(element, attribs) {
	output('context [');
	if ('transform' in attribs) {
		outputTransforms(attribs.transform);
	}
	outputPresentationAttributes(attribs);
	convertSVGContainer(element);
	output(']');
};

converters.path = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	if ('d' in attribs) {
		output('path svg:[' + attribs.d + ']');
	} else {
		warning("Missing 'd' attribute in 'path' element.");
	}
	if (separate) output(']');
};

converters.circle = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'cx', 'cy', 'r');
	output(`ellipse ${convertUnits(attribs.cx)},${convertUnits(attribs.cy)},${convertUnits(attribs.r)}`);
	if (separate) output(']');
};

converters.ellipse = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'cx', 'cy', 'rx', 'ry');
	output(`ellipse ${convertUnits(attribs.cx)},${convertUnits(attribs.cy)},${convertUnits(attribs.rx)},${convertUnits(attribs.ry)}`);
	if (separate) output(']');
};

converters.line = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'x1', 'y1', 'x2', 'y2');
	output(`path svg:[M${convertUnits(attribs.x1)},${convertUnits(attribs.y1)}L${convertUnits(attribs.x2)},${convertUnits(attribs.y2)}]`);
	if (separate) output(']');
};

converters.rect = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'x', 'y', 'width', 'height');
	let s = `rect ${convertUnits(attribs.x)},${convertUnits(attribs.y)},${convertUnits(attribs.width)},${convertUnits(attribs.height)}`;
	const hasRX = 'rx' in attribs;
	const hasRY = 'ry' in attribs;
	if (hasRX && hasRY) {
		s += ` rounded:${convertUnits(attribs.rx)},${convertUnits(attribs.ry)}`;
	} else if (hasRX) {
		s += ` rounded:${convertUnits(attribs.rx)}`;
	} else if (hasRY) {
		s += ` rounded:${convertUnits(attribs.ry)}`;
	}
	output(s);
	if (separate) output(']');
};

converters.polygon = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'points');
	const pts = parsePoints(attribs.points);
	if (pts.length < 2) {
		warning("Not enough points in 'polygon'.");
	} else {
		let s = `M${pts[0][0]},${pts[0][1]}`;
		for (let i = 1; i < pts.length; i++) {
			s += `L${pts[i][0]},${pts[i][1]}`;
		}
		s += 'Z';
		output('path svg:[' + s + ']');
	}
	if (separate) output(']');
};

converters.polyline = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'points');
	const pts = parsePoints(attribs.points);
	if (pts.length < 2) {
		warning("Not enough points in 'polyline'.");
	} else {
		let s = `M${pts[0][0]},${pts[0][1]}`;
		for (let i = 1; i < pts.length; i++) {
			s += `L${pts[i][0]},${pts[i][1]}`;
		}
		output('path svg:[' + s + ']');
	}
	if (separate) output(']');
};


converters.defs = function(element) {
	convertSVGContainer(element);
};

converters.linearGradient = function(element, attribs) {
	if (!('id' in attribs)) {
		warning("Missing 'id' in linearGradient");
		return;
	}
	const g = {
		type: 'linear',
		x1: parseGradientCoord(attribs.x1 || '0%'),
		y1: parseGradientCoord(attribs.y1 || '0%'),
		x2: parseGradientCoord(attribs.x2 || '100%'),
		y2: parseGradientCoord(attribs.y2 || '0%'),
		stops: parseGradientStops(element),
		relative: attribs.gradientUnits !== 'userSpaceOnUse'
	};
	gradients[attribs.id] = g;
};

converters.radialGradient = function(element, attribs) {
	if (!('id' in attribs)) {
		warning("Missing 'id' in radialGradient");
		return;
	}
	const g = {
		type: 'radial',
		cx: parseGradientCoord(attribs.cx || '50%'),
		cy: parseGradientCoord(attribs.cy || '50%'),
		r: parseGradientCoord(attribs.r || '50%'),
		stops: parseGradientStops(element),
		relative: attribs.gradientUnits !== 'userSpaceOnUse'
	};
	gradients[attribs.id] = g;
};

function convertSVGElement(element) {
	const type = element.type;
	if (converters[type]) {
		converters[type](element, element.attributes);
	} else {
		warning("Can't convert type: " + type);
	}
}

function convertSVGContainer(container) {
	for (const item of container.contents || []) {
		if (item.element) {
			convertSVGElement(item.element);
		}
	}
}

function parseXML(src) {
	let pos = 0;
	const len = src.length;
	
	function skipWS() {
		while (pos < len && /[\s]/.test(src[pos])) pos++;
	}
	
	function parseName() {
		const start = pos;
		while (pos < len && /[-.0-9:_A-Za-z]/.test(src[pos])) pos++;
		return src.slice(start, pos);
	}
	
	function parseAttributes() {
		const attrs = {};
		while (true) {
			skipWS();
			if (pos >= len || src[pos] === '/' || src[pos] === '>') break;
			const name = parseName();
			skipWS();
			if (src[pos] === '=') {
				pos++;
				skipWS();
				const quote = src[pos++];
				const start = pos;
				while (pos < len && src[pos] !== quote) pos++;
				attrs[name] = src.slice(start, pos);
				pos++;
			} else {
				attrs[name] = '';
			}
		}
		return attrs;
	}
	
	function parseNode() {
		if (src.startsWith('<!--', pos)) {
			pos = src.indexOf('-->', pos);
			if (pos < 0) pos = len; else pos += 3;
			return null;
		}
		if (src.startsWith('<?', pos)) {
			pos = src.indexOf('?>', pos);
			if (pos < 0) pos = len; else pos += 2;
			return null;
		}
		if (src.startsWith('<!', pos)) {
			pos = src.indexOf('>', pos);
			if (pos < 0) pos = len; else pos += 1;
			return null;
		}
		return parseElement();
	}
	
	function parseElement() {
		if (src[pos] !== '<') return null;
		pos++;
		const name = parseName();
		const attrs = parseAttributes();
		let selfClose = false;
		if (src[pos] === '/') {
			selfClose = true;
			pos++;
		}
		if (src[pos] !== '>') throw new Error('Malformed XML');
		pos++;
		const contents = [];
		if (!selfClose) {
			while (true) {
				skipWS();
				if (src.startsWith('</' + name, pos)) {
					pos += name.length + 2;
					skipWS();
					if (src[pos] === '>') pos++; else throw new Error('Malformed XML');
					break;
				} else if (src[pos] === '<') {
					const child = parseNode();
					if (child) contents.push({element: child});
				} else {
					const start = pos;
					while (pos < len && src[pos] !== '<') pos++;
					const text = src.slice(start, pos);
					if (text) contents.push({text});
				}
			}
		}
		return {type: name, attributes: attrs, contents};
	}
	
	const contents = [];
	while (pos < len) {
		skipWS();
		if (pos >= len) break;
		if (src[pos] === '<') {
			const el = parseNode();
			if (el) contents.push({element: el});
		} else {
			pos++;
		}
	}
	return {contents};
}

const args = process.argv.slice(2);
if (args.length < 1) {
	console.error('Usage: node svg2ivg.js input.svg [output.ivg]');
	process.exit(1);
}
const svgPath = args[0];
const ivgPath = args[1];
const svgSource = fs.readFileSync(svgPath, 'utf8');
const svg = parseXML(svgSource);

output('format IVG-1 requires:IMPD-1');
convertSVGContainer(svg);
if (ivgPath) {
	fs.writeFileSync(ivgPath, outputString, 'utf8');
	console.log('Converted ' + svgPath + ' to ' + ivgPath);
} else {
	console.log('------');
	console.log(outputString);
}

