// Node test: attribute values must survive WASM heap growth (WASM build).
//
// Regression test for the "detached ArrayBuffer" bug: numeric attribute values
// were handed to JS as zero-copy views into the WASM heap. Loading a variable's
// data (decompression) — or any later allocation — grows the heap and detaches
// every outstanding view, so reading such an attribute later threw
// "Cannot perform %TypedArray%.prototype.join on a detached ArrayBuffer".
// The fix makes attribute values owned copies. This test forces heap growth,
// asserts the growth actually happened (so it can't silently false-pass), and
// checks a captured numeric attribute is still readable.
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

function load(file)
{
    const bytes = new Uint8Array(readFileSync(join(resourcesDir, file)));
    const cdf = Module.load(bytes);
    if (!cdf.is_valid())
        throw new Error(`failed to load ${file}`);
    return cdf;
}

// Grow the WASM heap past its current size. Module.load copies its input into a
// WASM-side std::vector, so an allocation larger than the whole current heap
// forces it to grow, detaching `witness` (a live zero-copy view over the heap).
// Returns true only if the heap actually grew, so the caller can fail loudly
// instead of false-passing when no detachment occurred.
function forceHeapGrowth(witness)
{
    const heapBytes = witness.buffer.byteLength;
    const big = Module.load(new Uint8Array(heapBytes + 64 * 1024 * 1024));
    big.delete();
    return witness.buffer.byteLength === 0; // detached buffers report length 0
}

// Find the first variable that carries a numeric (TypedArray) attribute AND has
// a loadable values view (used as the heap-growth witness).
function findTarget(cdf)
{
    for (const vname of cdf.variable_names())
    {
        const v = cdf.get_variable(vname);
        if (v.values === undefined)
            continue;
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
    const target = findTarget(cdf);
    check("a_cdf.cdf has a numeric attribute on a variable with values", target !== undefined);

    if (target)
    {
        const { vname, an, snapshot } = target;
        // Re-fetch: capture the attribute (owned copy, under test) and a live
        // values view (the heap-growth witness), then grow the heap.
        const v = cdf.get_variable(vname);
        const captured = v.attributes[an];
        const grew = forceHeapGrowth(v.values);

        // Guard against a vacuous pass: if the heap didn't actually grow, the
        // test proves nothing about detachment, so fail.
        check(`${vname} values view detached (heap actually grew)`, grew);

        // With the bug, `captured` would be detached too (join throws). With the
        // fix it is an owned copy and still matches the snapshot.
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
