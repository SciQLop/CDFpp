// Pure logic for the converter panel: which codecs to offer, and how each re-encoded
// output compares with the original file and with GZIP. No DOM, no WASM.

export const CODECS = [
    { key: "none", label: "Uncompressed", note: "CDF standard", experimental: false },
    { key: "gzip", label: "GZIP", note: "CDF standard, level 6", experimental: false },
    { key: "zstd", label: "Zstd", note: "experimental, level 1", experimental: true },
    { key: "blosc2", label: "Blosc2", note: "experimental, shuffle + Zstd level 5", experimental: true },
];

/** Codecs this WASM build exposes, in table order. */
export function availableCodecs(Module) {
    return CODECS.filter((c) => Module.CompressionType[c.key] !== undefined);
}

/**
 * runs: [{ key, size, writeMs, readMs, identical }] in completion order.
 * Returns the runs with vsOriginal, vsGzip (null until GZIP has run) and best
 * (smallest output among lossless round trips).
 */
export function summarizeRuns(runs, originalSize) {
    const gzip = runs.find((r) => r.key === "gzip");
    const lossless = runs.filter((r) => r.identical);
    const bestSize = lossless.length ? Math.min(...lossless.map((r) => r.size)) : null;
    return runs.map((r) => ({
        ...r,
        vsOriginal: r.size / originalSize,
        vsGzip: gzip ? r.size / gzip.size : null,
        best: r.identical && r.size === bestSize,
    }));
}

/** Download name: the loaded file's base name tagged with the codec. */
export function outputName(name, key) {
    const base = (name ?? "converted.cdf").replace(/\.cdf$/i, "");
    return `${base}.${key}.cdf`;
}

// The converter's worst moment is the uncompressed round trip: the decoded original, the
// uncompressed output and its decoded reload all live in WASM memory at once, plus the input
// while it is first loaded. 10% of each limit is kept for allocator overhead.
// simplify: ~1.1 GiB decoded fits wasm32 and ~4.6 GiB wasm64; skipping the reload check for
// large files would raise both ceilings.
const MEMORY_BUDGET = { wasm32: 0.9 * 2 ** 32, wasm64: 0.9 * 16 * 2 ** 30 };

/**
 * Which WASM build can convert a file of `inputBytes` whose values decode to `decodedBytes`:
 * "wasm32", "wasm64" (only if the browser supports 64-bit memory), or null if neither fits.
 */
export function pickBuild(inputBytes, decodedBytes, memory64Supported) {
    const needed = inputBytes + 3 * decodedBytes;
    if (needed <= MEMORY_BUDGET.wasm32) return "wasm32";
    if (memory64Supported && needed <= MEMORY_BUDGET.wasm64) return "wasm64";
    return null;
}
