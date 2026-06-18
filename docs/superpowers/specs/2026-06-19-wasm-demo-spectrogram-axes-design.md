# WASM demo — spectrogram axes + colorbar — design

**Date:** 2026-06-19
**Status:** Approved
**Component:** `wacdfpp/` (browser demo; no C++/wrapper changes)
**Builds on:** Phase 2 plotting (`2026-06-18-wasm-demo-phase2-plotting-design.md`). Branch `wasm-demo-app`.

## Goal

Make the quick-look spectrogram readable: give it a labeled time x-axis, a
bin (energy/frequency) y-axis, and a viridis colorbar. Today it is a bare
`<canvas>` heatmap with no axes or scale legend.

## Approach

**Host the heatmap inside uPlot** (the same library the line plots use) rather
than a standalone canvas. uPlot owns the axes, ticks, zoom/pan and cursor; we
paint the cells in a `draw` hook. This makes the spectrogram visually consistent
with the line plots and removes hand-rolled tick math.

### Rendering

- A uPlot instance with:
  - **x scale**: time in seconds (`resolveXAxis`, `time: true`) when DEPEND_0 is a
    time variable, else record index (linear).
  - **y scale**: DEPEND_1 bin values, with explicit `range` from the bin edges;
    `distr: 3` (log) or `distr: 1` (linear) per `SCALETYP` (below).
  - one dummy y series (`paths: () => null`, points hidden) so uPlot draws axes;
    data y-column is nulls (the real y-range comes from the explicit `range`).
  - a `hooks.draw` that paints one filled rect per (time-cell × bin-cell):
    color = `viridis(normalizeLevel(value, cmin, cmax, colorScale))`; masked/NaN
    cells skipped (transparent). Rect pixel bounds via `u.valToPos(v, axis, true)`,
    clipped to `u.bbox`.
- Cell edges = midpoints between adjacent centers, outer edges extrapolated;
  computed in log space when the axis is log (`cellEdges(centers, log)`).
- Time-column block-max decimation (existing `decimateGridCols`) still bounds the
  painted cell count to `MAX_COLS`; decimated x-centers are the block midpoints.

### Scales from ISTP `SCALETYP`

- **Y (bins):** the DEPEND_1 variable's `SCALETYP` — `"log"` → log axis, `"linear"`
  → linear, anything else → fallback. Fall back to **bin index (linear)** when
  DEPEND_1 is unresolved, non-monotonic, or absent.
- **Color (z):** default the log/linear choice from the **data variable's**
  `SCALETYP` (fallback `log`), still overridable by the existing log/linear toggle.

### Colorbar

A thin vertical viridis gradient (small canvas, ~14 px wide, plot height) to the
right of the uPlot, with **max** label at top, **min** at bottom, and the scale
(`log`/`linear`) + the variable's `UNITS` if present. Laid out in a flex row:
`[ uPlot ] [ colorbar ]`.

## Module layout & tests

- `spectrogram.js` keeps the pure `viridis` / `normalizeLevel`. Add pure,
  Node-tested helpers:
  - `cellEdges(centers, log)` → `N+1` edge array (midpoints + extrapolated ends;
    log-space when `log`).
  - `scaleTypeOf(attrs, fallback)` → `"log" | "linear"` from `SCALETYP`.
  - `isMonotonic(arr)` → bool (helper for the bin fallback decision).
  The uPlot heatmap-draw builder and colorbar DOM live in `plot.js` (the uPlot
  orchestrator). The old standalone-canvas `drawSpectrogram` is removed (replaced).
- `plot.js` `drawSpectro` rewritten to build the uPlot heatmap + colorbar; resolves
  the DEPEND_1 variable (bin centers + its `SCALETYP`/`UNITS`) and the data var's
  `SCALETYP`. Line plots (`drawLines`) unchanged.
- Tests: extend `tests/wacdfpp_plot/test.mjs` with `cellEdges`, `scaleTypeOf`,
  `isMonotonic`. Browser-verify (Playwright): spectrogram shows a labeled time
  x-axis, bin y-axis (log when SCALETYP=log), and colorbar; log/linear + override
  toggles still work; line plots unaffected; zero console errors.

## Out of scope

- Per-cell cursor readout / tooltip values on the heatmap.
- Colorbar tick labels beyond min/max (no intermediate ticks).
- Non-uniform colormaps / colormap selection (still viridis only).
