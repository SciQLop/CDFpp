# WASM Demo Phase 1 — UX Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restructure the WASM demo (`wacdfpp/`) from a single-file flat metadata dump into a master/detail single-page app with a searchable, VAR_TYPE-grouped variable list and a detail panel — the scaffold Phase 2 (plotting) and Phase 3 (edit/validate) plug into.

**Architecture:** No-build, vanilla ES modules served statically next to `cdfpp.js`. Split the inline script into `cdf-model.js` (pure data view over `CdfFile`: VAR_TYPE grouping + search), `render.js` (pure formatters + DOM rendering of list/detail), and `app.js` (orchestration: load/fetch/drag-drop, state, selection). `wacdfpp.html` becomes markup + CSS only. No C++/`wacdfpp.cpp` changes — `VAR_TYPE`, shape, type, and attributes are already reachable via existing bindings.

**Tech Stack:** C++20/Emscripten (unchanged), vanilla ES modules, Node (hand-rolled test runner matching the existing `tests/wasm_*/test.mjs` pattern), Meson/Ninja.

**Spec:** `docs/superpowers/specs/2026-06-18-wasm-demo-app-design.md`

**Dev loop (for all manual verification):**
```bash
source ~/Documents/prog/emsdk/emsdk_env.sh
ninja -C build_wasm
meson test -C build_wasm           # never the full_corpus test (network)
```
Serve the demo from the build dir and open `wacdfpp.html` (e.g. `python3 -m http.server` in `build_wasm/wacdfpp`). Use a real multi-variable CDF from `tests/resources/`.

---

## File Structure

```
wacdfpp/
  wacdfpp.html      # MODIFIED: shell markup (two-pane) + CSS only; script becomes <script type="module" src="./app.js">
  app.js            # NEW: load/fetch/drag-drop, app state, selection, search wiring
  cdf-model.js      # NEW: normalizeVarType / buildModel / filterModel (pure) + rawFromCdfFile (browser adapter)
  render.js         # NEW: pure formatters (nsToISO, stripPadding, decodeChars, chunkRecords, formatAttrValue) + DOM renderers (renderList, renderDetail)
  meson.build       # MODIFIED: copy new JS files into build dir; register pure cdf-model node test
tests/
  wacdfpp_model/test.mjs   # NEW: pure Node test for cdf-model.js (no WASM)
  wacdfpp_format/test.mjs  # NEW: pure Node test for render.js formatters (no WASM)
```

**Cross-task interface contract (keep names identical across tasks):**

`cdf-model.js`:
- `VAR_GROUPS = ["data", "support_data", "metadata"]`
- `normalizeVarType(varType) -> "data"|"support_data"|"metadata"`
- `buildModel(rawVars, rawGlobals) -> { globalAttributes, groups }`
  - `rawVars` item: `{ name, shape:number[], typeName:string, type:number, isNrv:boolean, varType:string|undefined, attributes:{[k]:any} }`
  - `rawGlobals` item: `{ name:string, value:string }`
  - returns `{ globalAttributes: rawGlobals, groups: { data:[], support_data:[], metadata:[] } }`
- `filterModel(model, query) -> model` (same shape; empty query returns model unchanged)
- `rawFromCdfFile(cdf) -> { rawVars, rawGlobals }` (browser only; not exercised by Node tests)

`render.js`:
- pure: `nsToISO(bigint)`, `stripPadding(string)`, `decodeChars(bytes, shape, max) -> {strings, total}`, `chunkRecords(array, recLen) -> array[]`, `formatAttrValue(value) -> string`
- DOM: `renderList(container, model, { selected, onSelect })`, `renderDetail(container, cdf, name)`

`app.js` state: `Module`, `currentCdf` (the live `CdfFile`, must be `.delete()`d before replacing), `currentModel`, `selectedName`, `searchQuery`.

---

## Task 1: cdf-model.js — grouping & filtering (pure, TDD)

**Files:**
- Create: `wacdfpp/cdf-model.js`
- Test: `tests/wacdfpp_model/test.mjs`
- Modify: `wacdfpp/meson.build`

- [ ] **Step 1: Write the failing test**

Create `tests/wacdfpp_model/test.mjs`:

