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

function quoteIMPD(value) {
	return /^[A-Za-z0-9_-]+$/.test(value) ? value : `[${value.replace(/]/g, '\\]')}]`;
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
'fill',
'opacity',
'fill-opacity',
'stroke-opacity',
'fill-rule'
];

function rgbToHex(r, g, b) {
const toHex = n => Math.max(0, Math.min(255, Math.round(n))).toString(16).padStart(2, '0');
return '#' + toHex(r) + toHex(g) + toHex(b);
}

function hslToRgb(h, s, l) {
h = ((h % 360) + 360) % 360 / 360;
s = Math.max(0, Math.min(1, s));
l = Math.max(0, Math.min(1, l));
const c = (1 - Math.abs(2 * l - 1)) * s;
const x = c * (1 - Math.abs((h * 6) % 2 - 1));
const m = l - c / 2;
let r, g, b;
if (h < 1 / 6) {
r = c; g = x; b = 0;
	} else if (h < 2 / 6) {
r = x; g = c; b = 0;
	} else if (h < 3 / 6) {
r = 0; g = c; b = x;
	} else if (h < 4 / 6) {
r = 0; g = x; b = c;
	} else if (h < 5 / 6) {
r = x; g = 0; b = c;
		} else {
r = c; g = 0; b = x;
}
return { r: (r + m) * 255, g: (g + m) * 255, b: (b + m) * 255 };
}

const SVG_COLORS = {
	aliceblue:"#f0f8ff",antiquewhite:"#faebd7",aquamarine:"#7fffd4",azure:"#f0ffff",beige:"#f5f5dc",bisque:"#ffe4c4",blanchedalmond:"#ffebcd",blueviolet:"#8a2be2",brown:"#a52a2a",burlywood:"#deb887",cadetblue:"#5f9ea0",chartreuse:"#7fff00",chocolate:"#d2691e",coral:"#ff7f50",cornflowerblue:"#6495ed",cornsilk:"#fff8dc",crimson:"#dc143c",cyan:"#00ffff",darkblue:"#00008b",darkcyan:"#008b8b",darkgoldenrod:"#b8860b",darkgray:"#a9a9a9",darkgreen:"#006400",darkgrey:"#a9a9a9",darkkhaki:"#bdb76b",darkmagenta:"#8b008b",darkolivegreen:"#556b2f",darkorange:"#ff8c00",darkorchid:"#9932cc",darkred:"#8b0000",darksalmon:"#e9967a",darkseagreen:"#8fbc8f",darkslateblue:"#483d8b",darkslategray:"#2f4f4f",darkslategrey:"#2f4f4f",darkturquoise:"#00ced1",darkviolet:"#9400d3",deeppink:"#ff1493",deepskyblue:"#00bfff",dimgray:"#696969",dimgrey:"#696969",dodgerblue:"#1e90ff",firebrick:"#b22222",floralwhite:"#fffaf0",forestgreen:"#228b22",gainsboro:"#dcdcdc",ghostwhite:"#f8f8ff",gold:"#ffd700",goldenrod:"#daa520",grey:"#808080",greenyellow:"#adff2f",honeydew:"#f0fff0",hotpink:"#ff69b4",indianred:"#cd5c5c",indigo:"#4b0082",ivory:"#fffff0",khaki:"#f0e68c",lavender:"#e6e6fa",lavenderblush:"#fff0f5",lawngreen:"#7cfc00",lemonchiffon:"#fffacd",lightblue:"#add8e6",lightcoral:"#f08080",lightcyan:"#e0ffff",lightgoldenrodyellow:"#fafad2",lightgray:"#d3d3d3",lightgreen:"#90ee90",lightgrey:"#d3d3d3",lightpink:"#ffb6c1",lightsalmon:"#ffa07a",lightseagreen:"#20b2aa",lightskyblue:"#87cefa",lightslategray:"#778899",lightslategrey:"#778899",lightsteelblue:"#b0c4de",lightyellow:"#ffffe0",limegreen:"#32cd32",linen:"#faf0e6",magenta:"#ff00ff",mediumaquamarine:"#66cdaa",mediumblue:"#0000cd",mediumorchid:"#ba55d3",mediumpurple:"#9370db",mediumseagreen:"#3cb371",mediumslateblue:"#7b68ee",mediumspringgreen:"#00fa9a",mediumturquoise:"#48d1cc",mediumvioletred:"#c71585",midnightblue:"#191970",mintcream:"#f5fffa",mistyrose:"#ffe4e1",moccasin:"#ffe4b5",navajowhite:"#ffdead",oldlace:"#fdf5e6",olivedrab:"#6b8e23",orange:"#ffa500",orangered:"#ff4500",orchid:"#da70d6",palegoldenrod:"#eee8aa",palegreen:"#98fb98",paleturquoise:"#afeeee",palevioletred:"#db7093",papayawhip:"#ffefd5",peachpuff:"#ffdab9",peru:"#cd853f",pink:"#ffc0cb",plum:"#dda0dd",powderblue:"#b0e0e6",rosybrown:"#bc8f8f",royalblue:"#4169e1",saddlebrown:"#8b4513",salmon:"#fa8072",sandybrown:"#f4a460",seagreen:"#2e8b57",seashell:"#fff5ee",sienna:"#a0522d",skyblue:"#87ceeb",slateblue:"#6a5acd",slategray:"#708090",slategrey:"#708090",snow:"#fffafa",springgreen:"#00ff7f",steelblue:"#4682b4",tan:"#d2b48c",thistle:"#d8bfd8",tomato:"#ff6347",turquoise:"#40e0d0",violet:"#ee82ee",wheat:"#f5deb3",whitesmoke:"#f5f5f5",yellowgreen:"#9acd32"
};

