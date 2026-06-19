# CDF Compare (structural diff) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Compare" mode to the wacdfpp CDF Explorer that diffs two CDF files on structure + metadata (variables added/removed/changed; global attributes diffed per entry) and renders a unified, color-coded diff.

**Architecture:** Reuse the existing pure-model / render / orchestration split. A shared `entryText` canonicalizer in `cdf-model.js` feeds three consumers (filter, single-file render, diff). A new pure `cdf-diff.js` computes a plain diff structure from two models (Node-testable). A new `compare.js` orchestrates loading two files (deleting each `CdfFile` right after building its model) and renders the diff DOM. `wasm.js` memoizes the WASM module so both modes share one instance.

**Tech Stack:** Vanilla ES modules, Emscripten/embind WASM (`cdfpp.js`), Node for pure unit tests, Meson/Ninja build.

---

## File Structure

| File | Responsibility |
|------|----------------|
| `wacdfpp/cdf-model.js` *(modify)* | Add `entryText`; global-attr records keep `entries`+`types`; `filterModel` uses `entryText` |
| `wacdfpp/cdf-diff.js` *(new)* | Pure `diffModels(a,b)` + `diffSummary(diff)` |
| `wacdfpp/wasm.js` *(new)* | Memoized `loadModule()` |
| `wacdfpp/compare.js` *(new)* | Compare orchestration + diff DOM rendering |
| `wacdfpp/render.js` *(modify)* | Global-attr rows display via `entryText` |
| `wacdfpp/app.js` *(modify)* | Use `loadModule()`; Compare toggle; `?compare` / `?a=&b=` URL params |
| `wacdfpp/wacdfpp.html` *(modify)* | Compare toggle, A/B input row, result container, diff CSS |
| `wacdfpp/meson.build` *(modify)* | Add new JS files to `extra_files` + `fs.copyfile` |
| `tests/wacdfpp_model/test.mjs` *(modify)* | Update global-attr records to new shape; add `entryText` checks |
| `tests/wacdfpp_diff/test.mjs` *(new)* | Unit tests for `diffModels` / `diffSummary` |
| `tests/meson.build` *(modify)* | Register `wacdfpp_diff` Node test |

**Conventions to follow:**
- Pure JS test harness (see `tests/wacdfpp_model/test.mjs`): top-level `check(name, ok)` + `eq = (a,b)=>JSON.stringify(a)===JSON.stringify(b)`, `process.exit(failures?1:0)`. No test framework.
- 4-space indent, ES module imports with `./` relative paths and `.js` extension.
- DOM rendering is verified manually via the dev loop (no jsdom in this project); pure logic is unit-tested in Node.

---

## Task 1: Shared `entryText` canonicalizer + global-attr model shape

**Files:**
- Modify: `wacdfpp/cdf-model.js`
- Modify: `tests/wacdfpp_model/test.mjs`

Global attributes currently collapse to a joined `value` string in `rawFromCdfFile`, and `filterModel` reads `a.value`. We switch the model to keep `entries` + `types`, and introduce one canonicalizer (`entryText`) reused by filter, render, and diff.

- [ ] **Step 1: Write failing tests for `entryText` and the new global-attr filter shape**

In `tests/wacdfpp_model/test.mjs`, change the import line to add `entryText`:

```js
import {
    VAR_GROUPS, normalizeVarType, buildModel, filterModel, entryText,
} from "../../wacdfpp/cdf-model.js";
```

Replace the `rawGlobals` definition (currently the two `{name, value}` records) with the new shape:

```js
const rawGlobals = [
    { name: "Project", entries: ["THEMIS"], types: [51] },
    { name: "Discipline", entries: ["Space Physics"], types: [51] },
];
```

Add these checks just before the final `process.exit(...)` line:

```js
check("entryText trims strings", entryText("  THEMIS  ") === "THEMIS");
check("entryText joins numeric array", entryText(new Float64Array([1, 2, 3])) === "1, 2, 3");
check("entryText joins string array", entryText(["a", "b"]) === "a, b");
check("entryText scalar", entryText(42) === "42");
check("entryText nullish -> empty", entryText(undefined) === "" && entryText(null) === "");
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `node tests/wacdfpp_model/test.mjs`
Expected: FAIL — `entryText` is `undefined` (TypeError) and/or the `themis` global-filter check fails because `filterModel` still reads `a.value`.

- [ ] **Step 3: Implement `entryText` and update `filterModel` in `cdf-model.js`**

Add `entryText` near the top of `wacdfpp/cdf-model.js` (after the `VAR_GROUPS` export):

```js
// Canonical display/compare string for a single attribute value or entry.
// string -> trimmed; numeric/typed array or array -> comma list; else String().
export function entryText(value) {
    if (value == null) return "";
    if (typeof value === "string") return value.trim();
    if (Array.isArray(value) || ArrayBuffer.isView(value))
        return Array.from(value).map(String).join(", ");
    return String(value);
}
```

Replace the global-attribute branch of `filterModel` (the line filtering `model.globalAttributes`) with:

```js
    const globalAttributes = model.globalAttributes.filter(
        a => [a.name, ...a.entries.map(entryText)].join("\n").toLowerCase().includes(q));
```

Update `rawFromCdfFile`'s global mapping (the `rawGlobals` block) to keep entries + types:

```js
    const rawGlobals = cdf.attribute_names().map(name => {
        const a = cdf.get_attribute(name);
        return { name, entries: Array.from(a.entries), types: Array.from(a.types) };
    });
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `node tests/wacdfpp_model/test.mjs`
Expected: PASS — all `ok` lines, including the new `entryText` checks and the existing `filter narrows global attributes` check.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/cdf-model.js tests/wacdfpp_model/test.mjs
git commit -m "wacdfpp: add entryText canonicalizer; keep global-attr entries in model"
```

---

## Task 2: `cdf-diff.js` — variable diffing

**Files:**
- Create: `wacdfpp/cdf-diff.js`
- Create: `tests/wacdfpp_diff/test.mjs`
- Modify: `tests/meson.build`

- [ ] **Step 1: Write the failing test for variable diffing**

Create `tests/wacdfpp_diff/test.mjs`:

```js
// Pure Node test for wacdfpp/cdf-diff.js — no WASM required.
//   node test.mjs
import { buildModel } from "../../wacdfpp/cdf-model.js";
import { diffModels } from "../../wacdfpp/cdf-diff.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

const V = (name, over = {}) => ({
    name, shape: [3], typeName: "CDF_FLOAT", type: 21, isNrv: false,
    varType: "data", attributes: { VAR_TYPE: "data" }, ...over,
});