```js
// Pure Node test for wacdfpp/cdf-model.js — no WASM required.
//   node test.mjs
import {
    VAR_GROUPS, normalizeVarType, buildModel, filterModel,
} from "../../wacdfpp/cdf-model.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

check("VAR_GROUPS order", eq(VAR_GROUPS, ["data", "support_data", "metadata"]));
check("normalizeVarType default", normalizeVarType(undefined) === "data");
check("normalizeVarType case-insensitive", normalizeVarType("Support_Data") === "support_data");
check("normalizeVarType unknown -> data", normalizeVarType("weird") === "data");

const rawVars = [
    { name: "Epoch", shape: [3], typeName: "CDF_EPOCH", type: 31, isNrv: false,
      varType: "support_data", attributes: { VAR_TYPE: "support_data", UNITS: "ns" } },
    { name: "B_gse", shape: [3, 3], typeName: "CDF_FLOAT", type: 21, isNrv: false,
      varType: "data", attributes: { VAR_TYPE: "data", UNITS: "nT", DEPEND_0: "Epoch" } },
    { name: "labl", shape: [3], typeName: "CDF_CHAR", type: 51, isNrv: true,
      varType: "metadata", attributes: { VAR_TYPE: "metadata" } },
    { name: "noType", shape: [1], typeName: "CDF_INT4", type: 4, isNrv: false,
      varType: undefined, attributes: {} },
];
const rawGlobals = [
    { name: "Project", value: "THEMIS" },
    { name: "Discipline", value: "Space Physics" },
];
const model = buildModel(rawVars, rawGlobals);

check("data group holds B_gse + untyped noType",
    eq(model.groups.data.map(v => v.name), ["B_gse", "noType"]));
check("support_data group holds Epoch",
    eq(model.groups.support_data.map(v => v.name), ["Epoch"]));
check("metadata group holds labl",
    eq(model.groups.metadata.map(v => v.name), ["labl"]));
check("globalAttributes passed through", eq(model.globalAttributes, rawGlobals));

check("filter empty returns same model", filterModel(model, "  ") === model);

const byName = filterModel(model, "b_gse");
check("filter by var name", eq(byName.groups.data.map(v => v.name), ["B_gse"]));
check("filter by var name empties others",
    byName.groups.support_data.length === 0 && byName.groups.metadata.length === 0);

const byAttrValue = filterModel(model, "nT");
check("filter by attr value matches B_gse", eq(byAttrValue.groups.data.map(v => v.name), ["B_gse"]));

const byAttrName = filterModel(model, "depend_0");
check("filter by attr name matches B_gse", eq(byAttrName.groups.data.map(v => v.name), ["B_gse"]));

const byGlobal = filterModel(model, "themis");
check("filter narrows global attributes",
    eq(byGlobal.globalAttributes.map(a => a.name), ["Project"]));

const noMatch = filterModel(model, "zzzzz");
check("no match empties all var groups",
    noMatch.groups.data.length === 0 && noMatch.groups.support_data.length === 0 &&
    noMatch.groups.metadata.length === 0);

process.exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_model/test.mjs`
Expected: FAIL — `ERR_MODULE_NOT_FOUND` for `wacdfpp/cdf-model.js`.

- [ ] **Step 3: Write minimal implementation**

Create `wacdfpp/cdf-model.js`:

```js
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

export function buildModel(rawVars, rawGlobals) {
    const groups = { data: [], support_data: [], metadata: [] };
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
    const groups = { data: [], support_data: [], metadata: [] };
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
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_model/test.mjs`
Expected: all `ok` lines, exit 0.

- [ ] **Step 5: Register the pure test in Meson**

In `wacdfpp/meson.build`, add a `node` lookup and the pure test **outside** the `if ... == 'emscripten'` block (so it runs in normal native builds too). At the very top of the file, after `fs = import('fs')`, add:

```meson
node = find_program('node', required: false)
if node.found()
    test('wacdfpp_model', node,
        args: [files('../tests/wacdfpp_model/test.mjs')],
        timeout: 60,
    )
endif
```

Then **remove** the now-duplicate `node = find_program('node', required: false)` line that currently sits inside the emscripten block (keep the `if node.found()` that guards the wasm tests).

- [ ] **Step 6: Verify the test runs under Meson**

Run: `meson test -C build wacdfpp_model` (any existing native build dir; create with `meson setup build` if needed).
Expected: `wacdfpp_model` PASS.

