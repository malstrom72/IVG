const assert = require("assert");
const core = require("../QuickHashGenCore");

const input = 'alpha\n\n  \n beta \n"gamma delta"\n\t\n';
const result = core.parseQuickHashGenInput(input);
assert.deepStrictEqual(result, ["alpha", "beta", "gamma delta"]);
console.log("parseQuickHashGenInput empty lines test passed");
