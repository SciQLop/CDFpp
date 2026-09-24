// Pure Node test for wacdfpp/convert-model.js — no WASM required.
//   node test.mjs
import { CODECS, availableCodecs, summarizeRuns, outputName, pickBuild } from "../../wacdfpp/convert-model.js";

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
const near = (a, b) => Math.abs(a - b) < 1e-12;
const rows = summarizeRuns(runs, 500);
const blosc2 = rows.find((r) => r.key === "blosc2");
check("vs original is size / original size", near(blosc2.vsOriginal, 0.6));
check("vs gzip is size / gzip size", near(blosc2.vsGzip, 0.75));
check("gzip is 1.0 against itself", near(rows.find((r) => r.key === "gzip").vsGzip, 1));
check("smallest output is marked best", blosc2.best && !rows.find((r) => r.key === "gzip").best);
check("vs gzip is null until gzip has run",
    summarizeRuns([{ key: "blosc2", size: 3, writeMs: 1, readMs: 1, identical: true }], 10)[0].vsGzip === null);
check("a failed round trip is never best",
    !summarizeRuns([{ ...runs[2], identical: false }, runs[1]], 500).find((r) => r.key === "blosc2").best);

// --- outputName: keeps the base name, tags the codec -------------------
check("tags codec before extension", outputName("ac_h0_mfi_20200101_v07.cdf", "blosc2") === "ac_h0_mfi_20200101_v07.blosc2.cdf");
check("case-insensitive extension", outputName("X.CDF", "gzip") === "X.gzip.cdf");
check("no name falls back", outputName(null, "zstd") === "converted.zstd.cdf");

// --- pickBuild: 32-bit WASM when it fits, else 64-bit when the browser has it -------
const GiB = 2 ** 30;
const HPCA = [294807679, 2.34 * GiB];   // 295 MB MMS HPCA file, 2.34 GiB decoded
check("a small file uses wasm32", pickBuild(1e6, 3e6, true) === "wasm32");
check("about 1 GiB decoded still uses wasm32", pickBuild(0.2 * GiB, 1.0 * GiB, true) === "wasm32");
check("the HPCA file uses wasm64 when available", pickBuild(...HPCA, true) === "wasm64");
check("the HPCA file cannot be converted without wasm64", pickBuild(...HPCA, false) === null);
check("beyond 16 GiB nothing fits", pickBuild(1 * GiB, 5 * GiB, true) === null);

if (failures > 0) {
    console.error(`\n${failures} check(s) failed`);
    process.exit(1);
}
console.log("\nall checks passed");