- [ ] **Step 7: Commit**

```bash
git add wacdfpp/cdf-model.js tests/wacdfpp_model/test.mjs wacdfpp/meson.build
git commit -m "wacdfpp: add cdf-model (VAR_TYPE grouping + search) with pure test"
```

---

## Task 2: render.js — pure formatters (TDD)

**Files:**
- Create: `wacdfpp/render.js`
- Test: `tests/wacdfpp_format/test.mjs`
- Modify: `wacdfpp/meson.build`

- [ ] **Step 1: Write the failing test**

Create `tests/wacdfpp_format/test.mjs`:

```js
// Pure Node test for wacdfpp/render.js formatters — no WASM, no DOM.
//   node test.mjs
import { nsToISO, stripPadding, chunkRecords, formatAttrValue } from "../../wacdfpp/render.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

check("nsToISO whole second",
    nsToISO(1700000000000000000n) === "2023-11-14T22:13:20.000000000Z");
check("nsToISO sub-ms precision",
    nsToISO(1700000000123456789n) === "2023-11-14T22:13:20.123456789Z");

check("stripPadding trims nulls then space",
    stripPadding("hello    ") === "hello");
check("stripPadding empty", stripPadding("  ") === "");

check("chunkRecords 2x3", eq(chunkRecords([1, 2, 3, 4, 5, 6], 3), [[1, 2, 3], [4, 5, 6]]));
check("chunkRecords recLen<=0 -> single row", eq(chunkRecords([1, 2, 3], 0), [[1, 2, 3]]));
check("chunkRecords ragged tail kept", eq(chunkRecords([1, 2, 3, 4, 5], 2), [[1, 2], [3, 4], [5]]));

check("formatAttrValue string quotes+trims", formatAttrValue("  hi  ") === '"hi"');
check("formatAttrValue array joins", formatAttrValue([1, 2, 3]) === "1, 2, 3");
check("formatAttrValue scalar passthrough", formatAttrValue(42) === 42);

process.exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_format/test.mjs`
Expected: FAIL — `ERR_MODULE_NOT_FOUND` for `wacdfpp/render.js`.

- [ ] **Step 3: Write minimal implementation (formatters only)**

Create `wacdfpp/render.js` with the pure formatters first (DOM renderers added in Task 3):

```js
// Rendering for the wacdfpp master/detail UI. The formatters at the top are pure
// (no DOM) and are unit-tested in Node; the renderList/renderDetail functions
// (Task 3) build DOM and are verified manually via the dev loop.

export const CDF_EPOCH = 31, CDF_EPOCH16 = 32, CDF_TIME_TT2000 = 33;
export const CDF_CHAR = 51, CDF_UCHAR = 52;

export const isTimeType = (t) => t === CDF_EPOCH || t === CDF_EPOCH16 || t === CDF_TIME_TT2000;
export const isCharType = (t) => t === CDF_CHAR || t === CDF_UCHAR;

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
    if (strLen > 0 && bytes.length > strLen && bytes.length % strLen === 0) {
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

// Render one attribute value: string -> quoted/trimmed, array -> comma list,
// anything else (numeric typed array / scalar) -> as-is.
export function formatAttrValue(value) {
    if (typeof value === "string") return `"${value.trim()}"`;
    if (Array.isArray(value)) return value.map(String).join(", ");
    return value;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_format/test.mjs`
Expected: all `ok` lines, exit 0.

> Note: `formatAttrValue` here returns the raw string (e.g. `"hi"`); HTML escaping is applied at the DOM layer in Task 3 via `esc()`. This matches the test which checks the unescaped value.

- [ ] **Step 5: Register the formatter test in Meson**

In `wacdfpp/meson.build`, inside the same top-level `if node.found()` block added in Task 1, add:

```meson
    test('wacdfpp_format', node,
        args: [files('../tests/wacdfpp_format/test.mjs')],
        timeout: 60,
    )
```

- [ ] **Step 6: Verify under Meson**

Run: `meson test -C build wacdfpp_format`
Expected: `wacdfpp_format` PASS.

- [ ] **Step 7: Commit**

```bash
git add wacdfpp/render.js tests/wacdfpp_format/test.mjs wacdfpp/meson.build
git commit -m "wacdfpp: add render formatters with pure tests"
```