const BASIC_COLORS = {aqua:'#00ffff', black:'#000000', blue:'#0000ff', fuchsia:'#ff00ff', gray:'#808080', green:'#008000', lime:'#00ff00', maroon:'#800000', navy:'#000080', olive:'#808000', purple:'#800080', red:'#ff0000', silver:'#c0c0c0', teal:'#008080', white:'#ffffff', yellow:'#ffff00'};
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
		return { paint: buildGradient(gradients[id]), opacity: 1 };
	}
	if (sourcePaint === 'none') {
		return { paint: 'none', opacity: 1 };
	}
	if (/^#[0-9a-f]{6}$/.test(sourcePaint)) {
		return { paint: sourcePaint, opacity: 1 };
	}
	const rgb3 = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i;
	let m = sourcePaint.match(rgb3);
	if (m) {
		return { paint: '#' + m[1] + m[1] + m[2] + m[2] + m[3] + m[3], opacity: 1 };
	}
	m = sourcePaint.match(/^rgba\(([^)]+)\)$/);
	if (m) {
		const nums = m[1].split(/[ ,]+/);
		if (nums.length !== 4) throw new Error('Invalid rgba color: ' + sourcePaint);
		return { paint: rgbToHex(parseFloat(nums[0]), parseFloat(nums[1]), parseFloat(nums[2])), opacity: parseFloat(nums[3]) };
	}
	m = sourcePaint.match(/^rgb\(([^)]+)\)$/);
	if (m) {
		const nums = m[1].split(/[ ,]+/);
		if (nums.length !== 3) throw new Error('Invalid rgb color: ' + sourcePaint);
		return { paint: rgbToHex(parseFloat(nums[0]), parseFloat(nums[1]), parseFloat(nums[2])), opacity: 1 };
	}
	m = sourcePaint.match(/^hsla\(([^)]+)\)$/);
	if (m) {
		const nums = m[1].split(/[ ,]+/);
		if (nums.length !== 4) throw new Error('Invalid hsla color: ' + sourcePaint);
		const rgb = hslToRgb(parseFloat(nums[0]), parseFloat(nums[1]) / 100, parseFloat(nums[2]) / 100);
		return { paint: rgbToHex(rgb.r, rgb.g, rgb.b), opacity: parseFloat(nums[3]) };
	}
	m = sourcePaint.match(/^hsl\(([^)]+)\)$/);
	if (m) {
		const nums = m[1].split(/[ ,]+/);
		if (nums.length !== 3) throw new Error('Invalid hsl color: ' + sourcePaint);
		const rgb = hslToRgb(parseFloat(nums[0]), parseFloat(nums[1]) / 100, parseFloat(nums[2]) / 100);
		return { paint: rgbToHex(rgb.r, rgb.g, rgb.b), opacity: 1 };
	}
	if (sourcePaint in BASIC_COLORS) {
		return { paint: BASIC_COLORS[sourcePaint], opacity: 1 };
	}
	if (sourcePaint in SVG_COLORS) {
		return { paint: SVG_COLORS[sourcePaint], opacity: 1 };
	}
	throw new Error('Unrecognized color: ' + sourcePaint);
}

