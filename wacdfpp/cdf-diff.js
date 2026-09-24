// Pure structural diff over two cdf-model models. No DOM, no WASM.
// Compares variables (shape/type/NRV + per-variable attributes) and global
// attributes (per entry). Values are compared via entryText (shared canonicalizer).
import { VAR_GROUPS, entryText } from "./cdf-model.js";

export const STATUS = { ADDED: "added", REMOVED: "removed", CHANGED: "changed", RENAMED: "renamed", SAME: "same" };

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

// A variable's "content" as a set of independent facts (shape/type/isNrv plus
// every attribute key=value), so two variables can be compared by how much
// content they share regardless of their name.
function signature(v) {
    const s = new Set();
    s.add(`shape=${v.shape.join(",")}`);
    s.add(`type=${v.typeName}`);
    s.add(`isNrv=${!!v.isNrv}`);
    for (const [k, val] of Object.entries(v.attributes ?? {})) s.add(`${k}=${entryText(val)}`);
    return s;
}

function jaccard(sa, sb) {
    let inter = 0;
    for (const x of sa) if (sb.has(x)) inter += 1;
    const union = sa.size + sb.size - inter;
    return union === 0 ? 0 : inter / union;
}

// simplify: rename detection is a greedy approximation of Git's own algorithm
// (score every removed x added pair by content overlap, match highest-scoring
// pairs first above a threshold, same as `git diff -M`) rather than an optimal
// bipartite assignment. Good enough for the handful of variables in a typical
// CDF; a pathological case with many equally-similar candidates could pick a
// less-than-ideal pairing. Upgrade path: the Hungarian algorithm, if that ever
// matters in practice.
const RENAME_SIMILARITY_THRESHOLD = 0.5;
// Variables with almost no attributes would otherwise match trivially (e.g.
// two variables that only share "VAR_TYPE=data" already hit 100% Jaccard).
const RENAME_MIN_SIGNATURE_SIZE = 2;

// Pair up same-group removed/added variables that are likely the same
// variable renamed (SciQLop/CDFpp#102: a whole screen of unrelated-looking
// +/- blocks for what were actually renames read as "messy at first glance").
// removed/added: [{ name, v }]. Returns the matched pairs plus whatever is
// left over as genuine adds/removes.
function detectRenames(removed, added) {
    const candidates = [];
    for (const r of removed) {
        const sr = signature(r.v);
        if (sr.size < RENAME_MIN_SIGNATURE_SIZE) continue;
        for (const a of added) {
            const sa = signature(a.v);
            if (sa.size < RENAME_MIN_SIGNATURE_SIZE) continue;
            const score = jaccard(sr, sa);
            if (score >= RENAME_SIMILARITY_THRESHOLD) candidates.push({ r, a, score });
        }
    }
    candidates.sort((x, y) => y.score - x.score);
    const usedR = new Set(), usedA = new Set(), pairs = [];
    for (const c of candidates) {
        if (usedR.has(c.r.name) || usedA.has(c.a.name)) continue;
        usedR.add(c.r.name);
        usedA.add(c.a.name);
        pairs.push({ oldName: c.r.name, newName: c.a.name, a: c.r.v, b: c.a.v });
    }
    return {
        pairs,
        remainingRemoved: removed.filter((r) => !usedR.has(r.name)),
        remainingAdded: added.filter((a) => !usedA.has(a.name)),
    };
}

function diffRenamedVariable(group, oldName, newName, a, b) {
    return {
        name: newName, oldName, group, status: STATUS.RENAMED,
        fields: diffFields(a, b), attributes: diffAttrs(a.attributes ?? {}, b.attributes ?? {}),
    };
}

const RANK = {
    [STATUS.CHANGED]: 0, [STATUS.RENAMED]: 1, [STATUS.ADDED]: 2, [STATUS.REMOVED]: 3, [STATUS.SAME]: 4,
};
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

// Splits every variable name into an exact-name diff (pushed straight into
// `groups`) or a raw removed/added candidate per group -- held back so a
// rename pass can pair up look-alikes before anything is finalized.
function partitionVariables(fa, fb, names, groups) {
    const rawRemoved = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    const rawAdded = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    for (const name of names) {
        const ea = fa.get(name), eb = fb.get(name);
        const group = eb ? eb.group : ea.group;
        if (ea && eb) groups[group].push(diffVariable(name, group, ea.v, eb.v));
        else if (ea) rawRemoved[group].push({ name, v: ea.v });
        else rawAdded[group].push({ name, v: eb.v });
    }
    return { rawRemoved, rawAdded };
}