---

## Task 3: render.js — DOM renderers (list + detail)

**Files:**
- Modify: `wacdfpp/render.js` (append DOM functions)

DOM rendering has no headless test harness (vanilla, no-build); verify manually in Task 6. Keep these functions free of app state — they receive data + callbacks.

- [ ] **Step 1: Append `renderList` to `wacdfpp/render.js`**

```js
// --- DOM rendering ------------------------------------------------------------

const GROUP_LABELS = { data: "Data", support_data: "Support", metadata: "Metadata" };

function varRow(v, selected, onSelect) {
    const row = document.createElement("div");
    row.className = "var-row" + (v.name === selected ? " sel" : "");
    row.innerHTML =
        `<span class="log-key">${esc(v.name)}</span> ` +
        `<span class="log-dim">[${esc(v.shape.join(", "))}]</span> ` +
        `<span class="log-val">${esc(v.typeName)}</span>` +
        (v.isNrv ? `<span class="log-dim"> nrv</span>` : "");
    row.addEventListener("click", () => onSelect(v.name));
    return row;
}

function group(title, count, children, startOpen) {
    const wrap = document.createElement("div");
    wrap.className = "group";
    const head = document.createElement("div");
    head.className = "group-head";
    head.innerHTML = `<span class="caret">${startOpen ? "▾" : "▸"}</span> ` +
        `<span class="group-title">${esc(title)}</span> <span class="log-dim">(${count})</span>`;
    const body = document.createElement("div");
    body.className = "group-body";
    body.style.display = startOpen ? "block" : "none";
    for (const c of children) body.appendChild(c);
    head.addEventListener("click", () => {
        const open = body.style.display === "none";
        body.style.display = open ? "block" : "none";
        head.querySelector(".caret").textContent = open ? "▾" : "▸";
    });
    wrap.append(head, body);
    return wrap;
}

// Render the left list. container is cleared and repopulated.
export function renderList(container, model, { selected, onSelect }) {
    container.innerHTML = "";

    const gAttrRows = model.globalAttributes.map(a => {
        const row = document.createElement("div");
        row.className = "attr-row";
        row.innerHTML = `<span class="log-key">${esc(a.name)}</span> ` +
            `<span class="log-dim">${esc(a.value)}</span>`;
        return row;
    });
    if (gAttrRows.length)
        container.appendChild(group("Global Attributes", gAttrRows.length, gAttrRows, false));

    for (const g of ["data", "support_data", "metadata"]) {
        const vars = model.groups[g];
        if (!vars.length) continue;
        const rows = vars.map(v => varRow(v, selected, onSelect));
        container.appendChild(group(GROUP_LABELS[g], vars.length, rows, g === "data"));
    }

    if (!container.children.length)
        container.innerHTML = `<div class="log-dim" style="padding:0.5rem">No matches</div>`;
}
```

- [ ] **Step 2: Append `renderDetail` to `wacdfpp/render.js`**

