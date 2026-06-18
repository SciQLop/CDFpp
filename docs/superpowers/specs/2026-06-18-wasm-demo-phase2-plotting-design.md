# WASM demo — Phase 2: Quick-look plotting — design

**Date:** 2026-06-18
**Status:** Approved
**Component:** `wacdfpp/` (browser demo; no C++/wrapper changes)
**Builds on:** Phase 1 (master/detail shell) — see
`2026-06-18-wasm-demo-app-design.md`. Branch `wasm-demo-app`.

## Goal

Turn the detail panel from "metadata + value table" into "I can see my data."
When a plottable variable is selected, draw it — line plot or spectrogram —
directly in the detail panel, ISTP-driven, entirely client-side.

This is the phase that changes the app's identity from *viewer* to *quick-look
tool* for scientists.

## Constraints

- **No build step.** Stay vanilla: static ES modules served as-is. The only new
  dependency is uPlot, **vendored** as a static file in `wacdfpp/vendor/`
  (~50 KB) — not a meson subproject, not a package manager.
- Honor KISS / small-units / locality-of-reasoning: keep all testable logic in a
  pure module; isolate DOM/canvas/uPlot in their own modules.
- **No `wacdfpp.cpp` / wrapper changes.** Everything needed is already exposed:
  `Variable.attributes` (FILLVAL/VALIDMIN/VALIDMAX/DEPEND_*/DISPLAY_TYPE/UNITS,
  with time-typed values decoded to ISO strings), `Variable.attribute_types`,
  `Variable.copy_values` (owned), and `CdfFile.time_values_as_ns_since_1970`.
- Respect the documented WASM heap-growth / detached-ArrayBuffer rule: read values
  via `copy_values` (owned) and time via `time_values_as_ns_since_1970`
  immediately on render, before any further allocation.

## Module architecture

Buildless ES modules, mirroring the Phase 1 split:

```
wacdfpp/
  vendor/
    uPlot.esm.js        # vendored uPlot (ESM build), ~50 KB
    uPlot.min.css       # uPlot stylesheet
  plot-model.js         # PURE, Node-tested — no DOM, no Emscripten
  spectrogram.js        # canvas heatmap renderer + viridis LUT + log/linear scaling
  plot.js               # plot panel: uPlot line plots, mounts spectrogram, controls, export
  render.js             # renderDetail reordered to plot-first; mounts the plot panel
```

Responsibilities:

- **`plot-model.js`** — pure data transformation, the natural unit to cover with a
  Node test (like `cdf-model.js`):
  - `plotSpec(variable, overrideKind?)` → a plain object describing what to draw:
    `{ kind: "line" | "spectrogram" | "none", components, fill, validMin, validMax,
    depend0, depend1, labelPtr1, reason }`. Decides kind from DISPLAY_TYPE / shape
    (see below). Operates only on the variable's own metadata
    (`type`, `shape`, `attributes`, `attribute_types`); sibling resolution and
    value fetching happen in `plot.js`.
  - `applyMask(values, { fill, validMin, validMax })` → `Float64Array` with masked
    samples set to `NaN`.
  - `decimateMinMax(x, y, targetCols)` → decimated `{ x, y }` preserving per-column
    min/max extremes for line plots.
  - `toCSV(columns)` / `toJSON(columns)` → string serialization of exported records.
- **`spectrogram.js`** — given a `<canvas>`, a 2D value grid, y-bins, and a scale
  (`log`/`linear`), draw a heatmap: build an offscreen `[cols × rows]` `ImageData`
  using a **viridis** lookup table, then `drawImage` it scaled to the display
  canvas. Masked (`NaN`) cells are transparent. Exposes the viridis colormap.
