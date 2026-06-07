// Node test for CdfFile.time_values_as_ns_since_1970 (WASM build).
//
// Oracle values come from pycdfpp's to_datetime64() on the same resource files,
// which uses the exact same cdf::to_ns_from_1970() leap-second-correct path.
// Run after building the WASM module:
//   node test.mjs <path-to-cdfpp.js> <path-to-tests/resources>

import { readFileSync } from "node:fs";
import { join } from "node:path";
import process from "node:process";

const [, , modulePath, resourcesDir] = process.argv;
if (!modulePath || !resourcesDir)
{
    console.error("usage: node test.mjs <cdfpp.js> <resources_dir>");
    process.exit(2);
}

const { default: createCdfModule } = await import(modulePath);
const Module = await createCdfModule();

let failures = 0;
function check(name, actual, expected)
{
    const ok = actual === expected;
    if (!ok)
    {
        failures += 1;
        console.error(`FAIL ${name}: got ${actual}, expected ${expected}`);
    }
    else
    {
        console.log(`ok   ${name}`);
    }
}

function load(file)
{
    const bytes = new Uint8Array(readFileSync(join(resourcesDir, file)));
    const cdf = Module.load(bytes);
    if (!cdf.is_valid())
        throw new Error(`failed to load ${file}`);
    return cdf;
}

// a_cdf.cdf: all three time types, 101 points each (exercises the threaded/bulk path).
//
// Note on tt2000[0]: its raw J2000 value decodes to 1969-12-31T23:59:59, i.e. before
// the NASA leap-second table's first entry (1972-01-01). In that undefined extrapolation
// zone CDFpp's scalar conversion (this WASM build, xsimd gated off non-x86) and its SIMD
// conversion (pycdfpp on x86) disagree by 9 s. We therefore don't pin element 0 here;
// leap-second correctness is pinned by testutf8.cdf below (spans the 2015 leap second).
{
    const cdf = load("a_cdf.cdf");
    const cases = {
        epoch: { n: 101, first: 0n, last: 1555200000000000000n },
        epoch16: { n: 101, first: 0n, last: 1555200000000000000n },
        tt2000: { n: 101, last: 1555200000000000000n },
    };
    for (const [varname, exp] of Object.entries(cases))
    {
        const ns = cdf.time_values_as_ns_since_1970(varname);
        check(`a_cdf.cdf/${varname} length`, ns.length, exp.n);
        if (exp.first !== undefined)
            check(`a_cdf.cdf/${varname} first`, ns[0], exp.first);
        check(`a_cdf.cdf/${varname} last`, ns[ns.length - 1], exp.last);
    }
    cdf.delete();
}

// testutf8.cdf: small arrays (scalar path, n<8); tt2000 spans the 2015 leap second.
{
    const cdf = load("testutf8.cdf");
    const expected = {
        ep: [920610367100000000n, 883710245666000000n],
        ep16: [1101743723030411520n, 1104339384031411584n, 1135875384031444608n],
        tt2000: [
            1435708798123456789n, 1435708799123456789n, 1435708800123456789n,
            1435708800123456789n, 1435708801123456789n, 1435708802123456789n,
        ],
    };
    for (const [varname, exp] of Object.entries(expected))
    {
        const ns = cdf.time_values_as_ns_since_1970(varname);
        check(`testutf8.cdf/${varname} length`, ns.length, exp.length);
        for (let i = 0; i < exp.length; i++)
            check(`testutf8.cdf/${varname}[${i}]`, ns[i], exp[i]);
    }

    // Non-time variable and missing variable both return undefined.
    check("non-time var -> undefined",
        cdf.time_values_as_ns_since_1970("Latitude") === undefined, true);
    check("missing var -> undefined",
        cdf.time_values_as_ns_since_1970("does_not_exist") === undefined, true);
    cdf.delete();
}

if (failures > 0)
{
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
