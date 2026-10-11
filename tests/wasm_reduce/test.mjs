// Node test: variable_info and reduce_values (WASM build).
//
// variable_info gives a variable's metadata without reading its values, so the Explorer can
// list and gate large variables. reduce_values collapses or slices a distribution's
// dimensions in WASM and hands JS only the (records, kept dims) result.
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
const eq = (a, b) => JSON.stringify(Array.from(a)) === JSON.stringify(b);
function throws(f)
{
    try { f(); return false; } catch (e) { return e instanceof Error; }
}

const cdf = Module.load(new Uint8Array(readFileSync(join(resourcesDir, "dist_4d.cdf"))));
check("fixture loads", cdf.is_valid());

// dist[r, phi, theta, energy] = 1000r + 100p + 10t + e, dist[0,0,0,0] = FILLVAL
const KEEP = -1, COLLAPSE = -2, FILL = -1e31;

const info = cdf.variable_info("dist");
check("info shape", eq(info.shape, [3, 2, 3, 4]));
check("info attributes", info.attributes.DEPEND_3 === "energy");
check("info has no values", info.copy_values === undefined && info.values === undefined);
check("info does not load values", info.values_loaded === false);
check("info of a missing variable", cdf.variable_info("nope") === undefined);

const omni = cdf.reduce_values("dist", [COLLAPSE, COLLAPSE, KEEP], "sum", FILL, NaN, NaN);
check("sum over angles", eq(omni, [360, 366, 372, 378, 6360, 6366, 6372, 6378, 12360, 12366, 12372, 12378]));

const avg = cdf.reduce_values("dist", [COLLAPSE, COLLAPSE, KEEP], "mean", FILL, NaN, NaN);
check("mean skips FILLVAL", avg[0] === 72 && avg[1] === 61);

const slice = cdf.reduce_values("dist", [1, 2, KEEP], "sum", NaN, NaN, NaN);
check("slice", eq(slice.subarray(0, 4), [120, 121, 122, 123]));

const unmasked = cdf.reduce_values("dist", [0, 0, 0], "sum", NaN, NaN, NaN);
check("no mask keeps FILLVAL", unmasked[0] === Math.fround(FILL));

const validRange = cdf.reduce_values("dist", [1, 2, KEEP], "sum", NaN, 121, 122);
check("VALIDMIN/VALIDMAX", Number.isNaN(validRange[0]) && validRange[1] === 121 && Number.isNaN(validRange[3]));

check("wrong mode count throws", throws(() => cdf.reduce_values("dist", [KEEP], "sum", NaN, NaN, NaN)));
check("slice out of range throws", throws(() => cdf.reduce_values("dist", [5, KEEP, KEEP], "sum", NaN, NaN, NaN)));
check("unknown op throws", throws(() => cdf.reduce_values("dist", [KEEP, KEEP, KEEP], "max", NaN, NaN, NaN)));
check("time variable throws", throws(() => cdf.reduce_values("Epoch", [], "sum", NaN, NaN, NaN)));
check("missing variable is undefined", cdf.reduce_values("nope", [], "sum", NaN, NaN, NaN) === undefined);

cdf.delete();
if (failures)
{
    console.error(`${failures} failure(s)`);
    process.exit(1);
}
console.log("all passed");
