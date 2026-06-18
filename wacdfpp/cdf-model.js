// Pure data view over a loaded CdfFile. No DOM. The pure functions
// (normalizeVarType/buildModel/filterModel) take plain records so they are
// testable in Node without the WASM module; rawFromCdfFile is the browser-only
// adapter that reads the Emscripten CdfFile getters.

export const VAR_GROUPS = ["data", "support_data", "metadata"];

// Normalize an ISTP VAR_TYPE attribute value to one of VAR_GROUPS; default "data".
export function normalizeVarType(varType) {
    const v = String(varType ?? "").trim().toLowerCase();
    return VAR_GROUPS.includes(v) ? v : "data";
}

// Variables are shared by reference into the groups; callers must not mutate them.
export function buildModel(rawVars, rawGlobals) {
    const groups = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    for (const v of rawVars) groups[normalizeVarType(v.varType)].push(v);
    return { globalAttributes: rawGlobals, groups };
}

// Lowercased haystack for a variable: its name + every attribute name and value.
function variableHaystack(v) {
    const parts = [v.name];
    for (const [k, val] of Object.entries(v.attributes ?? {})) {
        parts.push(k);
        parts.push(String(val));
    }
    return parts.join("\n").toLowerCase();
}

// Free-text filter. Empty/whitespace query returns the model unchanged (same ref).
export function filterModel(model, query) {
    const q = query.trim().toLowerCase();
    if (!q) return model;
    const groups = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    for (const g of VAR_GROUPS)
        groups[g] = model.groups[g].filter(v => variableHaystack(v).includes(q));
    const globalAttributes = model.globalAttributes.filter(
        a => `${a.name}\n${a.value}`.toLowerCase().includes(q));
    return { globalAttributes, groups };
}

// Browser-only: read a loaded CdfFile into raw records for buildModel.
// NOTE: get_variable() currently force-loads values (see wacdfpp.cpp); we keep
// only metadata here (attributes are owned copies, safe) and re-fetch values
// lazily on selection. A metadata-only binding is a future (Phase 2+) optimization.
export function rawFromCdfFile(cdf) {
    const rawVars = cdf.variable_names().map(name => {
        const v = cdf.get_variable(name);
        const attributes = {};
        for (const an of v.attribute_names) attributes[an] = v.attributes[an];
        return {
            name,
            shape: Array.from(v.shape),
            typeName: v.type_name,
            type: v.type,
            isNrv: v.is_nrv,
            varType: attributes.VAR_TYPE,
            attributes,
        };
    });
    const rawGlobals = cdf.attribute_names().map(name => {
        const a = cdf.get_attribute(name);
        return { name, value: a.entries.join(", ") };
    });
    return { rawVars, rawGlobals };
}