```js
// Record length = product of non-record dims (shape[1:]); 1 for 0/1-D vars.
function recordLength(shape) {
    if (shape.length <= 1) return 1;
    return shape.slice(1).reduce((a, b) => a * b, 1);
}

const PREVIEW_RECORDS = 20;

function previewTable(cdf, v) {
    const table = document.createElement("table");
    table.className = "preview";

    if (isTimeType(v.type)) {
        const ns = cdf.time_values_as_ns_since_1970(v.name);
        const total = ns ? ns.length : 0;
        const n = Math.min(PREVIEW_RECORDS, total);
        for (let i = 0; i < n; i++) {
            const tr = table.insertRow();
            tr.insertCell().textContent = i;
            tr.insertCell().textContent = nsToISO(ns[i]);
        }
        return { table, total, shown: n };
    }

    if (isCharType(v.type) && v.values !== undefined) {
        const { strings, total } = decodeChars(v.values, v.shape, PREVIEW_RECORDS);
        strings.forEach((s, i) => {
            const tr = table.insertRow();
            tr.insertCell().textContent = i;
            tr.insertCell().textContent = s;
        });
        return { table, total, shown: strings.length };
    }

    const values = v.copy_values;
    if (values === undefined || values.length === 0) return { table, total: 0, shown: 0 };
    const recLen = recordLength(v.shape);
    const records = chunkRecords(values, recLen);
    const total = records.length;
    const n = Math.min(PREVIEW_RECORDS, total);
    for (let i = 0; i < n; i++) {
        const tr = table.insertRow();
        tr.insertCell().textContent = i;
        tr.insertCell().textContent = records[i].join(", ");
    }
    return { table, total, shown: n };
}

// Render the right detail panel for variable `name`. Re-fetches the variable so
// values are read immediately (avoids holding stale views across heap growth).
export function renderDetail(container, cdf, name) {
    container.innerHTML = "";
    const v = cdf.get_variable(name);
    if (v === undefined) {
        container.innerHTML = `<div class="log-err">Variable not found: ${esc(name)}</div>`;
        return;
    }

    const head = document.createElement("div");
    head.className = "detail-title";
    head.innerHTML = `<span class="log-key">${esc(name)}</span> ` +
        `<span class="log-dim">[${esc(Array.from(v.shape).join(", "))}]</span> ` +
        `<span class="log-val">${esc(v.type_name)}</span>` +
        (v.is_nrv ? `<span class="log-dim"> nrv</span>` : "");
    container.appendChild(head);

    const meta = document.createElement("div");
    meta.className = "log-dim";
    meta.textContent = `compression: ${v.compression}`;
    container.appendChild(meta);

    const attrs = document.createElement("div");
    attrs.className = "detail-attrs";
    for (const an of v.attribute_names) {
        const row = document.createElement("div");
        row.innerHTML = `<span class="log-dim">${esc(an)}:</span> ${esc(formatAttrValue(v.attributes[an]))}`;
        attrs.appendChild(row);
    }
    container.append(sectionLabel("Attributes"), attrs);

    const { table, total, shown } = previewTable(cdf, v);
    container.appendChild(sectionLabel(`Values (showing ${shown} of ${total})`));
    container.appendChild(table);

    const ph = document.createElement("div");
    ph.className = "plot-placeholder";
    ph.textContent = "📈 Plot — coming in Phase 2";
    container.appendChild(ph);
}

function sectionLabel(text) {
    const el = document.createElement("div");
    el.className = "section-label";
    el.textContent = text;
    return el;
}
```

- [ ] **Step 3: Sanity-check the formatter test still passes**

Run: `node tests/wacdfpp_format/test.mjs`
Expected: all `ok` (appending DOM functions must not break the pure exports; importing the module in Node must not touch `document` at top level).

- [ ] **Step 4: Commit**

```bash
git add wacdfpp/render.js
git commit -m "wacdfpp: add list + detail DOM renderers"
```

---

## Task 4: app.js — orchestration + new HTML shell

**Files:**
- Create: `wacdfpp/app.js`
- Modify: `wacdfpp/wacdfpp.html`

- [ ] **Step 1: Create `wacdfpp/app.js`**

