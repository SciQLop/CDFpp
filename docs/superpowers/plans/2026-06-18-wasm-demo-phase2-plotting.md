# WASM demo Phase 2 — Quick-look plotting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When a plottable variable is selected in the `wacdfpp/` demo, draw it as a line plot or spectrogram directly in the detail panel — ISTP-driven, masked, exportable — entirely client-side.

**Architecture:** Buildless ES modules mirroring Phase 1. All testable logic is pure in `plot-model.js` (Node-tested). `spectrogram.js` holds a canvas heatmap renderer plus pure colormap/scale helpers. `plot.js` is the DOM/uPlot orchestration: it reads the `CdfFile`, builds uPlot line charts or calls the spectrogram renderer, and wires the override/scale toggles and CSV/JSON export. `render.js` is reordered to plot-first and exposes an empty mount; `app.js` calls `renderPlot` into that mount on selection — keeping `render.js` free of the uPlot import so its existing pure Node test still passes.

**Tech Stack:** Vanilla ES modules, uPlot 1.6.31 (vendored static file, no build step), HTML `<canvas>` 2D, Emscripten WASM wrapper (unchanged), Meson/ninja, Node for pure tests.

**Spec deviation (intentional):** the design names a `wacdfpp/vendor/` directory for uPlot. Meson's `fs.copyfile` cannot write into a build subdirectory, and Phase 1 copies all demo assets flat into the build dir. So the two uPlot files live at `wacdfpp/` root (`uPlot.esm.js`, `uPlot.min.css`) and are imported with `./` paths — functionally identical, just flat.

---

### Task 1: Vendor uPlot (static asset)

**Files:**
- Create: `wacdfpp/uPlot.esm.js` (downloaded)
- Create: `wacdfpp/uPlot.min.css` (downloaded)

- [ ] **Step 1: Download the pinned uPlot ESM build + CSS**

Run:
```bash
cd wacdfpp
curl -fsSL https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.esm.js  -o uPlot.esm.js
curl -fsSL https://cdn.jsdelivr.net/npm/uplot@1.6.31/dist/uPlot.min.css -o uPlot.min.css
cd ..
```

- [ ] **Step 2: Verify the files are real (size + default export)**

Run:
```bash
ls -l wacdfpp/uPlot.esm.js wacdfpp/uPlot.min.css
node --input-type=module -e "import u from './wacdfpp/uPlot.esm.js'; console.log(typeof u)"
```
Expected: `uPlot.esm.js` ~40–55 KB, `uPlot.min.css` ~2–4 KB, and the node command prints `function`.

- [ ] **Step 3: Commit**

```bash
git add wacdfpp/uPlot.esm.js wacdfpp/uPlot.min.css
git commit -m "wacdfpp: vendor uPlot 1.6.31 (esm + css) for Phase 2 plotting"
```

---

### Task 2: `plot-model.js` — `plotSpec` + helpers

**Files:**
- Create: `wacdfpp/plot-model.js`
- Create: `tests/wacdfpp_plot/test.mjs`
- Modify: `tests/meson.build` (register the new node test, in the existing `if node.found()` block ending at line 69)

- [ ] **Step 1: Write the failing test**

Create `tests/wacdfpp_plot/test.mjs`:
```javascript
// Pure Node test for wacdfpp/plot-model.js and the pure helpers of spectrogram.js.
//   node test.mjs
import {
    MAX_LINES, recordLength, plotSpec,
} from "../../wacdfpp/plot-model.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// recordLength = product of non-record dims
check("recordLength 1D", recordLength([100]) === 1);
check("recordLength scalar", recordLength([]) === 1);
check("recordLength vector", recordLength([100, 3]) === 3);
check("recordLength 2D grid", recordLength([100, 8, 4]) === 32);

const fillAttr = new Float32Array([-1e31]);

// DISPLAY_TYPE wins
check("DISPLAY_TYPE time_series -> line", plotSpec(
    { type: 21, shape: [10, 32], attributes: { DISPLAY_TYPE: "time_series" } }).kind === "line");
check("DISPLAY_TYPE spectrogram -> spectrogram", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "spectrogram" } }).kind === "spectrogram");
check("DISPLAY_TYPE case-insensitive", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "Time_Series" } }).kind === "line");

// shape inference when DISPLAY_TYPE missing
check("infer 1D -> line", plotSpec({ type: 21, shape: [10], attributes: {} }).kind === "line");
check("infer small vector -> line", plotSpec({ type: 21, shape: [10, 3], attributes: {} }).kind === "line");
check("infer wide 2D -> spectrogram", plotSpec({ type: 21, shape: [10, 32], attributes: {} }).kind === "spectrogram");
check("infer boundary MAX_LINES -> line",
    plotSpec({ type: 21, shape: [10, MAX_LINES], attributes: {} }).kind === "line");
check("infer above MAX_LINES -> spectrogram",
    plotSpec({ type: 21, shape: [10, MAX_LINES + 1], attributes: {} }).kind === "spectrogram");

// override beats everything
check("override forces spectrogram", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "time_series" } }, "spectrogram").kind === "spectrogram");
check("override forces line", plotSpec(
    { type: 21, shape: [10, 32], attributes: { DISPLAY_TYPE: "spectrogram" } }, "line").kind === "line");

// not plottable
check("char type -> none", plotSpec({ type: 51, shape: [10, 16], attributes: {} }).kind === "none");
check("zero records -> none", plotSpec({ type: 21, shape: [0, 3], attributes: {} }).kind === "none");
check("empty shape var still plottable as line",
    plotSpec({ type: 21, shape: [5], attributes: {} }).kind === "line");

// resolved fields surface DEPEND_*/LABL_PTR_1 and numeric FILLVAL
const spec = plotSpec({
    type: 21, shape: [10, 3],
    attributes: { DEPEND_0: "Epoch", DEPEND_1: "v_bins", LABL_PTR_1: "labels",
                  FILLVAL: fillAttr, VALIDMIN: new Float32Array([-100]), VALIDMAX: new Float32Array([100]) },
});
check("spec.depend0", spec.depend0 === "Epoch");
check("spec.depend1", spec.depend1 === "v_bins");
check("spec.labelPtr1", spec.labelPtr1 === "labels");
check("spec.components", spec.components === 3);
check("spec.fill from typed array", spec.fill === -1e31);
check("spec.validMin/Max", spec.validMin === -100 && spec.validMax === 100);

process.exit(failures ? 1 : 0);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: FAIL — `Cannot find module '.../wacdfpp/plot-model.js'`.

- [ ] **Step 3: Write minimal implementation**

Create `wacdfpp/plot-model.js`:
```javascript
// Pure data view for plotting. No DOM, no Emscripten. plotSpec decides what to
// draw from a variable's own metadata; sibling resolution + value fetching live
// in plot.js. Unit-tested in Node (tests/wacdfpp_plot).

