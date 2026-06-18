// Node test: save() must round-trip on wasm32 (WASM build).
//
// Regression test for a 32-bit save bug. The VVR fast-path wrote the record's
// size header as a platform-width std::size_t instead of the fixed 8-byte
// cdf_offset_field_t. On wasm32 (size_t == 4 bytes) every VVR header was 4
// bytes short, shifting all following record offsets and tripping the layout
// assertion `offset - vvr.size == vvr.offset` in saving.hpp, which aborts the
// module ("unreachable"). Native (size_t == 8 bytes) hid the bug. The fix casts
// the value to the header's record_size type. This test saves a real file,
// re-loads the saved bytes, and checks variable values survive the round-trip.
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

// Resources live alongside this test in the source tree; resolve them relative
// to the test file rather than from a caller-supplied path.
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

function load(bytesOrFile)
{
    const bytes = typeof bytesOrFile === "string"
        ? new Uint8Array(readFileSync(join(resourcesDir, bytesOrFile)))
        : bytesOrFile;
    const cdf = Module.load(bytes);
    if (!cdf.is_valid())
        throw new Error("failed to load CDF");
    return cdf;
}

function snapshotValues(cdf)
{
    const snap = {};
    for (const vname of cdf.variable_names())
    {
        const values = cdf.get_variable(vname).values;
        snap[vname] = values === undefined ? null : Array.from(values);
    }
    return snap;
}

function sameValues(a, b)
{
    const names = Object.keys(a);
    if (names.length !== Object.keys(b).length)
        return false;
    return names.every((n) =>
    {
        const x = a[n], y = b[n];
        if (x === null || y === null)
            return x === y;
        return x.length === y.length && x.every((v, i) => v === y[i]);
    });
}

// Any file with a non-empty variable exercises the VVR fast-path; this one has
// several uncompressed variables, so it hits the bug on the very first VVR.
const RESOURCE = "ac_h0_mfi_00000000_v01.cdf";

{
    const cdf = load(RESOURCE);
    const before = snapshotValues(cdf);

    let saved;
    let threw = false;
    try { saved = cdf.save(); }
    catch (e) { threw = true; console.error(`save threw: ${e.message}`); }
    cdf.delete();

    // Pre-fix this aborts the module ("unreachable") instead of returning bytes.
    check(`save(${RESOURCE}) does not abort`, !threw && saved !== undefined);

    if (!threw && saved !== undefined)
    {
        const reloaded = load(new Uint8Array(saved));
        const after = snapshotValues(reloaded);
        reloaded.delete();
        check("re-loaded CDF has identical variable values", sameValues(before, after));
    }
}

if (failures > 0)
{
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
