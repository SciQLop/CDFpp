// Node test: the Explorer's converter (WASM build).
//
// save_as(codec) re-encodes every variable of a loaded file with one codec,
// load_eager() reloads it with every value decoded, and same_values() checks the
// round trip byte for byte. The experimental codecs (zstd, blosc2) must be exposed
// when the build enables them, so the converter can offer them.
//
//   node test.mjs <path-to-cdfpp.js>

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import process from "node:process";

const [, , modulePath] = process.argv;
if (!modulePath)
{
    console.error("usage: node test.mjs <cdfpp.js>");
    process.exit(2);
}

const resourcesDir = join(dirname(fileURLToPath(import.meta.url)), "..", "resources");

const { default: createCdfModule } = await import(modulePath);
const Module = await createCdfModule();

let failures = 0;
function check(name, ok)
{
    if (ok)
        console.log(`ok   ${name}`);
    else
    {
        failures += 1;
        console.error(`FAIL ${name}`);
    }
}

// A file with real records: a zero-record skeleton gives codecs nothing to compress.
const RESOURCE = "ge_k0_cpi_19921231_v02.cdf";
const original = Module.load(new Uint8Array(readFileSync(join(resourcesDir, RESOURCE))));
check(`load(${RESOURCE})`, original.is_valid());

const codecs = ["none", "gzip", "zstd", "blosc2"];
check("zstd and blosc2 are exposed", codecs.every((c) => Module.CompressionType[c] !== undefined));

const sizes = {};
for (const codec of codecs.filter((c) => Module.CompressionType[c] !== undefined))
{
    const bytes = original.save_as(Module.CompressionType[codec]);
    check(`save_as(${codec}) returns bytes`, bytes instanceof Uint8Array && bytes.length > 0);
    if (!(bytes instanceof Uint8Array))
        continue;
    sizes[codec] = bytes.length;
    const reloaded = Module.load_eager(bytes);
    check(`load_eager(${codec} output) is valid`, reloaded.is_valid());
    check(`${codec} round trip keeps every value`, reloaded.same_values(original));
    const first = reloaded.get_variable(reloaded.variable_names()[0]);
    check(`${codec} output is stored as ${codec}`,
        codec === "none" ? first.compression === "None" : first.compression !== "None");
    reloaded.delete();
}
check("blosc2 output is smaller than uncompressed", sizes.blosc2 < sizes.none);

// C++ exceptions must reach JS as Error objects carrying the C++ message, not as an
// opaque WebAssembly.Exception (the converter used to show "[object WebAssembly.Exception]").
{
    let error;
    try { original.save_as(Module.CompressionType.huffman); }
    catch (e) { error = e; }
    check("save_as with an unsupported codec throws an Error",
        error instanceof Error && /Unsupported compression algorithm/.test(error.message));
    check("the loaded file keeps its codecs after a failed save_as",
        original.get_variable(original.variable_names()[0]).compression !== "Huffman");
}

// decoded_nbytes() sizes the decoded values from shapes, without loading them, so the
// converter can refuse files that cannot fit the 4 GiB WASM memory up front.
{
    const lazy = Module.load(new Uint8Array(readFileSync(join(resourcesDir, RESOURCE))));
    const estimate = lazy.decoded_nbytes();
    const untouched = lazy.variable_names().every((n) => !lazy.get_variable(n).values_loaded);
    const actual = lazy.variable_names().reduce((sum, n) => sum + (lazy.get_variable(n).values?.byteLength ?? 0), 0);
    check("decoded_nbytes does not load values", untouched);
    check("decoded_nbytes matches the decoded size", estimate === actual);
    lazy.delete();
}
check("save_as leaves the loaded file untouched",
    original.get_variable(original.variable_names()[0]).compression
        === Module.load(new Uint8Array(readFileSync(join(resourcesDir, RESOURCE))))
            .get_variable(original.variable_names()[0]).compression);
original.delete();

if (failures > 0)
{
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