```js
// Orchestration: load (file/URL/?url=/drag-drop), hold current CdfFile + model +
// selection, wire search and list selection to re-render.
import createCdfModule from "./cdfpp.js";
import { rawFromCdfFile, buildModel, filterModel } from "./cdf-model.js";
import { renderList, renderDetail } from "./render.js";

const els = {
    output: document.getElementById("output"),
    fileInput: document.getElementById("fileInput"),
    loadBtn: document.getElementById("loadBtn"),
    fetchBtn: document.getElementById("fetchBtn"),
    urlInput: document.getElementById("urlInput"),
    status: document.getElementById("status"),
    statusText: document.getElementById("statusText"),
    fileName: document.getElementById("fileName"),
    parseTime: document.getElementById("parseTime"),
    search: document.getElementById("search"),
    varlist: document.getElementById("varlist"),
    detail: document.getElementById("detail-body"),
    dropzone: document.getElementById("dropzone"),
};

let Module;
let currentCdf;          // live CdfFile — delete before replacing
let currentModel;        // built once per file
let selectedName = null;
let searchQuery = "";

function setStatus(cls, text) { els.status.className = cls; els.statusText.textContent = text; }

function refreshList() {
    const view = filterModel(currentModel, searchQuery);
    renderList(els.varlist, view, {
        selected: selectedName,
        onSelect: selectVariable,
    });
}

function selectVariable(name) {
    selectedName = name;
    renderDetail(els.detail, currentCdf, name);
    refreshList();   // update selection highlight
}

function inspect(data, name, dt) {
    if (currentCdf) { currentCdf.delete(); currentCdf = undefined; }
    const cdf = Module.load(data);
    if (!cdf.is_valid()) {
        setStatus("error", `Failed to parse ${name}`);
        els.detail.innerHTML = `<div class="log-err">ERROR: failed to parse CDF</div>`;
        cdf.delete();
        return;
    }
    currentCdf = cdf;
    const { rawVars, rawGlobals } = rawFromCdfFile(cdf);
    currentModel = buildModel(rawVars, rawGlobals);
    selectedName = null;
    searchQuery = "";
    els.search.value = "";
    els.fileName.textContent = name;
    els.parseTime.textContent = `parsed in ${dt} ms`;
    els.detail.innerHTML = `<div class="log-dim">Select a variable to inspect.</div>`;
    refreshList();
    setStatus("ready", `Loaded ${name} (${data.length.toLocaleString()} bytes)`);
}

function loadFile(file) {
    setStatus("loading", `Loading ${file.name}...`);
    const reader = new FileReader();
    reader.onload = (e) => {
        const data = new Uint8Array(e.target.result);
        const t0 = performance.now();
        inspect(data, file.name, (performance.now() - t0).toFixed(1));
    };
    reader.readAsArrayBuffer(file);
}

async function fetchUrl(url) {
    if (!url || !Module) return;
    setStatus("loading", "Fetching...");
    try {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const data = new Uint8Array(await resp.arrayBuffer());
        const name = url.split("/").pop().split("?")[0].split("#")[0] || "remote.cdf";
        const t0 = performance.now();
        inspect(data, name, (performance.now() - t0).toFixed(1));
    } catch (err) {
        setStatus("error", `Fetch error: ${err.message}`);
    }
}

els.loadBtn.addEventListener("click", () => {
    const file = els.fileInput.files[0];
    if (file) loadFile(file);
});
els.fetchBtn.addEventListener("click", () => fetchUrl(els.urlInput.value.trim()));
els.urlInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") fetchUrl(els.urlInput.value.trim());
});
els.search.addEventListener("input", () => {
    if (!currentModel) return;
    searchQuery = els.search.value;
    refreshList();
});

async function init() {
    try {
        Module = await createCdfModule();
        setStatus("ready", "Ready");
        els.loadBtn.disabled = false;
        els.fetchBtn.disabled = false;
        const params = new URLSearchParams(globalThis.location.search);
        const url = params.get("url") || params.get("cdf");
        if (url) { els.urlInput.value = url; fetchUrl(url); }
    } catch (err) {
        setStatus("error", `Init failed: ${err.message}`);
    }
}

await init();

export { loadFile };   // used by drag-drop wiring (Task 5)
```

- [ ] **Step 2: Replace the body + script of `wacdfpp.html`**

Replace everything from `<body>` to `</body>` in `wacdfpp/wacdfpp.html` with the two-pane shell (keep the existing `<head>`/`<style>`; CSS additions come in Step 3):

```html
<body>
<div class="container">
    <header>
        <h1>CDFpp WASM Demo</h1>
        <span class="badge">WebAssembly</span>
    </header>

    <div class="input-section">
        <div class="card">
            <label>Load local file</label>
            <div class="row">
                <input type="file" id="fileInput" accept=".cdf">
                <button id="loadBtn" disabled>Inspect</button>
            </div>
        </div>
        <div class="card">
            <label>Fetch from URL</label>
            <div class="row">
                <input type="text" id="urlInput" placeholder="https://example.com/data.cdf">
                <button id="fetchBtn" disabled>Fetch</button>
            </div>
        </div>
    </div>

    <div id="status" class="loading">
        <span class="dot"></span>
        <span id="statusText">Initializing WASM module...</span>
    </div>

    <div id="output-header">
        <span id="fileName">No file loaded</span>
        <span id="parseTime"></span>
    </div>

    <div class="app">
        <aside class="sidebar">
            <input type="text" id="search" placeholder="Search variables & attributes…">
            <div id="varlist"></div>
        </aside>
        <section class="detail">
            <div id="detail-body"><div class="log-dim">Load a CDF to begin.</div></div>
        </section>
    </div>
    <!-- legacy node kept so older code paths referencing #output don't crash -->
    <div id="output" hidden></div>
</div>
<div id="dropzone" class="dropzone"><span>Drop a .cdf file</span></div>
<script type="module" src="./app.js"></script>
</body>
```

