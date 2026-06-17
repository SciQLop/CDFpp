// Node test: attribute values must survive WASM heap growth (WASM build).
//
// Regression test for the "detached ArrayBuffer" bug: numeric attribute values
// were handed to JS as zero-copy views into the WASM heap. Loading a variable's
// data (decompression) — or any later allocation — grows the heap and detaches
// every outstanding view, so reading such an attribute later threw
// "Cannot perform %TypedArray%.prototype.join on a detached ArrayBuffer".
// The fix makes attribute values owned copies. This test forces heap growth and
// checks a captured numeric attribute is still readable.
//
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
    const bytes = new Uint8Array(readFileSync(join(resourcesDir, file)));
    const cdf = Module.load(bytes);
    if (!cdf.is_valid())
        throw new Error(`failed to load ${file}`);
    return cdf;
}

// Grow the WASM heap by allocating a large buffer inside the module. Module.load
// copies its input into a WASM-side std::vector, so a big (invalid) input forces
// the heap to grow well past the initial size, detaching any stale views.
function forceHeapGrowth()
{
    const big = Module.load(new Uint8Array(96 * 1024 * 1024));
    big.delete();
}

// Find the first variable carrying a numeric (TypedArray) attribute.
function findNumericAttr(cdf)
{
    for (const vname of cdf.variable_names())
    {
        const v = cdf.get_variable(vname);
        for (const an of v.attribute_names)
        {
            const av = v.attributes[an];
            if (av !== undefined && typeof av !== "string" && av.length > 0)
                return { vname, an, snapshot: Array.from(av) };
        }
    }
    return undefined;
}

{
    const cdf = load("a_cdf.cdf");
    const target = findNumericAttr(cdf);
    check("a_cdf.cdf has a numeric variable attribute", target !== undefined);

    if (target)
    {
        const { vname, an, snapshot } = target;
        // Re-fetch the attribute, capture the live view, then grow the heap.
        const captured = cdf.get_variable(vname).attributes[an];
        forceHeapGrowth();

        // With the bug, `captured`'s buffer is now detached: length reads 0 and
        // join() throws. With the fix it is an owned copy and still matches.
        let readable = false;
        let joined = "<threw>";
        try
        {
            joined = captured.join(",");
            readable = captured.length === snapshot.length
                && snapshot.every((x, i) => captured[i] === x);
        }
        catch (e) { joined = `<threw: ${e.message}>`; }

        check(`${vname}.${an} survives heap growth (join=${joined})`, readable);
    }
    cdf.delete();
}

if (failures > 0)
{
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
