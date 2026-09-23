// Pure Node test for wacdfpp/convert-model.js — no WASM required.
//   node test.mjs
import { CODECS, availableCodecs, summarizeRuns, outputName } from "../../wacdfpp/convert-model.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

// --- availableCodecs: only what this WASM build exposes -----------------
const fullBuild = { CompressionType: { none: {}, gzip: {}, zstd: {}, blosc2: {} } };
const standardBuild = { CompressionType: { none: {}, gzip: {} } };
check("full build offers all codecs, in table order",
    availableCodecs(fullBuild).map((c) => c.key).join() === CODECS.map((c) => c.key).join());
check("standard build hides experimental codecs",
    availableCodecs(standardBuild).map((c) => c.key).join() === "none,gzip");
check("experimental codecs are flagged",
    CODECS.filter((c) => c.experimental).map((c) => c.key).join() === "zstd,blosc2");

// --- summarizeRuns: ratios against the original and against GZIP --------
const runs = [
    { key: "none", size: 1000, writeMs: 1, readMs: 1, identical: true },
    { key: "gzip", size: 400, writeMs: 10, readMs: 4, identical: true },
    { key: "blosc2", size: 300, writeMs: 5, readMs: 1, identical: true },
];
const rows = summarizeRuns(runs, 500);
const blosc2 = rows.find((r) => r.key === "blosc2");
check("vs original is size / original size", blosc2.vsOriginal === 0.6);
check("vs gzip is size / gzip size", blosc2.vsGzip === 0.75);
check("gzip is 1.0 against itself", rows.find((r) => r.key === "gzip").vsGzip === 1);
check("smallest output is marked best", blosc2.best && !rows.find((r) => r.key === "gzip").best);
check("vs gzip is null until gzip has run",
    summarizeRuns([{ key: "blosc2", size: 3, writeMs: 1, readMs: 1, identical: true }], 10)[0].vsGzip === null);
check("a failed round trip is never best",
    !summarizeRuns([{ ...runs[2], identical: false }, runs[1]], 500).find((r) => r.key === "blosc2").best);

// --- outputName: keeps the base name, tags the codec -------------------
check("tags codec before extension", outputName("ac_h0_mfi_20200101_v07.cdf", "blosc2") === "ac_h0_mfi_20200101_v07.blosc2.cdf");
check("case-insensitive extension", outputName("X.CDF", "gzip") === "X.gzip.cdf");
check("no name falls back", outputName(null, "zstd") === "converted.zstd.cdf");

if (failures > 0) {
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