// Identical models -> every variable "same", no field/attr diffs.
{
    const a = buildModel([V("B")], []);
    const b = buildModel([V("B")], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    check("identical var -> same", row.status === "same");
    check("identical var -> no field diffs", row.fields.length === 0);
    check("identical var -> no attr diffs", row.attributes.length === 0);
}

// Added / removed variables.
{
    const a = buildModel([V("only_a")], []);
    const b = buildModel([V("only_b")], []);
    const d = diffModels(a, b);
    check("removed var", d.groups.data.find(v => v.name === "only_a").status === "removed");
    check("added var", d.groups.data.find(v => v.name === "only_b").status === "added");
}

// Changed shape / type / isNrv.
{
    const a = buildModel([V("B")], []);
    const b = buildModel([V("B", { shape: [4], typeName: "CDF_DOUBLE", type: 22, isNrv: true })], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    const fieldNames = row.fields.map(f => f.field).sort();
    check("changed var -> changed status", row.status === "changed");
    check("changed shape/type/isNrv fields", fieldNames.join() === "isNrv,shape,type");
    const shape = row.fields.find(f => f.field === "shape");
    check("shape field old->new", shape.a === "3" && shape.b === "4");
}

// Per-variable attribute add / remove / change.
{
    const a = buildModel([V("B", { attributes: { VAR_TYPE: "data", UNITS: "nT", FILLVAL: "-1e31" } })], []);
    const b = buildModel([V("B", { attributes: { VAR_TYPE: "data", UNITS: "T", SCALE: "log" } })], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    const byName = Object.fromEntries(row.attributes.map(x => [x.name, x]));
    check("attr changed (UNITS)", byName.UNITS.status === "changed" && byName.UNITS.a === "nT" && byName.UNITS.b === "T");
    check("attr removed (FILLVAL)", byName.FILLVAL.status === "removed" && byName.FILLVAL.b === null);
    check("attr added (SCALE)", byName.SCALE.status === "added" && byName.SCALE.a === null);
    check("unchanged attr not listed (VAR_TYPE absent)", byName.VAR_TYPE === undefined);
}

// Variable placed by effective group (uses B's group; here VAR_TYPE support_data).
{
    const a = buildModel([V("E", { varType: "support_data", attributes: { VAR_TYPE: "support_data" } })], []);
    const b = buildModel([V("E", { varType: "support_data", attributes: { VAR_TYPE: "support_data" } })], []);
    const d = diffModels(a, b);
    check("grouped under support_data", d.groups.support_data.some(v => v.name === "E"));
}

process.exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `node tests/wacdfpp_diff/test.mjs`
Expected: FAIL — `Cannot find module .../wacdfpp/cdf-diff.js`.

- [ ] **Step 3: Implement `cdf-diff.js` (variable diffing)**

Create `wacdfpp/cdf-diff.js`:

```js
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
    return { globalAttributes: [], groups };   // global diff added in Task 3
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `node tests/wacdfpp_diff/test.mjs`
Expected: PASS — all `ok` lines.

- [ ] **Step 5: Register the test in `tests/meson.build`**

After the `wacdfpp_astralint` test block (around line 74), add:

```python
    test('wacdfpp_diff', node,
        args: [files('wacdfpp_diff/test.mjs')],
        timeout: 10,
    )
```

- [ ] **Step 6: Commit**

```bash
git add wacdfpp/cdf-diff.js tests/wacdfpp_diff/test.mjs tests/meson.build
git commit -m "wacdfpp: add cdf-diff variable diffing + Node tests"
```

---

## Task 3: `cdf-diff.js` — per-entry global-attribute diff + summary

**Files:**
- Modify: `wacdfpp/cdf-diff.js`
- Modify: `tests/wacdfpp_diff/test.mjs`

- [ ] **Step 1: Write the failing tests for global-attr diff + summary**

Append to `tests/wacdfpp_diff/test.mjs`, just before the final `process.exit(...)` line. First add the import for `diffSummary` by changing the existing diff import line to:

```js
import { diffModels, diffSummary } from "../../wacdfpp/cdf-diff.js";
```

Then add:

```js
// Global attributes: added / removed / per-entry changed, index-aligned.
{
    const ga = [
        { name: "Project", entries: ["THEMIS"], types: [51] },
        { name: "TimeRes", entries: ["1s", "3s"], types: [51, 51] },
        { name: "OnlyA", entries: ["x"], types: [51] },
    ];
    const gb = [
        { name: "Project", entries: ["THEMIS"], types: [51] },           // same
        { name: "TimeRes", entries: ["1s", "5s", "10s"], types: [51, 51, 51] }, // entry 1 changed, entry 2 added
        { name: "OnlyB", entries: ["y"], types: [51] },                  // added
    ];
    const d = diffModels(buildModel([], ga), buildModel([], gb));
    const by = Object.fromEntries(d.globalAttributes.map(a => [a.name, a]));
    check("global same attr", by.Project.status === "same" && by.Project.entries.length === 0);
    check("global removed attr", by.OnlyA.status === "removed");
    check("global added attr", by.OnlyB.status === "added");
    check("global changed attr", by.TimeRes.status === "changed");
    const e = Object.fromEntries(by.TimeRes.entries.map(x => [x.index, x]));
    check("entry 1 changed 3s->5s", e[1].status === "changed" && e[1].a === "3s" && e[1].b === "5s");
    check("entry 2 added", e[2].status === "added" && e[2].a === null && e[2].b === "10s");
    check("entry 0 unchanged not listed", e[0] === undefined);
}

// Summary counts across globals + variables.
{
    const a = buildModel([V("keep"), V("gone")], [{ name: "A", entries: ["1"], types: [51] }]);
    const b = buildModel([V("keep", { shape: [9] }), V("new")], [{ name: "B", entries: ["2"], types: [51] }]);
    const s = diffSummary(diffModels(a, b));
    check("summary added (new var + B attr)", s.added === 2);
    check("summary removed (gone var + A attr)", s.removed === 2);
    check("summary changed (keep var shape)", s.changed === 1);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `node tests/wacdfpp_diff/test.mjs`
Expected: FAIL — `diffSummary is not a function` and global-attr checks fail (`diffModels` returns `globalAttributes: []`).

- [ ] **Step 3: Implement global-attr diff + summary in `cdf-diff.js`**

Add these functions to `wacdfpp/cdf-diff.js` (before `diffModels`):

```js
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
```

In `diffModels`, replace the return line `return { globalAttributes: [], groups };` with:

```js
    return { globalAttributes: diffGlobals(modelA, modelB), groups };
```

Add `diffSummary` at the end of the file:

```js
// Count added/removed/changed across global attributes and variables.
export function diffSummary(diff) {
    const c = { added: 0, removed: 0, changed: 0 };
    const bump = (s) => { if (s in c) c[s] += 1; };
    for (const a of diff.globalAttributes) bump(a.status);
    for (const g of VAR_GROUPS) for (const v of diff.groups[g]) bump(v.status);
    return c;
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `node tests/wacdfpp_diff/test.mjs`
Expected: PASS — all `ok` lines.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/cdf-diff.js tests/wacdfpp_diff/test.mjs
git commit -m "wacdfpp: per-entry global-attr diff + diff summary"
```

---

## Task 4: Render single-file global attributes via `entryText`

**Files:**
- Modify: `wacdfpp/render.js`

The single-file explorer's global-attr rows currently read `a.value`, which no longer exists. Switch to `entryText`. No automated test (DOM render is verified manually in Task 9).

- [ ] **Step 1: Import `entryText` in `render.js`**

Add at the top of `wacdfpp/render.js` (the file currently has no imports — add this as the first line):

```js
import { entryText } from "./cdf-model.js";
```

- [ ] **Step 2: Update the global-attr row builder**

In `renderList`, replace the `gAttrRows` mapping's inner `innerHTML` assignment so the value uses entries:

```js
        row.innerHTML = `<span class="log-key">${esc(a.name)}</span> ` +
            `<span class="log-dim">${esc(a.entries.map(entryText).join(", "))}</span>`;
```

- [ ] **Step 3: Sanity-check the import resolves (syntax only)**

Run: `node --check wacdfpp/render.js`
Expected: no output (exit 0). (This validates syntax; full render is verified in Task 9.)

- [ ] **Step 4: Commit**

```bash
git add wacdfpp/render.js
git commit -m "wacdfpp: render global attrs from entries via entryText"
```

---

## Task 5: `wasm.js` memoized module loader

**Files:**
- Create: `wacdfpp/wasm.js`
- Modify: `wacdfpp/app.js`

- [ ] **Step 1: Create `wacdfpp/wasm.js`**

```js
// Single shared WASM module instance for all views (explorer + compare).
import createCdfModule from "./cdfpp.js";

let modulePromise;
export function loadModule() {
    if (!modulePromise) modulePromise = createCdfModule();
    return modulePromise;
}
```

- [ ] **Step 2: Use it in `app.js`**

In `wacdfpp/app.js`, replace the import:

```js
import createCdfModule from "./cdfpp.js";
```

with:

```js
import { loadModule } from "./wasm.js";
```

And in `init()`, replace:

```js
        Module = await createCdfModule();
```

with:

```js
        Module = await loadModule();
```

- [ ] **Step 3: Sanity-check syntax**

Run: `node --check wacdfpp/wasm.js && node --check wacdfpp/app.js`
Expected: no output (exit 0).

- [ ] **Step 4: Commit**

```bash
git add wacdfpp/wasm.js wacdfpp/app.js
git commit -m "wacdfpp: memoized shared WASM loader (wasm.js)"
```

---

## Task 6: `compare.js` — orchestration + diff rendering

**Files:**
- Create: `wacdfpp/compare.js`

This module owns compare mode: load two files (file or URL), build a model from each (deleting the `CdfFile` immediately), diff them, and render. Verified manually in Task 9.

- [ ] **Step 1: Create `wacdfpp/compare.js`**

```js
// Compare mode: diff two CDF files on structure + metadata.
// Each file is loaded -> model -> CdfFile deleted immediately (no values needed),
// so we never hold two live CdfFiles and sidestep nomap reference invalidation.
import { loadModule } from "./wasm.js";
import { rawFromCdfFile, buildModel } from "./cdf-model.js";
import { diffModels, diffSummary, STATUS } from "./cdf-diff.js";
import { esc } from "./render.js";

const GROUP_LABELS = { data: "Data", support_data: "Support", metadata: "Metadata" };
const BADGE = { added: "+", removed: "−", changed: "~", same: "·" };

async function bytesToModel(bytes) {
    const Module = await loadModule();
    const cdf = Module.load(bytes);
    if (!cdf.is_valid()) { cdf.delete(); return null; }
    const { rawVars, rawGlobals } = rawFromCdfFile(cdf);
    const model = buildModel(rawVars, rawGlobals);
    cdf.delete();
    return model;
}

function readFile(file) {
    return new Promise((resolve, reject) => {
        const r = new FileReader();
        r.onload = (e) => resolve(new Uint8Array(e.target.result));
        r.onerror = () => reject(new Error(`read ${file.name}`));
        r.readAsArrayBuffer(file);
    });
}

async function fetchBytes(url) {
    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    return new Uint8Array(await resp.arrayBuffer());
}

function diffRow(label, status, detail) {
    const row = document.createElement("div");
    row.className = `diff-row st-${status}`;
    row.dataset.status = status;
    row.innerHTML = `<span class="diff-badge">${BADGE[status]}</span> ` +
        `<span class="diff-name">${esc(label)}</span>` + (detail ? ` ${detail}` : "");
    return row;
}

function changePair(a, b) {
    if (a === null) return `<span class="log-key">${esc(b)}</span>`;
    if (b === null) return `<span class="log-err">${esc(a)}</span>`;
    return `<span class="log-err">${esc(a)}</span> → <span class="log-key">${esc(b)}</span>`;
}

function variableBlock(v) {
    const block = document.createElement("div");
    block.className = "diff-block";
    block.dataset.status = v.status;
    const shapeNote = v.fields.find(f => f.field === "shape");
    block.appendChild(diffRow(v.name, v.status,
        shapeNote ? `<span class="log-dim">[${esc(shapeNote.a)} → ${esc(shapeNote.b)}]</span>` : ""));
    const details = document.createElement("div");
    details.className = "diff-details";
    for (const f of v.fields)
        details.appendChild(detailLine(f.field, changePair(f.a, f.b)));
    for (const a of v.attributes)
        details.appendChild(detailLine(a.name, changePair(a.a, a.b)));
    if (details.children.length) block.appendChild(details);
    return block;
}

function globalBlock(a) {
    const block = document.createElement("div");
    block.className = "diff-block";
    block.dataset.status = a.status;
    block.appendChild(diffRow(a.name, a.status, ""));
    const details = document.createElement("div");
    details.className = "diff-details";
    for (const e of a.entries)
        details.appendChild(detailLine(`[${e.index}]`, changePair(e.a, e.b)));
    if (details.children.length) block.appendChild(details);
    return block;
}

function detailLine(label, html) {
    const el = document.createElement("div");
    el.className = "diff-detail";
    el.innerHTML = `<span class="log-dim">${esc(label)}:</span> ${html}`;
    return el;
}

function section(title, blocks) {
    const wrap = document.createElement("div");
    wrap.className = "diff-section";
    const head = document.createElement("div");
    head.className = "section-label";
    head.textContent = title;
    wrap.appendChild(head);
    for (const b of blocks) wrap.appendChild(b);
    return wrap;
}

export function renderDiff(container, diff) {
    container.innerHTML = "";
    const s = diffSummary(diff);
    const summary = document.createElement("div");
    summary.className = "diff-summary";
    summary.innerHTML =
        `<span class="st-added">${s.added} added</span> · ` +
        `<span class="st-removed">${s.removed} removed</span> · ` +
        `<span class="st-changed">${s.changed} changed</span>`;
    container.appendChild(summary);

    const gBlocks = diff.globalAttributes.filter(a => a.status !== STATUS.SAME).map(globalBlock);
    if (gBlocks.length) container.appendChild(section("Global Attributes", gBlocks));

    for (const g of ["data", "support_data", "metadata"]) {
        const rows = diff.groups[g].filter(v => v.status !== STATUS.SAME).map(variableBlock);
        if (rows.length) container.appendChild(section(GROUP_LABELS[g], rows));
    }

    if (container.children.length === 1)
        container.appendChild(diffRow("No structural differences", "same", ""));
    applyFilter(container, container.dataset.filter || "changes");
}

// Filter: "all" shows everything; "changes" hides same-status blocks (default).
export function applyFilter(container, mode) {
    container.dataset.filter = mode;
    for (const block of container.querySelectorAll(".diff-block"))
        block.style.display = (mode === "all" || block.dataset.status !== "same") ? "" : "none";
}

// Build a model from a source: { file } or { url }. Returns { model, name }.
async function loadSource(src) {
    if (src.file) return { model: await bytesToModel(await readFile(src.file)), name: src.file.name };
    if (src.url) {
        const name = src.url.split("/").pop().split("?")[0] || "remote.cdf";
        return { model: await bytesToModel(await fetchBytes(src.url)), name };
    }
    return { model: null, name: "" };
}

// Public entry: diff two sources into `container`, report status via setStatus.
export async function runCompare(container, srcA, srcB, setStatus) {
    setStatus("loading", "Loading A & B…");
    try {
        const [a, b] = await Promise.all([loadSource(srcA), loadSource(srcB)]);
        if (!a.model || !b.model) { setStatus("error", "Failed to parse one of the files"); return; }
        renderDiff(container, diffModels(a.model, b.model));
        setStatus("ready", `Compared ${a.name} ↔ ${b.name}`);
    } catch (err) {
        setStatus("error", `Compare error: ${err.message}`);
    }
}
```

- [ ] **Step 2: Sanity-check syntax**

Run: `node --check wacdfpp/compare.js`
Expected: no output (exit 0).

- [ ] **Step 3: Commit**

```bash
git add wacdfpp/compare.js
git commit -m "wacdfpp: compare.js — load/diff/render two CDFs"
```

---

## Task 7: HTML — Compare toggle, A/B inputs, result container, diff CSS

**Files:**
- Modify: `wacdfpp/wacdfpp.html`

- [ ] **Step 1: Add a Compare toggle to the header**

In `wacdfpp/wacdfpp.html`, replace the header block:

```html
    <header>
        <h1>CDFpp Explorer</h1>
        <span class="badge">WebAssembly</span>
        <span class="tagline">inspect · plot · validate CDF files in your browser</span>
    </header>
```

with:

```html
    <header>
        <h1>CDFpp Explorer</h1>
        <span class="badge">WebAssembly</span>
        <span class="tagline">inspect · plot · validate · compare CDF files in your browser</span>
        <button id="modeToggle" class="header-btn" style="margin-left:auto">Compare ↔</button>
    </header>
```

- [ ] **Step 2: Add the compare input section + result container**

Immediately after the closing `</div>` of `<div class="input-section">…</div>`, add:

```html
    <div class="input-section" id="compareInputs" style="display:none">
        <div class="card">
            <label>File A</label>
            <div class="row">
                <input type="file" id="fileA" accept=".cdf">
                <input type="text" id="urlA" placeholder="…or https URL">
            </div>
        </div>
        <div class="card">
            <label>File B</label>
            <div class="row">
                <input type="file" id="fileB" accept=".cdf">
                <input type="text" id="urlB" placeholder="…or https URL">
            </div>
        </div>
    </div>
    <div id="compareBar" style="display:none; margin-bottom:1rem; gap:0.5rem;">
        <button id="compareBtn">Compare</button>
        <button id="filterToggle" class="header-btn">Show all</button>
    </div>
    <div id="compareResult" class="detail" style="display:none; max-height:75vh;"></div>
```

- [ ] **Step 3: Add diff CSS**

Just before the closing `</style>` tag, add:

```css
        .diff-summary { font-family: var(--mono); font-size: 0.8rem; margin-bottom: 0.75rem; }
        .diff-section { margin-bottom: 0.75rem; }
        .diff-block { font-family: var(--mono); font-size: 0.78rem; padding: 0.1rem 0; }
        .diff-row { white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
        .diff-badge { display: inline-block; width: 1.1em; text-align: center; font-weight: 700; }
        .diff-name { color: var(--text); }
        .diff-details { padding: 0.1rem 0 0.3rem 1.6rem; }
        .diff-detail { white-space: normal; overflow-wrap: anywhere; color: var(--text-dim); }
        .st-added .diff-badge, .st-added { color: var(--green); }
        .st-removed .diff-badge, .st-removed { color: var(--red); }
        .st-changed .diff-badge, .st-changed { color: var(--yellow); }
        .st-same .diff-badge { color: var(--text-dim); }
```

- [ ] **Step 4: Sanity-check the HTML parses (no script execution)**

Run: `node -e "const fs=require('fs');const h=fs.readFileSync('wacdfpp/wacdfpp.html','utf8');['modeToggle','compareInputs','fileA','urlB','compareBtn','filterToggle','compareResult'].forEach(id=>{if(!h.includes('id=\"'+id+'\"'))throw new Error('missing '+id)});console.log('ok')"`
Expected: `ok`.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/wacdfpp.html
git commit -m "wacdfpp: compare-mode HTML (toggle, A/B inputs, result, diff CSS)"
```

---

## Task 8: Wire compare mode into `app.js` (toggle + URL params)

**Files:**
- Modify: `wacdfpp/app.js`

- [ ] **Step 1: Import compare entry points**

Add to the imports at the top of `wacdfpp/app.js`:

```js
import { runCompare, applyFilter } from "./compare.js";
```

- [ ] **Step 2: Grab the new elements**

Add these entries to the `els` object literal:

```js
    modeToggle: document.getElementById("modeToggle"),
    compareInputs: document.getElementById("compareInputs"),
    compareBar: document.getElementById("compareBar"),
    compareResult: document.getElementById("compareResult"),
    fileA: document.getElementById("fileA"),
    urlA: document.getElementById("urlA"),
    fileB: document.getElementById("fileB"),
    urlB: document.getElementById("urlB"),
    compareBtn: document.getElementById("compareBtn"),
    filterToggle: document.getElementById("filterToggle"),
```

- [ ] **Step 3: Add mode toggling + compare wiring**

Add this block just before `async function init()`:

```js
let compareMode = false;

function setCompareMode(on) {
    compareMode = on;
    els.modeToggle.textContent = on ? "Inspect ↩" : "Compare ↔";
    els.compareInputs.style.display = on ? "grid" : "none";
    els.compareBar.style.display = on ? "flex" : "none";
    els.compareResult.style.display = on ? "block" : "none";
    // single-file controls
    document.querySelector(".input-section").style.display = on ? "none" : "grid";
    els.app.style.display = on ? "none" : "grid";
    document.getElementById("output-header").style.display = on ? "none" : "flex";
}

function runCompareFromInputs() {
    runCompare(
        els.compareResult,
        { file: els.fileA.files[0], url: els.urlA.value.trim() || null },
        { file: els.fileB.files[0], url: els.urlB.value.trim() || null },
        setStatus,
    );
}

els.modeToggle.addEventListener("click", () => setCompareMode(!compareMode));
els.compareBtn.addEventListener("click", runCompareFromInputs);
els.filterToggle.addEventListener("click", () => {
    const all = els.compareResult.dataset.filter !== "all";
    applyFilter(els.compareResult, all ? "all" : "changes");
    els.filterToggle.textContent = all ? "Show changes" : "Show all";
});
```

- [ ] **Step 4: Honor URL params in `init()`**

In `init()`, replace the existing `?url=` handling block:

```js
        const params = new URLSearchParams(globalThis.location.search);
        const url = params.get("url") || params.get("cdf");
        if (url) { els.urlInput.value = url; fetchUrl(url); }
```

with:

```js
        const params = new URLSearchParams(globalThis.location.search);
        const a = params.get("a"), b = params.get("b");
        if (params.has("compare") || a || b) {
            setCompareMode(true);
            if (a) els.urlA.value = a;
            if (b) els.urlB.value = b;
            if (a && b) runCompareFromInputs();
        } else {
            const url = params.get("url") || params.get("cdf");
            if (url) { els.urlInput.value = url; fetchUrl(url); }
        }
```

- [ ] **Step 5: Sanity-check syntax**

Run: `node --check wacdfpp/app.js`
Expected: no output (exit 0).

- [ ] **Step 6: Commit**

```bash
git add wacdfpp/app.js
git commit -m "wacdfpp: wire compare mode toggle + ?compare/?a=&b= URL params"
```

---

## Task 9: Deploy new files in meson + full verification

**Files:**
- Modify: `wacdfpp/meson.build`

- [ ] **Step 1: Add new JS files to `extra_files` and `fs.copyfile`**

In `wacdfpp/meson.build`, add `'wasm.js', 'cdf-diff.js', 'compare.js'` to the `extra_files` list (the array ending with `'uPlot.esm.js', 'uPlot.min.css'`):

```python
        extra_files: ['wasm.txt', 'wacdfpp.html', 'app.js', 'cdf-model.js', 'render.js',
                      'plot-model.js', 'spectrogram.js', 'plot.js', 'astralint.js',
                      'wasm.js', 'cdf-diff.js', 'compare.js',
                      'uPlot.esm.js', 'uPlot.min.css']
```

And add three `fs.copyfile` lines alongside the others (after the `astralint.js` copy):

```python
    fs.copyfile('wasm.js', 'wasm.js')
    fs.copyfile('cdf-diff.js', 'cdf-diff.js')
    fs.copyfile('compare.js', 'compare.js')
```

- [ ] **Step 2: Run the full pure-JS test suite**

Run: `node tests/wacdfpp_model/test.mjs && node tests/wacdfpp_diff/test.mjs && node tests/wacdfpp_format/test.mjs`
Expected: all three print only `ok` lines and exit 0.

- [ ] **Step 3: Rebuild the WASM app and serve it**

Run:
```bash
ninja -C build_wasm cdfpp
( cd build_wasm/wacdfpp && python3 -m http.server 8000 )
```
Expected: build succeeds; server starts. (If `build_wasm` is not configured, the existing repo already contains it; otherwise configure with the emscripten cross-file used for the project.)

- [ ] **Step 4: Manual verification in a browser**

Open `http://localhost:8000/wacdfpp.html` and verify:
1. Single-file mode still works: load a CDF from `tests/resources/`, global attributes render with values (regression for the `entryText` change).
2. Click **Compare ↔** — the A/B inputs and result panel appear; the single-file explorer hides.
3. Load two different CDFs (e.g. two files from `tests/resources/`), click **Compare**: a summary line and color-coded added/removed/changed variables + global attributes appear; changed rows show `old → new`.
4. Compare a file against itself: shows "No structural differences".
5. **Show all** / **Show changes** toggles same-status visibility.
6. Open `http://localhost:8000/wacdfpp.html?a=<urlA>&b=<urlB>` with two reachable CDF URLs: it opens in compare mode and runs automatically.
7. Click **Inspect ↩** — returns to the single-file explorer.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/meson.build
git commit -m "wacdfpp: deploy wasm.js/cdf-diff.js/compare.js in meson"
```

---

## Self-Review Notes (for the implementer)

- The global-attr model shape change (`{name, value}` → `{name, entries, types}`) is the one cross-cutting change: it touches `rawFromCdfFile`, `filterModel`, the `wacdfpp_model` test, and `render.js`. Tasks 1 and 4 cover all four; do not skip Task 4 or the single-file global-attr display will break.
- `entryText` is the single canonicalizer; `cdf-diff.js`, `cdf-model.js`, and `render.js` all use it. Do not reintroduce a second value-stringifier.
- Compare mode never keeps a `CdfFile` alive (see `bytesToModel`), so it is immune to the nomap reference-invalidation limitation.
- DOM tasks (4, 6, 7, 8) have no Node unit tests by project convention; they are verified together in Task 9.