const definitions = {};
let viewportWidth = 100;
let viewportHeight = 100;
let defaultWidth = 800;
let defaultHeight = 800;
let defaultFontFamily = 'serif';
let defaultFontSize = 16;

function convertUnits(value, axis) {
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
	case '%':
		if (axis === 'y') {
			return (viewportHeight || 100) * num / 100;
		}
		return (viewportWidth || 100) * num / 100;
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
			convertUnits(nums[i], 'x'),
			convertUnits(nums[i + 1], 'y')
		]);
	}
	return points;
}

function parseGradientCoord(value, axis) {
	value = value.trim();
	if (value.endsWith('%')) {
		return parseFloat(value) / 100;
	}
	return convertUnits(value, axis);
}

function parseGradientStops(element) {
	const stops = [];
	for (const item of element.contents || []) {
		if (item.element && item.element.type === 'stop') {
			const a = item.element.attributes;
			if (!('offset' in a) || !('stop-color' in a)) continue;
			let o = a.offset.trim();
			o = o.endsWith('%') ? parseFloat(o) / 100 : parseFloat(o);
			const p = convertPaint(a['stop-color']);
			let op = p.opacity;
			if ('stop-opacity' in a) {
				op *= convertOpacity(a['stop-opacity']);
			}
			let color = p.paint;
			if (op !== 1) {
				const alpha = Math.round(op * 255).toString(16).padStart(2, '0');
				if (color.startsWith('#')) {
					color += alpha;
					} else {
						color = color + alpha;
					}
				}
				stops.push({offset: o, color});
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
		if (g.transform) {
			s += ` transform:[${transformToCommandString(g.transform)}]`;
		}
		if (g.relative) s += ' relative:yes';
		return s;
}


function transformToCommands(str) {
	const re = /(translate|scale|rotate|skewX|skewY|matrix)\(([^)]*)\)/g;
	const cmds = [];
	let m;
	while ((m = re.exec(str)) !== null) {
		const type = m[1];
		const params = m[2].trim().split(/[ ,]+/).filter(p => p.length);
		switch (type) {
			case 'translate': {
				const tx = convertUnits(params[0] || '0', 'x');
				const ty = params.length > 1 ? convertUnits(params[1], 'y') : 0;
				cmds.push(`offset ${tx},${ty}`);
				break;
			}
			case 'scale': {
				const sx = parseFloat(params[0] || '1');
				const sy = params.length > 1 ? parseFloat(params[1]) : sx;
				if (params.length > 1) {
					cmds.push(`scale ${sx},${sy}`);
				} else {
					cmds.push(`scale ${sx}`);
				}
				break;
			}
			case 'rotate': {
				const angle = parseFloat(params[0] || '0');
				let cmd = `rotate ${angle}`;
				if (params.length > 2) {
					const cx = convertUnits(params[1], 'x');
					const cy = convertUnits(params[2], 'y');
					cmd += ` anchor:${cx},${cy}`;
				}
				cmds.push(cmd);
				break;
			}
			case 'skewX': {
				const angle = parseFloat(params[0] || '0');
				const sx = Math.tan(angle * Math.PI / 180);
				cmds.push(`shear ${sx},0`);
				break;
			}
			case 'skewY': {
				const angle = parseFloat(params[0] || '0');
				const sy = Math.tan(angle * Math.PI / 180);
				cmds.push(`shear 0,${sy}`);
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
				const e = convertUnits(params[4], 'x');
				const f = convertUnits(params[5], 'y');
				cmds.push(`matrix ${a},${b},${c},${d},${e},${f}`);
				break;
			}
			default:
			warning('Unsupported transform: ' + type);
		}
	}
	return cmds;
}

function transformToCommandString(str) {
return transformToCommands(str).join('; ');
}

function outputTransforms(str) {
	for (const cmd of transformToCommands(str)) {
		output(cmd);
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
	let strokeOpacity = baseOpacity;
	let fillOpacity = baseOpacity;
	let strokePaint = null;
	if (hasStroke) {
		strokePaint = convertPaint(attribs.stroke);
		strokeOpacity *= strokePaint.opacity;
	}
	if ('stroke-opacity' in attribs) {
		strokeOpacity *= convertOpacity(attribs['stroke-opacity']);
	}
        let fillPaint = null;
        if ('fill' in attribs) {
                fillPaint = convertPaint(attribs.fill);
                fillOpacity *= fillPaint.opacity;
        }
        if ('fill-opacity' in attribs) {
                fillOpacity *= convertOpacity(attribs['fill-opacity']);
        }
        let fillRule = null;
        if ('fill-rule' in attribs) {
                const fr = attribs['fill-rule'].trim().toLowerCase();
                if (fr === 'evenodd') {
                        fillRule = 'even-odd';
                } else if (fr !== 'nonzero') {
                        throw new Error('Unrecognized fill-rule: ' + attribs['fill-rule']);
                }
        }
	if (hasStroke || hasStrokeWidth || hasStrokeLineJoin || hasStrokeMiterlimit || strokeOpacity !== 1) {
		let s = 'pen';
		if (strokePaint) {
			s += ' ' + strokePaint.paint;
		}
		if (hasStrokeWidth) {
			s += ' width:' + convertUnits(attribs['stroke-width'], 'x');
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
		if (strokeOpacity !== 1) {
			s += ' opacity:' + strokeOpacity;
		}
		output(s);
	}
        if (fillPaint || fillOpacity !== 1 || fillRule) {
                let s = 'fill';
                if (fillPaint) {
                        s += ' ' + fillPaint.paint;
                }
                if (fillRule) {
                        s += ' rule:' + fillRule;
                }
                if (fillOpacity !== 1) {
                        s += ' opacity:' + fillOpacity;
                }
                output(s);
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

function registerDefinition(element) {
	if (element.attributes && element.attributes.id) {
		definitions[element.attributes.id] = JSON.parse(JSON.stringify(element));
	}
}

const converters = {};
let firstSVG = true;


converters.svg = function(element, attribs) {
	let width, height;
	if ('width' in attribs) {
		width = convertUnits(attribs.width, 'x');
		} else {
		warning(`Missing 'width' attribute. Assuming a width of ${defaultWidth}.`);
		width = defaultWidth;
	}
	if ('height' in attribs) {
		height = convertUnits(attribs.height, 'y');
		} else {
		warning(`Missing 'height' attribute. Assuming a height of ${defaultHeight}.`);
		height = defaultHeight;
	}
	viewportWidth = width;
	viewportHeight = height;
	if (!firstSVG) {
		output('reset');
	}
	firstSVG = false;
	output(`bounds 0,0,${width},${height}`);
	output('fill black');
	output('pen miter-limit:4');
	const oldFamily = defaultFontFamily;
	const oldSize = defaultFontSize;
	if ('font-family' in attribs) {
		defaultFontFamily = attribs['font-family'].split(',')[0].trim().replace(/^['"]|['"]$/g, '');
}
	if ('font-size' in attribs) {
		defaultFontSize = convertUnits(attribs['font-size'], 'y');
}
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
	defaultFontFamily = oldFamily;
	defaultFontSize = oldSize;
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
	output(`ellipse ${convertUnits(attribs.cx, 'x')},${convertUnits(attribs.cy, 'y')},${convertUnits(attribs.r, 'x')}`);
	if (separate) output(']');
};

converters.ellipse = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'cx', 'cy', 'rx', 'ry');
	output(`ellipse ${convertUnits(attribs.cx, 'x')},${convertUnits(attribs.cy, 'y')},${convertUnits(attribs.rx, 'x')},${convertUnits(attribs.ry, 'y')}`);
	if (separate) output(']');
};

converters.line = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'x1', 'y1', 'x2', 'y2');
	output(`path svg:[M${convertUnits(attribs.x1, 'x')},${convertUnits(attribs.y1, 'y')}L${convertUnits(attribs.x2, 'x')},${convertUnits(attribs.y2, 'y')}]`);
	if (separate) output(']');
};

converters.rect = function(element, attribs) {
	const separate = createContextMaybe(attribs);
	checkRequiredAttributes(attribs, 'x', 'y', 'width', 'height');
	let s = `rect ${convertUnits(attribs.x, 'x')},${convertUnits(attribs.y, 'y')},${convertUnits(attribs.width, 'x')},${convertUnits(attribs.height, 'y')}`;
	const hasRX = 'rx' in attribs;
	const hasRY = 'ry' in attribs;
	if (hasRX && hasRY) {
		s += ` rounded:${convertUnits(attribs.rx, 'x')},${convertUnits(attribs.ry, 'y')}`;
	} else if (hasRX) {
		s += ` rounded:${convertUnits(attribs.rx, 'x')}`;
	} else if (hasRY) {
		s += ` rounded:${convertUnits(attribs.ry, 'y')}`;
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

function applyTextAttributes(base, attribs) {
  const out = Object.assign({}, base);
  if ('font-family' in attribs) {
    out.fontName = attribs['font-family'].split(',')[0].trim().replace(/^['"]|['"]$/g, '');
  }
  if ('font-size' in attribs) {
    out.size = convertUnits(attribs['font-size'], 'y');
  }
  if ('fill' in attribs) {
    const fillPaint = convertPaint(attribs.fill);
    out.fill = fillPaint.paint;
    out.fillOpacity = fillPaint.opacity;
  }
  if ('opacity' in attribs) {
    out.fillOpacity *= convertOpacity(attribs.opacity);
  }
  if ('fill-opacity' in attribs) {
    out.fillOpacity *= convertOpacity(attribs['fill-opacity']);
  }
  if ('stroke' in attribs && attribs.stroke !== 'none') {
    const strokePaint = convertPaint(attribs.stroke);
    out.stroke = strokePaint.paint;
    out.strokeOpacity = strokePaint.opacity;
  }
  if ('stroke-opacity' in attribs && out.stroke) {
    out.strokeOpacity *= convertOpacity(attribs['stroke-opacity']);
  }
  if ('stroke-width' in attribs && out.stroke) {
    out.strokeWidth = convertUnits(attribs['stroke-width'], 'x');
  }
  if ('stroke-linejoin' in attribs && out.stroke) {
    const lj = attribs['stroke-linejoin'];
    if (!(lj in LINEJOINS_TO_JOINTS)) {
      throw new Error('Unrecognized stroke-linejoin: ' + lj);
    }
    out.strokeJoin = LINEJOINS_TO_JOINTS[lj];
  }
  if ('stroke-linecap' in attribs && out.stroke) {
    const lc = attribs['stroke-linecap'];
    if (!SUPPORTED_LINECAPS.has(lc)) {
      throw new Error('Unrecognized stroke-linecap: ' + lc);
    }
    out.strokeCap = lc;
  }
  if ('stroke-miterlimit' in attribs && out.stroke) {
    out.strokeMiter = parseFloat(attribs['stroke-miterlimit']);
  }
  return out;
}

function collectTextSegments(element, baseAttribs) {
  const segments = [];
  const attribs = applyTextAttributes(baseAttribs, element.attributes || {});
  let prevEndsWithSpace = false;
  for (const item of element.contents || []) {
    if (item.text) {
      let text = item.text.replace(/\s+/g, ' ');
      if (!text) continue;
      if (segments.length && !prevEndsWithSpace && !/^\s/.test(text)) {
        text = ' ' + text;
      }
      prevEndsWithSpace = /\s$/.test(text);
      segments.push({ text, attribs });
    } else if (item.element && item.element.type === 'tspan') {
      const childSegs = collectTextSegments(item.element, attribs);
      if (childSegs.length) {
        if (segments.length && !prevEndsWithSpace && !childSegs[0].text.startsWith(' ')) {
          childSegs[0].text = ' ' + childSegs[0].text;
        }
        prevEndsWithSpace = childSegs[childSegs.length - 1].text.endsWith(' ');
        segments.push(...childSegs);
      }
    } else if (item.element) {
      warning('Unsupported nested element in text');
    }
  }
  if (segments.length) {
    segments[0].text = segments[0].text.replace(/^\s+/, '');
    segments[segments.length - 1].text = segments[segments.length - 1].text.replace(/\s+$/, '');
  }
  return segments.filter(s => s.text !== '');
}

converters.text = function(element, attribs) {
  const separate = 'transform' in attribs;
  if (separate) {
    output('context [');
    outputTransforms(attribs.transform);
  }
  const baseAttribs = applyTextAttributes({
    fontName: defaultFontFamily,
    size: defaultFontSize,
    fill: 'black',
    fillOpacity: 1
  }, attribs);
  const segments = collectTextSegments(element, baseAttribs);
  if (!segments.length) {
    if (separate) output(']');
    return;
  }
  let fontKey = '';
  let x = 'x' in attribs ? convertUnits(attribs.x, 'x') : 0;
  let y = 'y' in attribs ? convertUnits(attribs.y, 'y') : 0;
  let anchor = '';
  if ('text-anchor' in attribs) {
    switch (attribs['text-anchor']) {
    case 'middle': anchor = ' anchor:center'; break;
    case 'end': anchor = ' anchor:right'; break;
		}
	}
	let needAt = true;
	for (const seg of segments) {
			const key = JSON.stringify(seg.attribs);
			if (key !== fontKey) {
				let color = seg.attribs.fill;
				if (/[:\s]/.test(color) && color !== 'none') {
					color = `[${color}]`;
				}
				let fontCmd = `font ${quoteIMPD(seg.attribs.fontName)} size:${seg.attribs.size} color:${color}`;
				if (seg.attribs.fillOpacity !== 1) {
					fontCmd += ` opacity:${seg.attribs.fillOpacity}`;
				}
				if (seg.attribs.stroke) {
					let outline = seg.attribs.stroke;
					let opts = '';
					if (seg.attribs.strokeWidth) {
						opts += ` width:${seg.attribs.strokeWidth}`;
					}
					if (seg.attribs.strokeJoin) {
						opts += ` joints:${seg.attribs.strokeJoin}`;
					}
					if (seg.attribs.strokeCap) {
						opts += ` caps:${seg.attribs.strokeCap}`;
					}
					if (seg.attribs.strokeOpacity && seg.attribs.strokeOpacity !== 1) {
						opts += ` opacity:${seg.attribs.strokeOpacity}`;
					}
					if (seg.attribs.strokeMiter) {
						opts += ` miter:${seg.attribs.strokeMiter}`;
					}
					if (opts || /[:\s]/.test(outline)) {
						fontCmd += ` outline:[${outline}${opts}]`;
					} else {
						fontCmd += ` outline:${outline}`;
					}
				}
				output(fontCmd);
				fontKey = key;
			}
    const t = seg.text.replace(/"/g, '\\"');
    if (needAt) {
      output(`TEXT at:${x},${y}${anchor} "${t}"`);
      needAt = false;
    } else {
      output(`TEXT "${t}"`);
    }
  }
  if (separate) output(']');
};


converters.defs = function(element) {
	for (const item of element.contents || []) {
		if (!item.element) continue;
		const child = item.element;
		if (child.type === 'linearGradient' || child.type === 'radialGradient' || child.type === 'defs') {
			convertSVGElement(child);
		} else {
			registerDefinition(child);
		}
	}
};

converters.use = function(element, attribs) {
	let ref = attribs.href || attribs['xlink:href'];
	if (!ref) {
		warning("Missing 'href' in use");
		return;
	}
	if (ref.startsWith('#')) {
		ref = ref.slice(1);
	}
	if (!(ref in definitions)) {
		warning('Unrecognized reference: ' + ref);
		return;
	}
	const clone = JSON.parse(JSON.stringify(definitions[ref]));
	for (const [k, v] of Object.entries(attribs)) {
		if (k === 'href' || k === 'xlink:href' || k === 'x' || k === 'y' || k === 'transform') {
			continue;
		}
		clone.attributes[k] = v;
	}
	const needsContext = 'x' in attribs || 'y' in attribs || 'transform' in attribs;
	if (needsContext) {
		output('context [');
		if ('transform' in attribs) {
			outputTransforms(attribs.transform);
		}
		const tx = 'x' in attribs ? convertUnits(attribs.x, 'x') : 0;
		const ty = 'y' in attribs ? convertUnits(attribs.y, 'y') : 0;
		if (tx !== 0 || ty !== 0) {
			output(`offset ${tx},${ty}`);
		}
	}
	delete clone.attributes.id;
	convertSVGElement(clone);
	if (needsContext) {
		output(']');
	}
};

converters.linearGradient = function(element, attribs) {
	if (!('id' in attribs)) {
		warning("Missing 'id' in linearGradient");
		return;
	}
	const g = {
		type: 'linear',
		x1: parseGradientCoord(attribs.x1 || '0%', 'x'),
		y1: parseGradientCoord(attribs.y1 || '0%', 'y'),
		x2: parseGradientCoord(attribs.x2 || '100%', 'x'),
		y2: parseGradientCoord(attribs.y2 || '0%', 'y'),
		stops: parseGradientStops(element),
		relative: attribs.gradientUnits !== 'userSpaceOnUse'
	};
	if ('gradientTransform' in attribs) {
		g.transform = attribs.gradientTransform;
	}
	gradients[attribs.id] = g;
};

converters.radialGradient = function(element, attribs) {
	if (!('id' in attribs)) {
		warning("Missing 'id' in radialGradient");
		return;
	}
	const g = {
		type: 'radial',
		cx: parseGradientCoord(attribs.cx || '50%', 'x'),
		cy: parseGradientCoord(attribs.cy || '50%', 'y'),
		r: parseGradientCoord(attribs.r || '50%', 'x'),
		stops: parseGradientStops(element),
		relative: attribs.gradientUnits !== 'userSpaceOnUse'
	};
	if ('gradientTransform' in attribs) {
		g.transform = attribs.gradientTransform;
	}
	gradients[attribs.id] = g;
};

function convertSVGElement(element) {
	registerDefinition(element);
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
	console.error('Usage: node svg2ivg.js input.svg [output.ivg] [defaultWidth,defaultHeight]');
	process.exit(1);
}
const svgPath = args[0];
let ivgPath;
let defaultDimArg;
if (args[1]) {
	if (args[1].includes(',')) {
		defaultDimArg = args[1];
	} else {
		ivgPath = args[1];
		defaultDimArg = args[2];
	}
}
if (defaultDimArg) {
	const parts = defaultDimArg.split(',');
	if (parts.length === 2) {
		defaultWidth = parseFloat(parts[0]);
		defaultHeight = parseFloat(parts[1]);
	} else {
		console.error('Invalid default dimensions: ' + defaultDimArg);
		process.exit(1);
	}
}
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