- [ ] **Step 3: Add the master/detail CSS**

Append to the existing `<style>` block in `wacdfpp/wacdfpp.html`:

```css
.app {
    display: grid;
    grid-template-columns: 320px 1fr;
    gap: 1rem;
    margin-top: 1rem;
}
@media (max-width: 720px) { .app { grid-template-columns: 1fr; } }

.sidebar, .detail {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    padding: 0.75rem;
    max-height: 70vh;
    overflow-y: auto;
    scrollbar-width: thin;
    scrollbar-color: var(--border) transparent;
}

#search {
    width: 100%;
    margin-bottom: 0.75rem;
    font-family: var(--mono);
    font-size: 0.8rem;
    padding: 0.45em 0.7em;
    background: var(--bg);
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 4px;
    outline: none;
}
#search:focus { border-color: var(--accent); }

.group { margin-bottom: 0.5rem; }
.group-head { cursor: pointer; font-size: 0.75rem; color: var(--text-dim);
    text-transform: uppercase; letter-spacing: 0.05em; padding: 0.25rem 0; user-select: none; }
.group-title { color: var(--text); }
.group-body { font-family: var(--mono); font-size: 0.78rem; }

.var-row, .attr-row { padding: 0.2rem 0.4rem; border-radius: 3px; cursor: pointer;
    white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.var-row:hover, .attr-row:hover { background: var(--surface2); }
.var-row.sel { background: var(--accent-dim); border-left: 2px solid var(--accent); }

.detail-title { font-family: var(--mono); font-size: 0.9rem; margin-bottom: 0.4rem; }
.section-label { font-size: 0.72rem; color: var(--text-dim); text-transform: uppercase;
    letter-spacing: 0.05em; margin: 0.9rem 0 0.4rem; }
.detail-attrs { font-family: var(--mono); font-size: 0.78rem; line-height: 1.6; }

table.preview { width: 100%; border-collapse: collapse; font-family: var(--mono); font-size: 0.78rem; }
table.preview td { padding: 0.15rem 0.5rem; border-bottom: 1px solid var(--border);
    vertical-align: top; word-break: break-all; }
table.preview td:first-child { color: var(--text-dim); width: 3rem; text-align: right; }

.plot-placeholder { margin-top: 1rem; padding: 1.5rem; text-align: center;
    color: var(--text-dim); border: 1px dashed var(--border); border-radius: var(--radius); }

.dropzone { position: fixed; inset: 0; display: none; align-items: center; justify-content: center;
    background: rgba(15,17,23,0.85); color: var(--accent); font-size: 1.2rem; z-index: 10;
    border: 3px dashed var(--accent); }
.dropzone.active { display: flex; }
```

- [ ] **Step 4: Build and smoke-test load**

Run:
```bash
source ~/Documents/prog/emsdk/emsdk_env.sh
ninja -C build_wasm
```
Expected: builds clean; `build_wasm/wacdfpp/` contains `cdfpp.js`, `wacdfpp.html` (app.js/cdf-model.js/render.js copy is wired in Task 6 — until then copy them manually for this smoke test: `cp wacdfpp/{app,cdf-model,render}.js build_wasm/wacdfpp/`).
Serve and open the page (`cd build_wasm/wacdfpp && python3 -m http.server`), load a `tests/resources/*.cdf`, and confirm: status reaches "Loaded", the sidebar shows grouped variables, clicking one populates the detail panel with attributes + a value table + the plot placeholder, and search filters the list.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/app.js wacdfpp/wacdfpp.html
git commit -m "wacdfpp: master/detail shell + app orchestration"
```

---

## Task 5: Drag-and-drop

**Files:**
- Modify: `wacdfpp/app.js`

- [ ] **Step 1: Add drag-and-drop wiring to `app.js`**

Before the final `await init();` line, add:

```js
function setDrop(active) { els.dropzone.classList.toggle("active", active); }

["dragenter", "dragover"].forEach(ev =>
    globalThis.addEventListener(ev, (e) => { e.preventDefault(); setDrop(true); }));