// Runs the rename pass for one group's raw removed/added candidates, then
// finalizes whatever's left over as genuine adds/removes.
function resolveGroupRenames(groups, g, removed, added) {
    const { pairs, remainingRemoved, remainingAdded } = detectRenames(removed, added);
    for (const p of pairs) groups[g].push(diffRenamedVariable(g, p.oldName, p.newName, p.a, p.b));
    for (const r of remainingRemoved)
        groups[g].push({ name: r.name, group: g, status: STATUS.REMOVED, fields: [], attributes: [] });
    for (const a of remainingAdded)
        groups[g].push({ name: a.name, group: g, status: STATUS.ADDED, fields: [], attributes: [] });
}

export function diffModels(modelA, modelB) {
    const fa = flattenVars(modelA), fb = flattenVars(modelB);
    const names = new Set([...fa.keys(), ...fb.keys()]);
    const groups = Object.fromEntries(VAR_GROUPS.map(g => [g, []]));
    const { rawRemoved, rawAdded } = partitionVariables(fa, fb, names, groups);
    for (const g of VAR_GROUPS) resolveGroupRenames(groups, g, rawRemoved[g], rawAdded[g]);
    for (const g of VAR_GROUPS) sortDiffs(groups[g]);
    return { globalAttributes: diffGlobals(modelA, modelB), groups };
}

// Count added/removed/changed/renamed across global attributes and variables.
export function diffSummary(diff) {
    const c = { added: 0, removed: 0, changed: 0, renamed: 0 };
    const bump = (s) => { if (s in c) c[s] += 1; };
    for (const a of diff.globalAttributes) bump(a.status);
    for (const g of VAR_GROUPS) for (const v of diff.groups[g]) bump(v.status);
    return c;
}

// Flatten a diff into an ordered list of render-agnostic lines. The inline and
// side-by-side renderers both consume this. `includeSame` keeps unchanged items
// (the "Show all" view); section lines are emitted only when the section has at
// least one kept item.
//   { type: "section", section }                 // section: global|data|support_data|metadata
//   { type: "item",    status, label }            // a variable or global-attribute header
//   { type: "detail",  status, label, a, b }      // a field / attribute / entry change (a=old, b=new)
export function buildLines(diff, includeSame) {
    const lines = [];
    const keep = (s) => includeSame || s !== STATUS.SAME;

    const gItems = diff.globalAttributes.filter(a => keep(a.status));
    if (gItems.length) {
        lines.push({ type: "section", section: "global" });
        for (const a of gItems) {
            lines.push({ type: "item", status: a.status, label: a.name });
            for (const e of a.entries)
                lines.push({ type: "detail", status: e.status, label: `[${e.index}]`, a: e.a, b: e.b });
        }
    }

    for (const g of VAR_GROUPS) {
        const items = diff.groups[g].filter(v => keep(v.status));
        if (!items.length) continue;
        lines.push({ type: "section", section: g });
        for (const v of items) {
            const label = v.status === STATUS.RENAMED ? `${v.oldName} → ${v.name}` : v.name;
            lines.push({ type: "item", status: v.status, label });
            for (const f of v.fields)
                lines.push({ type: "detail", status: f.status, label: f.field, a: f.a, b: f.b });
            for (const at of v.attributes)
                lines.push({ type: "detail", status: at.status, label: at.name, a: at.a, b: at.b });
        }
    }
    return lines;
}

// GitHub-style line diff of one changed value (jsdiff diffArrays, via the
// global `Diff`). Returns one row per displayed line:
//   { a, b } with a === b  -> unchanged context line
//   { a, b } both non-null -> an edited line (removed run paired with the added run that follows)
//   { a, b: null } / { a: null, b } -> a pure removal / addition
const splitLines = (s) => (s === null || s === undefined ? [] : s.split("\n"));

export function lineRows(a, b) {
    const rows = [];
    let dels = [], adds = [];
    const flushChanges = () => {
        for (let i = 0; i < Math.max(dels.length, adds.length); i++)
            rows.push({ a: dels[i] ?? null, b: adds[i] ?? null });
        dels = []; adds = [];
    };
    for (const part of Diff.diffArrays(splitLines(a), splitLines(b))) {
        if (part.removed) dels.push(...part.value);
        else if (part.added) adds.push(...part.value);
        else { flushChanges(); rows.push(...part.value.map(line => ({ a: line, b: line }))); }
    }
    flushChanges();
    return rows;
}

// Word-level parts (jsdiff diffWords) for an edited line, or null when the two
// lines share less than half their text: lineRows pairs lines by position, so a
// pair can be two unrelated sentences, and marking every word of those is noise.
// simplify: fixed 0.5 threshold on shared characters; GitHub-like enough, tune if needed.
export function wordParts(a, b) {
    const parts = Diff.diffWords(a, b);
    const shared = parts.filter(p => !p.added && !p.removed).reduce((n, p) => n + p.value.length, 0);
    return shared * 2 >= Math.max(a.length, b.length) ? parts : null;
}
