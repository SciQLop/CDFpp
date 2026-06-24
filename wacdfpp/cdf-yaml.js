// YAML skeleton export for a loaded CDF. Mirrors the structure of
// pycdfpp.to_dict_skeleton: metadata only (types, shapes, attributes) — never
// array data, so it is safe for high-rank / huge variables.
//
// Split like cdf-model.js: toYaml/buildSkeleton are pure and Node-tested;
// skeletonFromCdfFile/cdfFileToYaml are browser-only adapters over the live
// Emscripten CdfFile getters.

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

// Quote a plain string only when it could be misread as structure, a number,
// or a reserved word; otherwise emit it bare for readability.
const QUOTE_RE = /[:#[\]{}&*!|>'"%@`,\n\t]/;
function needsQuote(s) {
    if (s === "") return true;
    if (s !== s.trim()) return true;
    if (QUOTE_RE.test(s)) return true;
    if (/^[-+]?(\d|\.\d)/.test(s) && !Number.isNaN(Number(s))) return true;
    if (/^(true|false|null|yes|no|on|off|~)$/i.test(s)) return true;
    return false;
}

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
    if (isScalar(value)) return `${pad}${key}: ${scalarYaml(value)}\n`;
    if (isList(value)) {
        const arr = Array.from(value);
        if (arr.length === 0) return `${pad}${key}: []\n`;
        if (arr.every(isScalar)) return `${pad}${key}: ${flow(arr)}\n`;
        let out = `${pad}${key}:\n`;
        for (const item of arr) out += emitSeqItem(item, indent + 2);
        return out;
    }
    const keys = Object.keys(value);
    if (keys.length === 0) return `${pad}${key}: {}\n`;
    let out = `${pad}${key}:\n`;
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

// Browser-only: read a loaded CdfFile into the skeleton object.
export function skeletonFromCdfFile(cdf) {
    const rawVars = cdf.variable_names().map((name) => {
        const v = cdf.get_variable(name);
        const attributes = {};
        const attributeTypes = {};
        for (const an of v.attribute_names) {
            attributes[an] = v.attributes[an];
            attributeTypes[an] = v.attribute_types[an];
        }
        return {
            name,
            typeName: v.type_name,
            shape: Array.from(v.shape),
            isNrv: v.is_nrv,
            compression: v.compression,
            attributes,
            attributeTypes,
        };
    });
    const rawGlobals = cdf.attribute_names().map((name) => {
        const a = cdf.get_attribute(name);
        return { name, entries: Array.from(a.entries), types: Array.from(a.types) };
    });
    return buildSkeleton(rawVars, rawGlobals, { compression: cdf.compression() });
}

// Browser-only: full YAML skeleton string for a loaded CdfFile.
export function cdfFileToYaml(cdf) {
    return toYaml(skeletonFromCdfFile(cdf));
}