export const MAX_LINES = 8;
export const TIME_TYPES = new Set([31, 32, 33]); // CDF_EPOCH, EPOCH16, TT2000
export const CHAR_TYPES = new Set([51, 52]);     // CDF_CHAR, CDF_UCHAR

// Record length = product of non-record dims (shape[1:]); 1 for 0/1-D records.
export function recordLength(shape) {
    if (!shape || shape.length <= 1) return 1;
    return shape.slice(1).reduce((a, b) => a * b, 1);
}

// Coerce a CDF attribute value (number | typed array | array | string) to a number.
function numericAttr(v) {
    if (v == null) return undefined;
    if (typeof v === "number") return v;
    if (ArrayBuffer.isView(v) || Array.isArray(v)) return v.length ? Number(v[0]) : undefined;
    const n = Number(v);
    return Number.isNaN(n) ? undefined : n;
}

// Decide what to draw for `variable` ({ type, shape, attributes }).
// override: undefined | "line" | "spectrogram" (from the manual toggle).
export function plotSpec(variable, override) {
    const attrs = variable.attributes ?? {};
    const recLen = recordLength(variable.shape);
    const base = {
        depend0: attrs.DEPEND_0,
        depend1: attrs.DEPEND_1,
        labelPtr1: attrs.LABL_PTR_1,
        components: recLen,
        fill: numericAttr(attrs.FILLVAL),
        validMin: numericAttr(attrs.VALIDMIN),
        validMax: numericAttr(attrs.VALIDMAX),
    };

    if (CHAR_TYPES.has(variable.type))
        return { ...base, kind: "none", reason: "character data is not plottable" };
    if (!variable.shape || variable.shape.length === 0 || (variable.shape[0] ?? 0) === 0)
        return { ...base, kind: "none", reason: "no records to plot" };

    let kind;
    if (override === "line" || override === "spectrogram") {
        kind = override;
    } else {
        const dt = String(attrs.DISPLAY_TYPE ?? "").trim().toLowerCase();
        if (dt === "time_series") kind = "line";
        else if (dt === "spectrogram") kind = "spectrogram";
        else kind = recLen <= MAX_LINES ? "line" : "spectrogram";
    }
    return { ...base, kind };
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: all `ok`, exit 0.

- [ ] **Step 5: Register the test in Meson**

In `tests/meson.build`, inside the existing `if node.found()` block, after the `wacdfpp_format` test (line 68), add:
```meson
    test('wacdfpp_plot', node,
        args: [files('wacdfpp_plot/test.mjs')],
        timeout: 10,
    )
```

- [ ] **Step 6: Commit**

```bash
git add wacdfpp/plot-model.js tests/wacdfpp_plot/test.mjs tests/meson.build
git commit -m "wacdfpp: plot-model plotSpec (ISTP/shape plot-type resolution)"
```

---

### Task 3: `plot-model.js` — `applyMask`

**Files:**
- Modify: `wacdfpp/plot-model.js`
- Test: `tests/wacdfpp_plot/test.mjs`

- [ ] **Step 1: Write the failing test**

Append to `tests/wacdfpp_plot/test.mjs` (before the final `process.exit` line — add the import to the existing import block):

Change the import to:
```javascript
import {
    MAX_LINES, recordLength, plotSpec, applyMask,
} from "../../wacdfpp/plot-model.js";
```
Then add, just above `process.exit`:
```javascript
const masked = applyMask([1, -1e31, 5, 200, -200, NaN], { fill: -1e31, validMin: -100, validMax: 100 });
check("applyMask keeps in-range", masked[0] === 1 && masked[2] === 5);
check("applyMask drops fill", Number.isNaN(masked[1]));
check("applyMask drops above validMax", Number.isNaN(masked[3]));
check("applyMask drops below validMin", Number.isNaN(masked[4]));
check("applyMask drops non-finite", Number.isNaN(masked[5]));
check("applyMask returns Float64Array", masked instanceof Float64Array);

const noBounds = applyMask([1, 999], {});
check("applyMask no bounds keeps all", noBounds[0] === 1 && noBounds[1] === 999);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: FAIL — `applyMask is not a function`.

- [ ] **Step 3: Write minimal implementation**

Append to `wacdfpp/plot-model.js`:
```javascript
// Mask values to NaN: non-finite, == fill, or outside [validMin, validMax].
// Returns a fresh Float64Array (display use; never mutate the source view).
export function applyMask(values, { fill, validMin, validMax } = {}) {
    const out = new Float64Array(values.length);
    for (let i = 0; i < values.length; i++) {
        const x = Number(values[i]);
        if (!Number.isFinite(x)
            || (fill !== undefined && x === fill)
            || (validMin !== undefined && x < validMin)
            || (validMax !== undefined && x > validMax)) {
            out[i] = NaN;
        } else {
            out[i] = x;
        }
    }
    return out;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: all `ok`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/plot-model.js tests/wacdfpp_plot/test.mjs
git commit -m "wacdfpp: plot-model applyMask (FILLVAL + valid range -> NaN)"
```

---

### Task 4: `plot-model.js` — `decimateMinMax`

**Files:**
- Modify: `wacdfpp/plot-model.js`
- Test: `tests/wacdfpp_plot/test.mjs`

- [ ] **Step 1: Write the failing test**

Add `decimateMinMax` to the import block, then append before `process.exit`:
```javascript
// 1000-point sawtooth; decimation must shrink it yet keep the global extremes.
const N = 1000;
const bigX = Array.from({ length: N }, (_, i) => i);
const bigY = Array.from({ length: N }, (_, i) => (i % 10) - 5); // min -5, max 4
const dec = decimateMinMax(bigX, bigY, 50);
check("decimate shrinks", dec.y.length <= 50 * 2 && dec.y.length < N);
check("decimate keeps global min", Math.min(...dec.y.filter(Number.isFinite)) === -5);
check("decimate keeps global max", Math.max(...dec.y.filter(Number.isFinite)) === 4);
check("decimate x aligned to y", dec.x.length === dec.y.length);

const small = decimateMinMax([0, 1, 2], [10, 20, 30], 50);
check("decimate passthrough when small", eq(small.y, [10, 20, 30]));
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: FAIL — `decimateMinMax is not a function`.

- [ ] **Step 3: Write minimal implementation**

Append to `wacdfpp/plot-model.js`:
```javascript
// Min/max decimation for line plots: bucket into ~targetCols columns, emitting the
// per-bucket min and max (in index order) so spikes survive. NaN-only buckets emit
// a single NaN gap. Returns parallel { x, y } arrays. Pass-through when already small.
export function decimateMinMax(x, y, targetCols) {
    const n = y.length;
    if (targetCols <= 0 || n <= targetCols * 2) return { x: Array.from(x), y: Array.from(y) };
    const bucket = n / targetCols;
    const rx = [], ry = [];
    for (let c = 0; c < targetCols; c++) {
        const start = Math.floor(c * bucket);
        const end = Math.min(n, Math.floor((c + 1) * bucket));
        let minI = -1, maxI = -1, min = Infinity, max = -Infinity;
        for (let i = start; i < end; i++) {
            const v = y[i];
            if (Number.isNaN(v)) continue;
            if (v < min) { min = v; minI = i; }
            if (v > max) { max = v; maxI = i; }
        }
        if (minI === -1) { rx.push(x[start]); ry.push(NaN); continue; }
        const a = Math.min(minI, maxI), b = Math.max(minI, maxI);
        rx.push(x[a]); ry.push(y[a]);
        if (b !== a) { rx.push(x[b]); ry.push(y[b]); }
    }
    return { x: rx, y: ry };
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: all `ok`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/plot-model.js tests/wacdfpp_plot/test.mjs
git commit -m "wacdfpp: plot-model decimateMinMax (spike-preserving line decimation)"
```

---

### Task 5: `plot-model.js` — `toCSV` / `toJSON`

**Files:**
- Modify: `wacdfpp/plot-model.js`
- Test: `tests/wacdfpp_plot/test.mjs`

- [ ] **Step 1: Write the failing test**

Add `toCSV, toJSON` to the import block, then append before `process.exit`:
```javascript
const cols = [
    { name: "time", values: ["2020-01-01T00:00:00Z", "2020-01-01T00:00:01Z"] },
    { name: "Bx", values: [1.5, 2.5] },
    { name: "lab,el", values: [10, 20] },
];
const csv = toCSV(cols);
check("toCSV header", csv.split("\n")[0] === 'time,Bx,"lab,el"');
check("toCSV first row", csv.split("\n")[1] === "2020-01-01T00:00:00Z,1.5,10");
check("toCSV trailing newline", csv.endsWith("\n"));

const json = JSON.parse(toJSON(cols));
check("toJSON keys", eq(Object.keys(json), ["time", "Bx", "lab,el"]));
check("toJSON values", eq(json.Bx, [1.5, 2.5]));
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: FAIL — `toCSV is not a function`.

- [ ] **Step 3: Write minimal implementation**

Append to `wacdfpp/plot-model.js`:
```javascript
// columns: [{ name, values: any[] }, ...] with equal-length value arrays.
function csvCell(v) {
    const s = v == null ? "" : String(v);
    return /[",\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
}

export function toCSV(columns) {
    const n = columns.length ? columns[0].values.length : 0;
    const lines = [columns.map(c => csvCell(c.name)).join(",")];
    for (let i = 0; i < n; i++)
        lines.push(columns.map(c => csvCell(c.values[i])).join(","));
    return lines.join("\n") + "\n";
}

export function toJSON(columns) {
    return JSON.stringify(Object.fromEntries(columns.map(c => [c.name, Array.from(c.values)])));
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: all `ok`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/plot-model.js tests/wacdfpp_plot/test.mjs
git commit -m "wacdfpp: plot-model toCSV/toJSON serializers"
```

---

### Task 6: `spectrogram.js` — viridis + scale (pure) + canvas renderer

**Files:**
- Create: `wacdfpp/spectrogram.js`
- Test: `tests/wacdfpp_plot/test.mjs`

- [ ] **Step 1: Write the failing test**

At the top of `tests/wacdfpp_plot/test.mjs`, add a second import:
```javascript
import { viridis, normalizeLevel } from "../../wacdfpp/spectrogram.js";
```
Then append before `process.exit`:
```javascript
check("viridis low end is dark purple", eq(viridis(0), [68, 1, 84]));
check("viridis high end is yellow", eq(viridis(1), [253, 231, 37]));
const mid = viridis(0.5);
check("viridis mid is in range", mid.every(c => c >= 0 && c <= 255));
check("viridis clamps below 0", eq(viridis(-1), viridis(0)));
check("viridis clamps above 1", eq(viridis(2), viridis(1)));

check("normalizeLevel linear midpoint", normalizeLevel(50, 0, 100, "linear") === 0.5);
check("normalizeLevel log decade", Math.abs(normalizeLevel(10, 1, 100, "log") - 0.5) < 1e-9);
check("normalizeLevel NaN passthrough", Number.isNaN(normalizeLevel(NaN, 0, 100, "linear")));
check("normalizeLevel log rejects non-positive", Number.isNaN(normalizeLevel(0, 1, 100, "log")));
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: FAIL — `Cannot find module '.../wacdfpp/spectrogram.js'`.

- [ ] **Step 3: Write minimal implementation**

Create `wacdfpp/spectrogram.js`:
```javascript
// Canvas heatmap for spectrogram-type variables. viridis + normalizeLevel are pure
// (unit-tested); drawSpectrogram touches the DOM and is verified in the browser.

const VIRIDIS_ANCHORS = [
    [68, 1, 84], [72, 40, 120], [62, 74, 137], [49, 104, 142], [38, 130, 142],
    [31, 158, 137], [53, 183, 121], [110, 206, 88], [181, 222, 43], [253, 231, 37],
];

// Map t in [0,1] to an [r,g,b] viridis color (piecewise-linear over the anchors).
export function viridis(t) {
    const x = Math.min(1, Math.max(0, t)) * (VIRIDIS_ANCHORS.length - 1);
    const i = Math.floor(x), f = x - i;
    const a = VIRIDIS_ANCHORS[i];
    const b = VIRIDIS_ANCHORS[Math.min(i + 1, VIRIDIS_ANCHORS.length - 1)];
    return [
        Math.round(a[0] + (b[0] - a[0]) * f),
        Math.round(a[1] + (b[1] - a[1]) * f),
        Math.round(a[2] + (b[2] - a[2]) * f),
    ];
}

// Map a value to [0,1] given the color range and scale ("log" | "linear").
// NaN (masked) stays NaN; log requires positive value and bounds.
export function normalizeLevel(v, min, max, scale) {
    if (Number.isNaN(v)) return NaN;
    if (scale === "log") {
        if (v <= 0 || min <= 0 || max <= 0) return NaN;
        return (Math.log10(v) - Math.log10(min)) / (Math.log10(max) - Math.log10(min));
    }
    return (v - min) / (max - min);
}

// Draw a heatmap. grid is column-major: grid[col*rows + row], col = record (x),
// row = bin (y, row 0 drawn at the bottom). NaN cells are transparent.
export function drawSpectrogram(canvas, { grid, cols, rows, min, max, scale }) {
    const ctx = canvas.getContext("2d");
    const off = document.createElement("canvas");
    off.width = cols;
    off.height = rows;
    const octx = off.getContext("2d");
    const img = octx.createImageData(cols, rows);
    for (let c = 0; c < cols; c++) {
        for (let r = 0; r < rows; r++) {
            const t = normalizeLevel(grid[c * rows + r], min, max, scale);
            const px = ((rows - 1 - r) * cols + c) * 4; // flip y: bin 0 at bottom
            if (Number.isNaN(t)) { img.data[px + 3] = 0; continue; }
            const [R, G, B] = viridis(t);
            img.data[px] = R; img.data[px + 1] = G; img.data[px + 2] = B; img.data[px + 3] = 255;
        }
    }
    octx.putImageData(img, 0, 0);
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(off, 0, 0, cols, rows, 0, 0, canvas.width, canvas.height);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `node tests/wacdfpp_plot/test.mjs`
Expected: all `ok`, exit 0.

- [ ] **Step 5: Commit**

```bash
git add wacdfpp/spectrogram.js tests/wacdfpp_plot/test.mjs
git commit -m "wacdfpp: spectrogram canvas renderer + viridis/log-scale helpers"
```

---

### Task 7: `plot.js` — the plot panel (uPlot lines + spectrogram + controls + export)

**Files:**
- Create: `wacdfpp/plot.js`

No unit test (DOM/uPlot/canvas — verified in the browser in Task 9). This task is code-complete + a syntax check.

- [ ] **Step 1: Write the module**

Create `wacdfpp/plot.js`:
```javascript
// Plot panel: reads a CdfFile variable, draws a uPlot line chart or a canvas
// spectrogram per plotSpec, and wires the override/scale toggles and CSV/JSON
// export. Imports uPlot (vendored) and the pure plot-model/spectrogram helpers.
// Reuses nsToISO from render.js for faithful time export (render.js has no import
// side effects, so this stays one-directional: render.js never imports plot.js).
import uPlot from "./uPlot.esm.js";
import { plotSpec, applyMask, decimateMinMax, toCSV, toJSON, TIME_TYPES } from "./plot-model.js";
import { drawSpectrogram } from "./spectrogram.js";
import { nsToISO } from "./render.js";

const LINE_COLORS = ["#6c8aff", "#4ade80", "#fbbf24", "#f87171", "#22d3ee", "#c084fc", "#fb923c", "#a3e635"];
const MAX_POINTS = 8000;   // line decimation cap
const MAX_COLS = 4000;     // spectrogram column cap
const PLOT_HEIGHT = 320;

// --- value helpers -----------------------------------------------------------

function deinterleave(values, comps, recCount, c) {
    const col = new Float64Array(recCount);
    for (let i = 0; i < recCount; i++) col[i] = values[i * comps + c];
    return col;
}

function strideSample(x, y, target) {
    const n = y.length;
    if (n <= target) return { x: Array.from(x), y: Array.from(y) };
    const step = Math.ceil(n / target);
    const rx = [], ry = [];
    for (let i = 0; i < n; i += step) { rx.push(x[i]); ry.push(y[i]); }
    return { x: rx, y: ry };
}

// Resolve the x-axis: DEPEND_0 time -> ms (uPlot time axis); else DEPEND_0 numeric
// values; else record index. Returns { values: number[], isTime, nsValues|null }.
function resolveXAxis(cdf, meta) {
    const dep0 = meta.attributes?.DEPEND_0;
    const nRec = meta.shape[0] ?? 0;
    if (dep0) {
        try {
            const ns = cdf.time_values_as_ns_since_1970(dep0);
            if (ns && ns.length) {
                const ms = new Array(ns.length);
                for (let i = 0; i < ns.length; i++) ms[i] = Number(ns[i]) / 1e6;
                return { values: ms, isTime: true, nsValues: ns };
            }
            const dv = cdf.get_variable(dep0);
            if (dv && dv.copy_values && dv.copy_values.length)
                return { values: Array.from(dv.copy_values, Number), isTime: false, nsValues: null };
        } catch { /* fall through to index */ }
    }
    return { values: Array.from({ length: nRec }, (_, i) => i), isTime: false, nsValues: null };
}

function decodeStrings(bytes, count) {
    if (!bytes || !count) return null;
    const len = Math.floor(bytes.length / count);
    if (len <= 0) return null;
    const dec = new TextDecoder("utf-8", { fatal: false });
    const out = [];
    for (let i = 0; i < count; i++)
        out.push(dec.decode(bytes.subarray(i * len, (i + 1) * len)).replace(/\0+$/, "").trim());
    return out;
}

function lineLabels(cdf, spec) {
    if (spec.labelPtr1) {
        try {
            const lv = cdf.get_variable(spec.labelPtr1);
            const s = decodeStrings(lv?.copy_values, spec.components);
            if (s) return s;
        } catch { /* fall back */ }
    }
    if (spec.depend1) {
        try {
            const dv = cdf.get_variable(spec.depend1);
            if (dv?.copy_values && dv.copy_values.length >= spec.components)
                return Array.from(dv.copy_values.slice(0, spec.components), v => String(v));
        } catch { /* fall back */ }
    }
    return Array.from({ length: spec.components }, (_, c) => `comp ${c}`);
}

// --- DOM helpers -------------------------------------------------------------

function note(text) {
    const el = document.createElement("div");
    el.className = "plot-note";
    el.textContent = text;
    return el;
}

function button(label, onClick, active) {
    const b = document.createElement("button");
    b.className = "plot-btn" + (active ? " active" : "");
    b.textContent = label;
    b.addEventListener("click", onClick);
    return b;
}

function download(filename, text, mime) {
    const blob = new Blob([text], { type: mime });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url; a.download = filename;
    a.click();
    URL.revokeObjectURL(url);
}

// --- renderers ---------------------------------------------------------------

function drawLines(target, cdf, meta, spec, x, values) {
    const recCount = x.values.length;
    const comps = Math.max(1, spec.components);
    const labels = lineLabels(cdf, spec);
    const xs = x.values;

    const series = [{ label: x.isTime ? "time" : "record" }];
    const data = [xs];
    for (let c = 0; c < comps; c++) {
        const masked = applyMask(deinterleave(values, comps, recCount, c), spec);
        const reduced = comps === 1
            ? decimateMinMax(xs, masked, MAX_POINTS / 2)
            : strideSample(xs, masked, MAX_POINTS);
        if (c === 0) data[0] = reduced.x;          // shared x from the first series
        data.push(reduced.y);
        series.push({ label: labels[c], stroke: LINE_COLORS[c % LINE_COLORS.length], width: 1, spanGaps: false });
    }

    const opts = {
        width: target.clientWidth || 800,
        height: PLOT_HEIGHT,
        scales: { x: { time: x.isTime } },
        series,
        legend: { show: comps > 1 },
        cursor: { drag: { x: true, y: false } },
    };
    new uPlot(opts, data, target);
}

function drawSpectro(target, cdf, meta, spec, x, values, scale) {
    const cols = x.values.length;
    const rows = Math.max(1, spec.components);
    const masked = applyMask(values, spec);

    let min = Infinity, max = -Infinity;
    for (let i = 0; i < masked.length; i++) {
        const v = masked[i];
        if (!Number.isFinite(v)) continue;
        if (scale === "log" && v <= 0) continue;
        if (v < min) min = v;
        if (v > max) max = v;
    }
    if (!Number.isFinite(min) || min === max) {
        target.appendChild(note("no finite values to display at this scale"));
        return;
    }

    const canvas = document.createElement("canvas");
    canvas.className = "spectro";
    canvas.width = Math.min(cols, MAX_COLS) * 2;
    canvas.height = PLOT_HEIGHT;
    target.appendChild(canvas);
    drawSpectrogram(canvas, { grid: masked, cols, rows, min, max, scale });
}

// --- export ------------------------------------------------------------------

function exportColumns(cdf, meta, spec, x, values) {
    const recCount = x.values.length;
    const comps = Math.max(1, spec.components);
    const timeCol = x.isTime && x.nsValues
        ? { name: "time", values: Array.from(x.nsValues, nsToISO) }
        : { name: "record", values: x.values };
    const labels = lineLabels(cdf, spec);
    const cols = [timeCol];
    for (let c = 0; c < comps; c++)
        cols.push({ name: labels[c] || `comp${c}`, values: Array.from(deinterleave(values, comps, recCount, c)) });
    return cols;
}

// --- entry point -------------------------------------------------------------

// Render the plot panel for `name` into `mount`. Re-fetches the variable and reads
// owned copies immediately (heap-growth safety). Toggles re-render from cached data.
export function renderPlot(mount, cdf, name) {
    mount.innerHTML = "";
    const v = cdf.get_variable(name);
    if (!v) return;

    const meta = { type: v.type, shape: Array.from(v.shape), attributes: v.attributes };
    const values = v.copy_values ? Float64Array.from(v.copy_values, Number) : new Float64Array(0);
    const x = resolveXAxis(cdf, meta);

    let override;          // undefined | "line" | "spectrogram"
    let scale = "log";     // spectrogram color scale

    const controls = document.createElement("div");
    controls.className = "plot-controls";
    const body = document.createElement("div");
    body.className = "plot-body";
    mount.append(controls, body);

    function redraw() {
        const spec = plotSpec(meta, override);
        controls.innerHTML = "";
        body.innerHTML = "";

        if (spec.kind === "none") {
            body.appendChild(note(spec.reason || "not plottable"));
            return;
        }

        controls.append(
            button("Line", () => { override = "line"; redraw(); }, spec.kind === "line"),
            button("Spectrogram", () => { override = "spectrogram"; redraw(); }, spec.kind === "spectrogram"),
        );
        if (spec.kind === "spectrogram") {
            controls.append(
                button("log", () => { scale = "log"; redraw(); }, scale === "log"),
                button("linear", () => { scale = "linear"; redraw(); }, scale === "linear"),
            );
        }
        controls.append(
            button("CSV", () => download(`${name}.csv`, toCSV(exportColumns(cdf, meta, spec, x, values)), "text/csv")),
            button("JSON", () => download(`${name}.json`, toJSON(exportColumns(cdf, meta, spec, x, values)), "application/json")),
        );

        if (spec.kind === "line") drawLines(body, cdf, meta, spec, x, values);
        else drawSpectro(body, cdf, meta, spec, x, values, scale);
    }

    redraw();
}
```

- [ ] **Step 2: Syntax-check the module**

Run: `node --check wacdfpp/plot.js`
Expected: no output, exit 0. (A full functional check happens in Task 9.)

- [ ] **Step 3: Commit**

```bash
git add wacdfpp/plot.js
git commit -m "wacdfpp: plot.js panel (uPlot lines, spectrogram, toggles, export)"
```

---

### Task 8: Wire the plot panel into the detail view + build assets

**Files:**
- Modify: `wacdfpp/render.js` (reorder `renderDetail` to plot-first; replace placeholder with an empty mount)
- Modify: `wacdfpp/app.js` (call `renderPlot` into the mount on selection)
- Modify: `wacdfpp/wacdfpp.html` (uPlot CSS link + plot panel CSS)
- Modify: `wacdfpp/meson.build` (copy new modules + uPlot; extend `extra_files`)

- [ ] **Step 1: Reorder `renderDetail` and expose a mount**

In `wacdfpp/render.js`, replace the body of `renderDetail` from the `container.appendChild(head);` line onward. The new order is: title → empty `.plot-panel` mount → compression → attributes → value table. Remove the old `.plot-placeholder` block.

Replace this (current lines 205–229):
```javascript
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
```
with:
```javascript
    container.appendChild(head);

    // Plot-first: an empty mount right under the title. app.js fills it via plot.js
    // (kept out of render.js so render.js stays free of the uPlot import and its
    // pure Node test is unaffected).
    const plotPanel = document.createElement("div");
    plotPanel.className = "plot-panel";
    container.appendChild(plotPanel);

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
}
```

- [ ] **Step 2: Call `renderPlot` from `app.js`**

In `wacdfpp/app.js`, add the import after the `render.js` import (line 5):
```javascript
import { renderPlot } from "./plot.js";
```
Then change `selectVariable` (lines 38–42) to mount the plot after rendering the detail:
```javascript
function selectVariable(name) {
    selectedName = name;
    renderDetail(els.detail, currentCdf, name);
    const mount = els.detail.querySelector(".plot-panel");
    if (mount && currentCdf) renderPlot(mount, currentCdf, name);
    refreshList();   // update selection highlight
}
```

- [ ] **Step 3: Add the uPlot stylesheet link + plot CSS to the HTML**

In `wacdfpp/wacdfpp.html`, add the stylesheet link in `<head>` immediately after the `<title>` line (line 6):
```html
    <link rel="stylesheet" href="./uPlot.min.css">
```
Then replace the `.plot-placeholder` CSS rule (lines 273–274) with the plot panel styles:
```css
        .plot-panel { margin: 0.4rem 0 0.6rem; }
        .plot-controls { display: flex; flex-wrap: wrap; gap: 0.35rem; margin-bottom: 0.5rem; }
        .plot-btn { font-family: var(--mono); font-size: 0.72rem; padding: 0.25em 0.7em;
            background: var(--surface2); color: var(--text); border: 1px solid var(--border);
            border-radius: 4px; cursor: pointer; }
        .plot-btn:hover:not(:disabled) { opacity: 1; background: var(--border); }
        .plot-btn.active { background: var(--accent-dim); color: var(--accent); border-color: var(--accent); }
        .plot-body { min-height: 60px; }
        canvas.spectro { width: 100%; height: 320px; image-rendering: pixelated;
            border: 1px solid var(--border); border-radius: var(--radius); background: var(--bg); }
        .plot-note { padding: 1.25rem; text-align: center; color: var(--text-dim);
            border: 1px dashed var(--border); border-radius: var(--radius); }
        .uplot, .u-wrap { width: 100% !important; }
```

- [ ] **Step 4: Copy the new assets in the WASM build**

In `wacdfpp/meson.build`, extend the `extra_files` list (line 24) to include the new files:
```meson
        extra_files: ['wasm.txt', 'wacdfpp.html', 'app.js', 'cdf-model.js', 'render.js',
                      'plot-model.js', 'spectrogram.js', 'plot.js', 'uPlot.esm.js', 'uPlot.min.css']
```
Then add these `fs.copyfile` lines right after the existing `fs.copyfile('render.js', 'render.js')` (line 30):
```meson
    fs.copyfile('plot-model.js', 'plot-model.js')
    fs.copyfile('spectrogram.js', 'spectrogram.js')
    fs.copyfile('plot.js', 'plot.js')
    fs.copyfile('uPlot.esm.js', 'uPlot.esm.js')
    fs.copyfile('uPlot.min.css', 'uPlot.min.css')
```

- [ ] **Step 5: Verify pure tests still pass (render.js untouched by uPlot)**

Run: `node tests/wacdfpp_format/test.mjs && node tests/wacdfpp_plot/test.mjs`
Expected: both all-`ok`, exit 0.

- [ ] **Step 6: Commit**

```bash
git add wacdfpp/render.js wacdfpp/app.js wacdfpp/wacdfpp.html wacdfpp/meson.build
git commit -m "wacdfpp: mount plot panel plot-first; copy plot assets in wasm build"
```

---

### Task 9: Build + browser verification

**Files:** none (build + manual/Playwright verification)

- [ ] **Step 1: Build the WASM demo**

Run:
```bash
source /home/jeandet/Documents/prog/emsdk/emsdk_env.sh
meson setup build_wasm --cross-file wacdfpp/wasm.txt -Ddisable_python_wrapper=true -Dwith_experimental_wasm=true 2>/dev/null || true
ninja -C build_wasm
```
Expected: builds `build_wasm/wacdfpp/cdfpp.js` + `cdfpp.wasm`, and copies `plot-model.js`, `spectrogram.js`, `plot.js`, `uPlot.esm.js`, `uPlot.min.css` next to it.

- [ ] **Step 2: Confirm assets landed**

Run: `ls build_wasm/wacdfpp/{plot.js,plot-model.js,spectrogram.js,uPlot.esm.js,uPlot.min.css}`
Expected: all five listed, none missing.

- [ ] **Step 3: Serve and verify in the browser (webapp-testing skill / Playwright)**

Run a static server over the build dir (ES modules + WASM need http, not file://):
```bash
python3 -m http.server 8099 --directory build_wasm/wacdfpp
```
Then drive a headless browser (webapp-testing skill) to `http://localhost:8099/wacdfpp.html` and load a real multi-variable file (e.g. a THEMIS file under `tests/resources/`). Verify, with **zero console errors**:
- Selecting a 1D data variable with a time DEPEND_0 draws a single uPlot line against a time axis.
- A vector variable (`[N,3]`, e.g. a `*_gse` field) draws a multi-line plot with a legend and component labels.
- A wide 2D variable (e.g. an energy-flux spectrogram) draws a viridis canvas heatmap.
- The **Line/Spectrogram** toggle switches a variable's plot type; **log/linear** changes the spectrogram (visible only for spectrograms).
- FILLVAL / out-of-range samples appear as line gaps / transparent cells.
- **CSV** and **JSON** buttons download a file with a time/record column + one column per component.
- A char/string variable shows the "character data is not plottable" note, and the rest of the detail panel (attributes, value table) still renders.

- [ ] **Step 4: Run the meson test suite (excluding the network corpus test)**

Run: `meson test -C build_wasm --no-suite full_corpus`
Expected: `wacdfpp_plot`, `wacdfpp_model`, `wacdfpp_format`, and the `wasm_*` tests pass. (Never run `full_corpus` — it downloads from the internet.)

- [ ] **Step 5: Commit any fixes**

If browser verification surfaced fixes, commit them:
```bash
git add -A
git commit -m "wacdfpp: Phase 2 plotting fixes from browser verification"
```

---

## Self-Review

**Spec coverage:**
- uPlot line plots — Task 7 `drawLines`, verified Task 9. ✓
- Custom canvas spectrogram — Task 6 `drawSpectrogram` + Task 7 `drawSpectro`. ✓
- ISTP plot-type resolution + manual override — Task 2 `plotSpec` + Task 7 toggle. ✓
- DEPEND_0 time / record-index x-axis — Task 7 `resolveXAxis`. ✓
- Multi-line labels (LABL_PTR_1 / DEPEND_1 / comp N) — Task 7 `lineLabels`. ✓
- Spectrogram y = DEPEND_1, viridis, log default + linear toggle — Tasks 6/7. ✓
- FILLVAL/VALIDMIN/VALIDMAX masking — Task 3 `applyMask`, applied in Task 7. ✓
- Big-data decimation (line min/max; spectrogram column cap) — Task 4 + Task 7 (`MAX_POINTS`, `MAX_COLS`). ✓
- CSV/JSON export, raw/faithful, ISO time — Task 5 + Task 7 `exportColumns`. ✓
- Plot-first detail layout — Task 8. ✓
- Pure Node test registered only in `tests/meson.build`; assets copied in `wacdfpp/meson.build` — Tasks 2 & 8. ✓
- No C++/wrapper changes — confirmed (every task is JS/HTML/build). ✓

**Placeholder scan:** No TBD/TODO; every code step contains complete code; commands have expected output. ✓

**Type consistency:** `plotSpec` returns `{ kind, components, depend0, depend1, labelPtr1, fill, validMin, validMax, reason }` — consumed identically in `plot.js`. `applyMask(values, spec)` relies on `spec.fill/validMin/validMax` (present on every spec). `decimateMinMax`/`strideSample`/`drawSpectrogram` signatures match their call sites. `renderPlot(mount, cdf, name)` matches the `app.js` call and the `.plot-panel` mount created in `render.js`. `nsToISO` is imported from `render.js` where it is exported. ✓
