# Spectrogram axes + colorbar Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline). Steps use checkbox (`- [ ]`) syntax.

**Goal:** Render the wacdfpp spectrogram inside uPlot with a labeled time x-axis, a SCALETYP-driven bin y-axis, and a viridis colorbar.

**Architecture:** Add pure, Node-tested helpers to `spectrogram.js` (`cellEdges`, `scaleTypeOf`, `isMonotonic`). Rewrite `drawSpectro` in `plot.js` to build a uPlot heatmap (cells painted in a `draw` hook) plus a colorbar canvas. Line plots untouched. No C++ changes.

**Tech Stack:** uPlot (vendored), canvas 2D, vanilla ES modules, Node pure tests.

---

### Task 1: Pure helpers in `spectrogram.js` (TDD)

**Files:** Modify `wacdfpp/spectrogram.js`; extend `tests/wacdfpp_plot/test.mjs`.

- [ ] **Step 1: Failing tests.** Add to the `spectrogram.js` import line in `tests/wacdfpp_plot/test.mjs`: `cellEdges, scaleTypeOf, isMonotonic`. Insert before `process.exit`:
```javascript
check("isMonotonic asc", isMonotonic([1, 2, 3]) === true);
check("isMonotonic desc", isMonotonic([3, 2, 1]) === true);
check("isMonotonic flat-step not strict", isMonotonic([1, 1, 2]) === false);
check("isMonotonic unsorted", isMonotonic([1, 3, 2]) === false);

check("scaleTypeOf log", scaleTypeOf({ SCALETYP: "log" }, "linear") === "log");
check("scaleTypeOf LINEAR ci", scaleTypeOf({ SCALETYP: "LINEAR" }, "log") === "linear");
check("scaleTypeOf missing -> fallback", scaleTypeOf({}, "log") === "log");
check("scaleTypeOf junk -> fallback", scaleTypeOf({ SCALETYP: "weird" }, "linear") === "linear");

const le = cellEdges([1, 2, 3], false);
check("cellEdges linear length n+1", le.length === 4);
check("cellEdges linear interior midpoints", le[1] === 1.5 && le[2] === 2.5);
check("cellEdges linear extrapolated ends", le[0] === 0.5 && le[3] === 3.5);
const lg = cellEdges([1, 10, 100], true);
check("cellEdges log interior is geometric mean",
    Math.abs(lg[1] - Math.sqrt(10)) < 1e-9 && Math.abs(lg[2] - Math.sqrt(1000)) < 1e-9);
check("cellEdges single center linear", JSON.stringify(cellEdges([5], false)) === JSON.stringify([4.5, 5.5]));
```

- [ ] **Step 2:** `node tests/wacdfpp_plot/test.mjs` → FAIL (not functions).

- [ ] **Step 3: Implement.** Append to `wacdfpp/spectrogram.js`:
```javascript
// True if arr is strictly increasing or strictly decreasing.
export function isMonotonic(arr) {
    if (arr.length < 2) return true;
    let inc = true, dec = true;
    for (let i = 1; i < arr.length; i++) {
        if (!(arr[i] > arr[i - 1])) inc = false;
        if (!(arr[i] < arr[i - 1])) dec = false;
    }
    return inc || dec;
}

// ISTP SCALETYP attr -> "log" | "linear"; unknown/missing -> fallback.
export function scaleTypeOf(attrs, fallback) {
    const s = String(attrs?.SCALETYP ?? "").trim().toLowerCase();
    return s === "log" || s === "linear" ? s : fallback;
}

// N bin centers -> N+1 cell edges (midpoints; ends extrapolated by half a step).
// log=true computes midpoints/extension geometrically (in log space).
export function cellEdges(centers, log) {
    const n = centers.length;
    if (n === 0) return [];
    const mid = log
        ? (a, b) => Math.sqrt(a * b)
        : (a, b) => (a + b) / 2;
    const edges = new Array(n + 1);
    for (let i = 1; i < n; i++) edges[i] = mid(centers[i - 1], centers[i]);
    if (n === 1) {
        if (log) { edges[0] = centers[0] / Math.SQRT2; edges[1] = centers[0] * Math.SQRT2; }
        else { edges[0] = centers[0] - 0.5; edges[1] = centers[0] + 0.5; }
        return edges;
    }
    edges[0] = log ? centers[0] ** 2 / edges[1] : 2 * centers[0] - edges[1];
    edges[n] = log ? centers[n - 1] ** 2 / edges[n - 1] : 2 * centers[n - 1] - edges[n - 1];
    return edges;
}
```

- [ ] **Step 4:** `node tests/wacdfpp_plot/test.mjs` → all `ok`.