["dragleave", "drop"].forEach(ev =>
    globalThis.addEventListener(ev, (e) => {
        e.preventDefault();
        if (ev === "dragleave" && e.relatedTarget) return; // ignore inner leaves
        setDrop(false);
    }));
globalThis.addEventListener("drop", (e) => {
    const file = e.dataTransfer?.files?.[0];
    if (file) loadFile(file);
});
```

- [ ] **Step 2: Manual verify**

Run the dev-loop build + serve from Task 4 Step 4. Drag a `.cdf` file from the file manager onto the page.
Expected: a dashed "Drop a .cdf file" overlay appears on dragover and disappears on drop/leave; dropping loads and inspects the file exactly like the Inspect button.

- [ ] **Step 3: Commit**

```bash
git add wacdfpp/app.js
git commit -m "wacdfpp: drag-and-drop file loading"
```

---

## Task 6: Meson — ship the new JS files + full verification

**Files:**
- Modify: `wacdfpp/meson.build`

- [ ] **Step 1: Copy the new ES modules into the build dir**

In `wacdfpp/meson.build`, next to the existing `fs.copyfile('wacdfpp.html', 'wacdfpp.html')` (inside the emscripten block), add:

```meson
    fs.copyfile('app.js', 'app.js')
    fs.copyfile('cdf-model.js', 'cdf-model.js')
    fs.copyfile('render.js', 'render.js')
```

Also add the three JS files to the `wasm_exe` target's `extra_files` for IDE visibility:

```meson
        extra_files: ['wasm.txt', 'wacdfpp.html', 'app.js', 'cdf-model.js', 'render.js']
```

- [ ] **Step 2: Clean rebuild and confirm the modules are copied**

Run:
```bash
source ~/Documents/prog/emsdk/emsdk_env.sh
ninja -C build_wasm
ls build_wasm/wacdfpp/{app.js,cdf-model.js,render.js,cdfpp.js,wacdfpp.html}
```
Expected: all five files present (no manual `cp` needed now).

- [ ] **Step 3: Run the full WASM + pure test suite**

Run: `meson test -C build_wasm`
Expected: existing `wasm_time_conversion`, `wasm_attr_detach`, `wasm_save_roundtrip`, `wasm_time_attrs` PASS, plus new `wacdfpp_model`, `wacdfpp_format` PASS. Do **not** run `full_corpus` (network).

- [ ] **Step 4: End-to-end manual verification**

Serve `build_wasm/wacdfpp` and exercise a real multi-variable CDF (e.g. a THEMIS file from `tests/resources/` or `?url=`):
- Grouping: Data / Support / Metadata headers appear with correct counts; collapse/expand works; Global Attributes group present.
- Selection: clicking a variable fills the detail panel (title, compression, attributes, value table, plot placeholder); time variables show ISO dates; char variables show strings; selection highlight follows the click.
- Search: typing filters across variable names, attribute names, and attribute values; "No matches" shows when nothing matches; clearing restores the full list.
- Drag-and-drop and `?url=` both load correctly.
- Reload a second file: no stale state, previous `CdfFile` freed (no console "detached ArrayBuffer" errors).

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/meson.build
git commit -m "wacdfpp: ship app/cdf-model/render modules with the demo"
```

---

## Self-Review Notes

- **Spec coverage:** master/detail shell (Tasks 3–4) ✓; ES-module split app/cdf-model/render (Tasks 1–4) ✓; VAR_TYPE grouping (Task 1) ✓; live search across names/attr-names/attr-values (Tasks 1, 4) ✓; drag-and-drop (Task 5) ✓; preserve file/URL/`?url=` (Task 4) ✓; detail metadata + 20-record value preview + plot placeholder (Task 3) ✓; status/perf line (Task 4) ✓; no C++ changes ✓; out-of-scope (plot/export/edit) excluded ✓; existing WASM tests still run (Task 6) ✓.
- **Heap-safety:** attributes consumed as owned copies in `rawFromCdfFile`; detail values read immediately after a fresh `get_variable` via `copy_values`/`time_values_as_ns_since_1970`; previous `CdfFile` `.delete()`d before replacement (Task 4).
- **Type consistency:** `renderList(container, model, {selected, onSelect})`, `renderDetail(container, cdf, name)`, `buildModel`/`filterModel`/`rawFromCdfFile`, and `app.js` state names are used identically across tasks.
```
