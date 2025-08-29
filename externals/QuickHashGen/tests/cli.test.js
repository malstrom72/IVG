const assert = require("assert");
const fs = require("fs");
const path = require("path");
const { execFileSync } = require("child_process");

const cli = path.join(__dirname, "..", "QuickHashGenCLI.node.js");

function runCli(args) {
	return execFileSync("node", [cli].concat(args), { encoding: "utf8" });
}

const out1 = runCli(["--seed", "1", path.join(__dirname, "input1.txt")]);
const golden1 = fs.readFileSync(path.join(__dirname, "golden1.c"), "utf8");
assert.strictEqual(out1, golden1);
console.log("CLI golden1 test passed");

const out2 = runCli(["--seed", "123", path.join(__dirname, "input2.txt")]);
const golden2 = fs.readFileSync(path.join(__dirname, "golden2.c"), "utf8");
assert.strictEqual(out2, golden2);
console.log("CLI golden2 test passed");

const outNZ = runCli(["--seed", "1", "--tests", "10", "--no-zero-termination", path.join(__dirname, "input1.txt")]);
assert(outNZ.includes("zero termination not required"));
assert(outNZ.includes("strncmp"));
console.log("CLI zero-termination toggle test passed");
