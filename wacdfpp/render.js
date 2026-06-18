// Rendering for the wacdfpp master/detail UI. The formatters at the top are pure
// (no DOM) and are unit-tested in Node; the renderList/renderDetail functions
// (added in a later task) build DOM and are verified manually via the dev loop.

export const CDF_EPOCH = 31, CDF_EPOCH16 = 32, CDF_TIME_TT2000 = 33;
export const CDF_CHAR = 51, CDF_UCHAR = 52;

export const isTimeType = (t) => t === CDF_EPOCH || t === CDF_EPOCH16 || t === CDF_TIME_TT2000;
export const isCharType = (t) => t === CDF_CHAR || t === CDF_UCHAR;

// NOTE: does not remap CDF fill sentinels — TT2000 INT64_MIN renders as a 1677
// date and EPOCH -1e31 as a large number / NaN fallback. This mirrors the demo's
// existing value-preview behavior; canonical sentinel display (9999-12-31) lives
// in the C++ repr/ISO path, not here.
// ns-since-1970 (BigInt, leap-second corrected) -> ISO 8601 with ns precision.
export function nsToISO(ns) {
    const NS_PER_MS = 1000000n;
    let ms = ns / NS_PER_MS;
    let rem = ns - ms * NS_PER_MS;
    if (rem < 0n) { rem += NS_PER_MS; ms -= 1n; }
    const d = new Date(Number(ms));
    if (Number.isNaN(d.getTime())) return ns.toString();
    return d.toISOString().replace("Z", "") + rem.toString().padStart(6, "0") + "Z";
}

// Drop trailing null padding then trailing whitespace, without regex.
export function stripPadding(s) {
    let end = s.length;
    while (end > 0 && s.codePointAt(end - 1) === 0) end--;
    return s.slice(0, end).trimEnd();
}

// Split a flat values array into one row per record. recLen<=0 -> one row.
export function chunkRecords(arr, recLen) {
    if (!recLen || recLen <= 0) return [Array.from(arr)];
    const out = [];
    for (let i = 0; i < arr.length; i += recLen)
        out.push(Array.from(arr.slice(i, i + recLen)));
    return out;
}

// Decode a CDF_CHAR/UCHAR byte buffer into one string per record (last shape dim
// is the fixed string length). Only the first `max` records are decoded; `total`
// reports the full record count.
export function decodeChars(bytes, shape, max) {
    const dec = new TextDecoder("utf-8", { fatal: false });
    const strLen = shape.length ? shape[shape.length - 1] : 0;
    if (strLen > 0 && bytes.length >= strLen && bytes.length % strLen === 0) {
        const total = bytes.length / strLen;
        const n = Math.min(max ?? total, total);
        const strings = [];
        for (let i = 0; i < n; i++)
            strings.push(stripPadding(dec.decode(bytes.subarray(i * strLen, (i + 1) * strLen))));
        return { strings, total };
    }
    return { strings: [stripPadding(dec.decode(bytes))], total: 1 };
}

export function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// Render one attribute value: string -> quoted/trimmed, array/typed-array ->
// comma list, anything else (scalar) -> as-is.
export function formatAttrValue(value) {
    if (typeof value === "string") return `"${value.trim()}"`;
    if (Array.isArray(value) || ArrayBuffer.isView(value))
        return Array.from(value).map(String).join(", ");
    return value;
}
