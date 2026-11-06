'use strict';

// Regenerates BuiltInFonts.h from the fonts in fonts/.

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
const FONTS = [
	[ 'serif', 'SERIF_SOURCE' ],
	[ 'sans-serif', 'SANS_SERIF_SOURCE' ],
	[ 'monospace', 'MONOSPACE_SOURCE' ]
];

function cStringLines(text) {
	const lines = text.replace(/\r\n/g, '\n').split('\n');
	if (lines[lines.length - 1] === '') {
		lines.pop();
	}
	return lines.map((line, i) => '\t"' + line.replace(/\\/g, '\\\\').replace(/"/g, '\\"')
			+ (i < lines.length - 1 ? '\\n' : '') + '"').join('\n');
}

let out = '// Generated from fonts/*.ivgfont by updateBuiltInFonts.sh. Do not edit.\n'
		+ '\n'
		+ '#ifndef TOOLS_IVGSNAPSHOT_BUILTINFONTS_H\n'
		+ '#define TOOLS_IVGSNAPSHOT_BUILTINFONTS_H\n'
		+ '\n'
		+ '#include <cstddef>\n'
		+ '#include <string>\n'
		+ '\n'
		+ 'namespace IVGSnapshotBuiltInFonts {\n'
		+ '\n'
		+ 'struct FontEntry {\n'
		+ '\tconst char *name;\n'
		+ '\tconst char *source;\n'
		+ '\tsize_t length;\n'
		+ '};\n'
		+ '\n';
for (const [ name, symbol ] of FONTS) {
	const text = fs.readFileSync(path.join(ROOT, 'fonts', name + '.ivgfont'), 'utf8');
	out += 'static const char ' + symbol + '[] = \n' + cStringLines(text) + ';\n\n';
}
out += 'static const FontEntry ENTRIES[] = {\n';
for (const [ name, symbol ] of FONTS) {
	out += '\t{ "' + name + '", ' + symbol + ', sizeof(' + symbol + ') - 1 },\n';
}
out += '};\n'
		+ '\n'
		+ 'inline const FontEntry *find(const std::string &name) {\n'
		+ '\tfor (size_t i = 0; i < sizeof(ENTRIES) / sizeof(ENTRIES[0]); ++i) {\n'
		+ '\t\tif (name == ENTRIES[i].name) {\n'
		+ '\t\t\treturn &ENTRIES[i];\n'
		+ '\t\t}\n'
		+ '\t}\n'
		+ '\treturn 0;\n'
		+ '}\n'
		+ '\n'
		+ '} // namespace IVGSnapshotBuiltInFonts\n'
		+ '\n'
		+ '#endif // TOOLS_IVGSNAPSHOT_BUILTINFONTS_H\n';

fs.writeFileSync(path.join(__dirname, 'BuiltInFonts.h'), out);
