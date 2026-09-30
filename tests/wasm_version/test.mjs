// Node test: the WASM module reports the CDFpp version it was built from.
//   node test.mjs <path-to-cdfpp.js> <expected version>
import process from "node:process";

const [, , modulePath, expected] = process.argv;
if (!modulePath || !expected)
{
    console.error("usage: node test.mjs <cdfpp.js> <expected version>");
    process.exit(2);
}

const { default: createCdfModule } = await import(modulePath);
const Module = await createCdfModule();
const version = typeof Module.version === "function" ? Module.version() : undefined;
if (version !== expected)
{
    console.error(`FAIL version: got ${version}, expected ${expected}`);
    process.exit(1);
}
console.log(`ok   version ${version}`);
