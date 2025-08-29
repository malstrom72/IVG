const assert = require("assert");
const core = require("../QuickHashGenCore");

const input = '"' + "\\";
assert.throws(() => core.parseCString(input), /Unterminated C escape sequence/);
console.log("parseCString trailing backslash test passed");