- **`plot.js`** — the plot panel (DOM + uPlot + canvas orchestration):
  - Resolve siblings via `cdf.get_variable(depend0/depend1/labelPtr1)`.
  - Determine the x-axis: if DEPEND_0 is a time variable, use
    `time_values_as_ns_since_1970` (→ milliseconds for uPlot's time axis);
    otherwise plot against record index `0..N−1`.
  - Build a uPlot chart for `kind: "line"` (one series per component); call
    `spectrogram.js` for `kind: "spectrogram"`; render the "not plottable" note for
    `kind: "none"`.
  - Render the controls: **line ⇄ spectrogram override toggle**,
    **log/linear scale toggle** (spectrogram), and **CSV / JSON export buttons**.
- **`render.js`** — `renderDetail` reordered to **plot-first**: title → plot panel
  (mounted where the Phase 1 placeholder was, now moved directly under the title) →
  Attributes → value preview table.

## Plot-type resolution (`plotSpec`)

Precedence:

1. **Manual override** (from the toggle) — if set, it wins.
2. **DISPLAY_TYPE present** — `time_series` → `line`, `spectrogram` → `spectrogram`.
3. **Inferred from record shape** (record length = product of non-record dims):
   - 1D record (scalar per record) → `line` (single series).
   - 2D record with secondary dim ≤ `MAX_LINES` (= 8) → `line` (multi-series; e.g.
     `[N, 3]` vector components).
   - 2D record with secondary dim > 8 → `spectrogram`.
4. **Not plottable** → `kind: "none"` with a `reason`: char/string types, or no
   numeric data, or 0 records.

The override toggle lets the user force line ⇄ spectrogram on files with
wrong/missing DISPLAY_TYPE.

## Axes, masking, big data

- **X-axis:** resolve DEPEND_0. If it names a time variable
  (CDF_EPOCH/EPOCH16/TT2000), use `time_values_as_ns_since_1970` → ms for uPlot's
  built-in time axis. If DEPEND_0 is absent or non-time, plot against record index.
- **Multi-line labels:** LABL_PTR_1 → the referenced char variable's per-component
  strings; else DEPEND_1 values; else `"comp N"`.
- **Spectrogram y-axis:** DEPEND_1 as 1D energy/frequency bins. If DEPEND_1 is
  time-varying (2D) or unresolved, fall back to bin index.
- **Color:** viridis, **log scale by default** (typical for flux), with a linear
  toggle. Single hard-coded colormap for Phase 2 (a colormap dropdown is deferred).
- **Masking (display only):** samples equal to FILLVAL, or outside
  `[VALIDMIN, VALIDMAX]`, become `NaN` → line gaps / transparent spectrogram cells.
  Auto-scaling ignores `NaN`.
- **Performance:**
  - Line plots: above a point cap, `decimateMinMax` to ~2× the canvas width,
    preserving per-column extremes so spikes survive.
  - Spectrogram: render at data resolution into an offscreen `ImageData`, scaled to
    the canvas; decimate columns when records exceed a cap.

## Export

CSV and JSON buttons on the plot panel emit the **selected variable's records**:
a time column (ISO-8601 from DEPEND_0, or record index when there is no time axis)
plus one value column per component. Export is **raw / faithful** — unmasked,
full-resolution (not decimated); masking and decimation are display concerns only.
Serialization (`toCSV` / `toJSON`) is pure and lives in `plot-model.js`; `plot.js`
wires the `Blob` download.

## Error handling

- Unresolvable DEPEND_0/DEPEND_1/LABL_PTR_1 → graceful fallback (record index / bin
  index / "comp N"), never a crash.
- A variable that is genuinely not plottable → the "not plottable" note; the rest of
  the detail panel (attributes, value table) renders as before.
- Re-fetch the variable and read `copy_values` / time immediately in the plot path,
  consistent with the heap-growth rule; never retain a zero-copy `values` view.
- On a failed parse, the existing Phase 1 behavior (clear model + list, error in
  status/detail) is unaffected.

## Testing

- **New pure Node test `tests/wacdfpp_plot/test.mjs`** covering `plotSpec`
  decisions (DISPLAY_TYPE, shape inference, override, not-plottable), `applyMask`
  (FILLVAL + range), `decimateMinMax` (extreme preservation), and `toCSV`/`toJSON`.
  Registered **only** in `tests/meson.build` (never in `wacdfpp/meson.build` —
  duplicate test names error when both `-Dwith_tests` and
  `-Dwith_experimental_wasm` are set).
- `wacdfpp/meson.build` copies the new JS modules and the vendored uPlot files into
  the build dir next to `cdfpp.js`.
- Existing WASM CI tests are unaffected (no C++ change). Never run `full_corpus`
  (network download).
- **Browser verification** via the dev loop
  (`source ~/Documents/prog/emsdk/emsdk_env.sh` → `ninja -C build_wasm` → serve over
  http; Playwright smoke) against real `tests/resources/*.cdf`: single line plot,
  multi-line vector, spectrogram, line⇄spectrogram override, log/linear toggle,
  FILLVAL/range masking gaps, CSV + JSON export, and a not-plottable variable.

## Out of scope (later / YAGNI)

- Colormap dropdown / per-plot color customization (Phase 3+).
- Overplotting multiple variables on one chart.
- Metadata editing, ISTP linter, re-save (Phase 3).
- npy export (CSV + JSON only).
