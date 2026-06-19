// Pure structural diff over two cdf-model models. No DOM, no WASM.
// Compares variables (shape/type/NRV + per-variable attributes) and global
// attributes (per entry). Values are compared via entryText (shared canonicalizer).
import { VAR_GROUPS, entryText } from "./cdf-model.js";

export const STATUS = { ADDED: "added", REMOVED: "removed", CHANGED: "changed", SAME: "same" };

// name -> { group, v } across all variable groups of a model.
function flattenVars(model) {
    const out = new Map();
    for (const g of VAR_GROUPS)
        for (const v of model.groups[g]) out.set(v.name, { group: g, v });
    return out;
}

// Only differing fields are returned; each is implicitly a "changed" field.
function diffFields(a, b) {
    const fields = [];
    const sa = a.shape.join(", "), sb = b.shape.join(", ");
    if (sa !== sb) fields.push({ field: "shape", status: STATUS.CHANGED, a: sa, b: sb });
    if (a.typeName !== b.typeName)
        fields.push({ field: "type", status: STATUS.CHANGED, a: a.typeName, b: b.typeName });
    if (!!a.isNrv !== !!b.isNrv)
        fields.push({ field: "isNrv", status: STATUS.CHANGED, a: String(!!a.isNrv), b: String(!!b.isNrv) });
    return fields;
}

// Keyed attribute diff; only added/removed/changed are returned (same omitted).
function diffAttrs(a, b) {
    const out = [];
    const names = [...new Set([...Object.keys(a), ...Object.keys(b)])].sort();
    for (const name of names) {
        const inA = name in a, inB = name in b;
        if (inA && !inB) out.push({ name, status: STATUS.REMOVED, a: entryText(a[name]), b: null });
        else if (!inA && inB) out.push({ name, status: STATUS.ADDED, a: null, b: entryText(b[name]) });
        else {
            const av = entryText(a[name]), bv = entryText(b[name]);
            if (av !== bv) out.push({ name, status: STATUS.CHANGED, a: av, b: bv });
        }
    }
    return out;
}

function diffVariable(name, group, a, b) {
    if (a && !b) return { name, group, status: STATUS.REMOVED, fields: [], attributes: [] };
    if (!a && b) return { name, group, status: STATUS.ADDED, fields: [], attributes: [] };
    const fields = diffFields(a, b);
    const attributes = diffAttrs(a.attributes ?? {}, b.attributes ?? {});
    const status = (fields.length || attributes.length) ? STATUS.CHANGED : STATUS.SAME;
    return { name, group, status, fields, attributes };
}

const RANK = { [STATUS.CHANGED]: 0, [STATUS.ADDED]: 1, [STATUS.REMOVED]: 2, [STATUS.SAME]: 3 };
function sortDiffs(list) {
    return list.sort((x, y) => RANK[x.status] - RANK[y.status] || x.name.localeCompare(y.name));
}

function diffGlobalAttr(name, ea, eb) {
    if (ea && !eb)
        return { name, status: STATUS.REMOVED,
            entries: ea.map((e, i) => ({ index: i, status: STATUS.REMOVED, a: entryText(e), b: null })) };
    if (!ea && eb)
        return { name, status: STATUS.ADDED,
            entries: eb.map((e, i) => ({ index: i, status: STATUS.ADDED, a: null, b: entryText(e) })) };
    const n = Math.max(ea.length, eb.length);
    const entries = [];
    for (let i = 0; i < n; i++) {
        const hasA = i < ea.length, hasB = i < eb.length;
        if (hasA && !hasB) entries.push({ index: i, status: STATUS.REMOVED, a: entryText(ea[i]), b: null });
        else if (!hasA && hasB) entries.push({ index: i, status: STATUS.ADDED, a: null, b: entryText(eb[i]) });
        else {
            const av = entryText(ea[i]), bv = entryText(eb[i]);
            if (av !== bv) entries.push({ index: i, status: STATUS.CHANGED, a: av, b: bv });
        }
    }
    return { name, status: entries.length ? STATUS.CHANGED : STATUS.SAME, entries };
}

function diffGlobals(modelA, modelB) {
    const ma = new Map(modelA.globalAttributes.map(a => [a.name, a.entries]));
    const mb = new Map(modelB.globalAttributes.map(a => [a.name, a.entries]));
    const names = [...new Set([...ma.keys(), ...mb.keys()])].sort();
    return names.map(name => diffGlobalAttr(name, ma.get(name), mb.get(name)));
}

export function diffModels(modelA, modelB) {
    const fa = flattenVars(modelA), fb = flattenVars(modelB);
    const names = new Set([...fa.keys(), ...fb.keys()]);
    const groups = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    for (const name of names) {
        const ea = fa.get(name), eb = fb.get(name);
        const group = eb ? eb.group : ea.group;
        groups[group].push(diffVariable(name, group, ea?.v, eb?.v));
    }
    for (const g of VAR_GROUPS) sortDiffs(groups[g]);
    return { globalAttributes: diffGlobals(modelA, modelB), groups };
}

// Count added/removed/changed across global attributes and variables.
export function diffSummary(diff) {
    const c = { added: 0, removed: 0, changed: 0 };
    const bump = (s) => { if (s in c) c[s] += 1; };
    for (const a of diff.globalAttributes) bump(a.status);
    for (const g of VAR_GROUPS) for (const v of diff.groups[g]) bump(v.status);
    return c;
}