- [ ] **Step 5: Commit.** `git add wacdfpp/spectrogram.js tests/wacdfpp_plot/test.mjs && git commit -m "wacdfpp: pure helpers for spectrogram axes (cellEdges/scaleTypeOf/isMonotonic)"`

---

### Task 2: uPlot-hosted heatmap + colorbar in `plot.js`

**Files:** Modify `wacdfpp/plot.js`; remove the now-unused standalone `drawSpectrogram` from `wacdfpp/spectrogram.js`; add CSS in `wacdfpp/wacdfpp.html`.

- [ ] **Step 1: Rewrite `drawSpectro`** to build a uPlot heatmap + colorbar. Resolve DEPEND_1 (bin centers via `copy_values`, `SCALETYP`, `UNITS`); decide y-scale via `scaleTypeOf` + `isMonotonic` (fallback to bin index, linear); decide default color scale via the data var's `scaleTypeOf` (fallback "log") — note: the panel's `scale` state should initialize from this. Build decimated grid + x-centers (block-max over time when `fullCols > MAX_COLS`), compute color min/max over the decimated grid (positive-only for log), paint cells in a uPlot `draw` hook using `viridis`/`normalizeLevel` and `cellEdges`, and append a viridis colorbar canvas with min/max/scale/UNITS labels. Import `cellEdges, scaleTypeOf, isMonotonic, viridis, normalizeLevel` from `spectrogram.js`. (Full code written during implementation; see design doc for behavior.)

- [ ] **Step 2: Initialize the color scale from SCALETYP.** In `renderPlot`, replace `let scale = "log";` with logic that seeds from the data var's `SCALETYP` (`scaleTypeOf(meta.attributes, "log")`) so the default honors ISTP, still overridable by the toggle.

- [ ] **Step 3: Remove the obsolete `drawSpectrogram`** export from `spectrogram.js` (the standalone-canvas renderer) and confirm nothing imports it (`grep -rn drawSpectrogram wacdfpp`).

- [ ] **Step 4: CSS** in `wacdfpp.html`: add a `.spectro-wrap { display:flex; gap:0.5rem; align-items:stretch; }`, `.colorbar { ... }`, `.colorbar-labels { ... }`; the old `canvas.spectro` rule can stay (still used for the colorbar gradient or heatmap fallback) or be adjusted. Keep `.uplot,.u-wrap { width:100% !important; }` working for the heatmap.

- [ ] **Step 5: Syntax + pure tests.** `node --check wacdfpp/plot.js` and `node tests/wacdfpp_plot/test.mjs && node tests/wacdfpp_format/test.mjs` → all green. `grep -rn drawSpectrogram wacdfpp` → no references.

- [ ] **Step 6: Commit.** `git add wacdfpp/plot.js wacdfpp/spectrogram.js wacdfpp/wacdfpp.html && git commit -m "wacdfpp: render spectrogram in uPlot with axes + viridis colorbar"`

---

### Task 3: Build + browser verification

**Files:** none (build + Playwright verify).

- [ ] **Step 1:** `source /home/jeandet/Documents/prog/emsdk/emsdk_env.sh && ninja -C build_wasm` (incremental — recopies JS).
- [ ] **Step 2:** Serve `build_wasm/wacdfpp` over http; Playwright-verify on `ge_k0_cpi_19921231_v02.cdf`: select `SW_V`, toggle **Spectrogram** → heatmap now has a visible **x time axis** and **y axis**, plus a **colorbar** with min/max labels; **log/linear** still re-renders; switch back to **Line** works; select a line var → unaffected. Assert zero console errors. Also load `a_cdf.cdf` → `var2d_counter` inferred spectrogram renders with axes.
- [ ] **Step 3:** Visual screenshot check of the spectrogram (axes + colorbar present).
- [ ] **Step 4:** Commit any fixes.

---

## Self-Review

- Spec coverage: uPlot-hosted heatmap (T2), time x-axis (T2 resolveXAxis reuse), SCALETYP y-scale + bin-index fallback (T1 helpers + T2), data-var SCALETYP color default (T2 step 2), colorbar (T2), cellEdges/scaleTypeOf/isMonotonic pure + tested (T1), line plots untouched, no C++ changes. ✓
- Placeholder scan: Task 2 step 1 defers full code to implementation time — acceptable for an inline-executed plan by the author who holds the design; the behavior is fully specified in the design doc. ✓
- Type consistency: helpers' signatures (`cellEdges(centers, log)`, `scaleTypeOf(attrs, fallback)`, `isMonotonic(arr)`) match their test calls and intended plot.js usage. ✓
