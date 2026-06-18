// Node test: time-typed attributes decode to ISO date strings (WASM build).
//
// Regression test for the demo showing raw EPOCH doubles / TT2000 counts for
// time-typed attributes (e.g. VALIDMIN/VALIDMAX/FILLVAL on epoch variables).
// The wrapper now decodes CDF_EPOCH / CDF_EPOCH16 / CDF_TIME_TT2000 attribute
// values to ISO-8601 date strings (via cdfpp's repr formatters) and exposes the
// CDF type code via `attribute_types` (variables) / `types` (global attrs).
// Using the repr path means: full date range (a 9999-12-31 FILLVAL sentinel
// renders correctly instead of overflowing int64 ns), exact EPOCH16 sub-second
// nanoseconds, and output identical to pycdfpp.
//
// Oracle strings match pycdfpp's repr of the same attributes.
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
const DT = Module.DataType;

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

function load(file)
{
    const cdf = Module.load(new Uint8Array(readFileSync(join(resourcesDir, file))));
    if (!cdf.is_valid())
        throw new Error(`failed to load ${file}`);
    return cdf;
}

// Each case: file, variable, attribute, expected CDF type, expected ISO string.
// The thg FILLVAL is the -1e31 fill sentinel — it must render as 9999-12-31
// (the case that previously overflowed int64 ns and wrapped to ~1677).
const cases = [
    ["testutf8.cdf", "ep", "dummy", DT.CDF_EPOCH.value, "2002-05-13T00:00:00.000000000"],
    ["testutf8.cdf", "ep16", "dummy", DT.CDF_EPOCH16.value, "2002-05-13T15:08:01.002003004"],
    ["testutf8.cdf", "tt2000", "dummy", DT.CDF_TIME_TT2000.value, "2010-05-13T10:20:30.040050060"],
    ["ac_h0_mfi_00000000_v01.cdf", "Epoch", "VALIDMIN", DT.CDF_EPOCH.value, "1996-01-01T00:00:00.000000000"],
    ["ac_h0_mfi_00000000_v01.cdf", "Epoch", "VALIDMAX", DT.CDF_EPOCH.value, "2020-01-01T00:00:00.000000000"],
    ["thg_l2_mag_mek_00000000_v01.cdf", "thg_mag_mek_epoch", "FILLVAL", DT.CDF_EPOCH.value, "9999-12-31T23:59:59.999"],
    ["thg_l2_mag_mek_00000000_v01.cdf", "thg_mag_mek_epoch", "VALIDMAX", DT.CDF_EPOCH.value, "2100-12-31T23:59:59.999000000"],
];

for (const [file, vname, aname, expType, expISO] of cases)
{
    const cdf = load(file);
    const v = cdf.get_variable(vname);

    check(`${file}:${vname}.${aname} type is time`, v.attribute_types[aname] === expType);

    const value = v.attributes[aname];
    check(`${file}:${vname}.${aname} decoded to ISO (${Array.isArray(value) ? value[0] : value})`,
        Array.isArray(value) && value.length === 1 && value[0] === expISO);

    cdf.delete();
}

if (failures > 0)
{
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
