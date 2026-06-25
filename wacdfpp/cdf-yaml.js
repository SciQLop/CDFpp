// YAML skeleton export for a loaded CDF. Mirrors the *structure* of
// pycdfpp.to_dict_skeleton: metadata only — types, shapes, attributes, never
// array data. Type/compression strings use the Explorer's clean names
// (e.g. CDF_FLOAT), not pycdfpp's DataType.* / CompressionType.* enum reprs.
//
// Pure and Node-tested: toYaml/buildSkeleton take the plain rawVars/rawGlobals
// records cdf-model.js already extracts at load time, so the export reads no
// array data of its own and needs no WASM (app.js feeds it the cached model).

// CDF type code -> canonical name (see include/cdfpp/cdf-enums.hpp CDF_Types).
export const CDF_TYPE_NAMES = {
    0: "CDF_NONE",
    1: "CDF_INT1", 2: "CDF_INT2", 4: "CDF_INT4", 8: "CDF_INT8",
    11: "CDF_UINT1", 12: "CDF_UINT2", 14: "CDF_UINT4",
    21: "CDF_REAL4", 22: "CDF_REAL8", 41: "CDF_BYTE",
    44: "CDF_FLOAT", 45: "CDF_DOUBLE",
    31: "CDF_EPOCH", 32: "CDF_EPOCH16", 33: "CDF_TIME_TT2000",
    51: "CDF_CHAR", 52: "CDF_UCHAR",
};

const typeName = (code) => CDF_TYPE_NAMES[code] ?? `CDF_TYPE_${code}`;

const isTypedArray = (v) => ArrayBuffer.isView(v) && !(v instanceof DataView);
const isList = (v) => Array.isArray(v) || isTypedArray(v);
const isScalar = (v) =>
    v === null || v === undefined ||
    typeof v === "string" || typeof v === "number" || typeof v === "boolean";

// Quote a plain string whenever it could be misread on re-parse as structure,
// a number, or a reserved word; otherwise emit it bare for readability. Biased
// toward quoting: a YAML round-trip must return the exact original string.
const QUOTE_RE = /[:#[\]{}&*!|>'"%@`,\n\t]/;
function needsQuote(s) {
    if (s === "") return true;
    if (s !== s.trim()) return true;                  // leading/trailing whitespace
    if (QUOTE_RE.test(s)) return true;                // structural / indicator chars
    if (/^[-?:](\s|$)/.test(s)) return true;          // leading "- " / "? " / ":" indicator
    if (s[0] === "~") return true;                    // leading null indicator
    if (/^[-+.]?[0-9]/.test(s)) return true;          // number-ish (incl. 1_000, 0x1a, .5)
    if (/^[-+]?\.(inf|nan)$/i.test(s)) return true;   // YAML float specials
    if (/^(true|false|null|yes|no|on|off)$/i.test(s)) return true;  // reserved words
    return false;
}

const keyYaml = (k) => {
    const s = String(k);
    return needsQuote(s) ? JSON.stringify(s) : s;
};

function scalarYaml(v) {
    if (v === null || v === undefined) return "null";
    if (typeof v === "boolean") return v ? "true" : "false";
    if (typeof v === "number") {
        if (Number.isNaN(v)) return ".nan";
        if (v === Infinity) return ".inf";
        if (v === -Infinity) return "-.inf";
        return String(v);
    }
    return needsQuote(v) ? JSON.stringify(v) : v;
}

const flow = (arr) => `[${arr.map(scalarYaml).join(", ")}]`;

function emitSeqItem(item, indent) {
    const pad = " ".repeat(indent);
    if (isScalar(item)) return `${pad}- ${scalarYaml(item)}\n`;
    if (isList(item)) {
        const arr = Array.from(item);
        if (arr.length === 0) return `${pad}- []\n`;
        if (arr.every(isScalar)) return `${pad}- ${flow(arr)}\n`;
        let out = `${pad}-\n`;
        for (const sub of arr) out += emitSeqItem(sub, indent + 2);
        return out;
    }
    const keys = Object.keys(item);
    if (keys.length === 0) return `${pad}- {}\n`;
    let out = "";
    keys.forEach((k, i) => {
        const entry = emitEntry(k, item[k], indent + 2);
        out += i === 0 ? `${pad}- ${entry.slice(indent + 2)}` : entry;
    });
    return out;
}

function emitEntry(key, value, indent) {
    const pad = " ".repeat(indent);
    const kk = keyYaml(key);
    if (isScalar(value)) return `${pad}${kk}: ${scalarYaml(value)}\n`;
    if (isList(value)) {
        const arr = Array.from(value);
        if (arr.length === 0) return `${pad}${kk}: []\n`;
        if (arr.every(isScalar)) return `${pad}${kk}: ${flow(arr)}\n`;
        let out = `${pad}${kk}:\n`;
        for (const item of arr) out += emitSeqItem(item, indent + 2);
        return out;
    }
    const keys = Object.keys(value);
    if (keys.length === 0) return `${pad}${kk}: {}\n`;
    let out = `${pad}${kk}:\n`;
    for (const k of keys) out += emitEntry(k, value[k], indent + 2);
    return out;
}

// Minimal YAML emitter for the restricted skeleton value space: nested plain
// objects, scalar/nested arrays (and typed arrays), strings, numbers
// (incl. NaN/Inf), booleans and null.
export function toYaml(value) {
    if (isScalar(value)) return `${scalarYaml(value)}\n`;
    if (isList(value)) {
        const arr = Array.from(value);
        if (arr.length === 0) return "[]\n";
        if (arr.every(isScalar)) return `${flow(arr)}\n`;
        return arr.map((item) => emitSeqItem(item, 0)).join("");
    }
    const keys = Object.keys(value);
    if (keys.length === 0) return "{}\n";
    return keys.map((k) => emitEntry(k, value[k], 0)).join("");
}

// Build the to_dict_skeleton-shaped object from plain raw records. A variable
// attribute holds a single entry, so its values/types are single-element lists,
// matching the global-attribute shape.
export function buildSkeleton(rawVars, rawGlobals, fileMeta = {}) {
    const attributes = {};
    for (const a of rawGlobals)
        attributes[a.name] = {
            values: a.entries,
            types: Array.from(a.types).map(typeName),
        };

    const variables = {};
    for (const v of rawVars) {
        const attrs = {};
        for (const an of Object.keys(v.attributes))
            attrs[an] = {
                values: [v.attributes[an]],
                types: [typeName(v.attributeTypes[an])],
            };
        variables[v.name] = {
            type: v.typeName,
            shape: Array.from(v.shape),
            is_nrv: v.isNrv,
            compression: v.compression,
            attributes: attrs,
        };
    }

    return { compression: fileMeta.compression, attributes, variables };
}
