// Plot panel: reads a CdfFile variable, draws a uPlot line chart or a canvas
// spectrogram per plotSpec, and wires the override/scale toggles and CSV/JSON
// export. Imports uPlot (vendored) and the pure plot-model/spectrogram helpers.
// Reuses nsToISO from render.js for faithful time export (render.js has no import
// side effects, so this stays one-directional: render.js never imports plot.js).
import uPlot from "./uPlot.esm.js";
import { plotSpec, applyMask, dropMissingX, lineView, gapThreshold, spectroView, columnSpans, toCSV, toJSON } from "./plot-model.js";
import { bindGestures } from "./plot-gestures.js";
import { viridis, normalizeLevel, cellEdges, scaleTypeOf, isMonotonic } from "./spectrogram.js";
import { nsToISO, nsToSeconds } from "./render.js";

const LINE_COLORS = ["#6c8aff", "#4ade80", "#fbbf24", "#f87171", "#22d3ee", "#c084fc", "#fb923c", "#a3e635"];
const MAX_POINTS = 8000;   // line decimation cap
const MAX_COLS = 4000;     // spectrogram column cap
const PLOT_HEIGHT = 320;
// simplify: uPlot's time axis has no ticks under 1 ms; speasy-proxy goes to 10 µs by wrapping
// the resolved axis incrs in an init hook. Do the same if sub-ms windows are needed here.
const MIN_TIME_SPAN_S = 1e-3;
const MIN_INDEX_SPAN = 1;

// uPlot draws axis text/ticks/grid on the canvas (not via CSS), defaulting to
// black — invisible on the app's dark theme. Theme them to match.
const AXIS_STROKE = "#8b8fa3";                 // tick labels / axis label (var --text-dim)
const GRID_STROKE = "rgba(255, 255, 255, 0.08)";
const TICK_STROKE = "rgba(255, 255, 255, 0.18)";

function themeAxis(extra) {
    return {
        stroke: AXIS_STROKE,
        grid: { stroke: GRID_STROKE, width: 1 },
        ticks: { stroke: TICK_STROKE, width: 1 },
        ...extra,
    };
}

// --- value helpers -----------------------------------------------------------

function deinterleave(values, comps, recCount, c) {
    const col = new Float64Array(recCount);
    for (let i = 0; i < recCount; i++) col[i] = values[i * comps + c];
    return col;
}

// Keep every step-th element. step <= 1 returns a plain copy.
// Resolve the x-axis (uPlot's time axis works in Unix seconds). Priority:
//   1. DEPEND_0 epoch (CDF_EPOCH/EPOCH16/TT2000) -> ns since 1970.
//   2. THEMIS DEPEND_TIME -> a CDF_DOUBLE of Unix SECONDS since 1970. THEMIS files
//      keep an empty DEPEND_0 epoch placeholder and put the real times here.
//   3. DEPEND_0 as plain numeric values.
//   4. record index.
// Returns { values: number[], isTime, nsValues|null }.
function resolveXAxis(cdf, meta) {
    const attrs = meta.attributes ?? {};
    const nRec = meta.shape[0] ?? 0;
    const dep0 = attrs.DEPEND_0;

    if (dep0) {
        try {
            const ns = cdf.time_values_as_ns_since_1970(dep0);
            if (ns && ns.length) {
                const secs = new Array(ns.length);
                for (let i = 0; i < ns.length; i++) secs[i] = nsToSeconds(ns[i]);
                return { values: secs, isTime: true, nsValues: ns };
            }
        } catch { /* try DEPEND_TIME / index */ }
    }

    const depTime = attrs.DEPEND_TIME;
    if (depTime) {
        try {
            const tv = cdf.get_variable(depTime);
            if (tv && tv.copy_values && tv.copy_values.length) {
                const secs = Array.from(tv.copy_values, Number);   // Unix seconds
                const ns = new BigInt64Array(secs.length);          // ms precision for export
                for (let i = 0; i < secs.length; i++) ns[i] = BigInt(Math.round(secs[i] * 1e3)) * 1000000n;
                return { values: secs, isTime: true, nsValues: ns };
            }
        } catch { /* fall through */ }
    }

    if (dep0) {
        try {
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

    const kept = dropMissingX(xs, Array.from({ length: comps },
        (_, c) => applyMask(deinterleave(values, comps, recCount, c), spec)));
    const gap = gapThreshold(kept.x);
    const series = [{ label: x.isTime ? "time" : "record" },
        ...Array.from({ length: comps }, (_, c) =>
            ({ label: labels[c], stroke: LINE_COLORS[c % LINE_COLORS.length], width: 1, spanGaps: false }))];

    const opts = {
        width: target.clientWidth || 800,
        height: PLOT_HEIGHT,
        scales: { x: { time: x.isTime } },
        axes: [themeAxis(), themeAxis()],
        series,
        legend: { show: comps > 1 },
    };
    zoomableChart(opts, target, x.isTime, {
        viewFor: (lo, hi) => lineView(kept, lo, hi, MAX_POINTS, gap),
        autoY: (u, dmin, dmax) => uPlot.rangeNum(dmin, dmax, 0.1, true),
    });
}

// A uPlot chart with speasy-proxy's gestures (plot-gestures.js). viewFor(lo, hi) gives the
// chart data for an x range and is rebuilt after every x change, so zooming shows full
// resolution. autoY(u, dmin, dmax) is the Y range until the user sets one on the Y gutter.
// The data must keep the variable's first and last x: double-click resets x to the data.
function zoomableChart(opts, el, isTime, { viewFor, autoY }) {
    let shown = null, holdX = false, userY = null;
    // uPlot picks the visible indices before firing setScale, so the new view is swapped in
    // afterwards with a full setData, holding x (auto off) so the zoom range survives.
    const refresh = (u, force) => {
        const { min, max } = u.scales.x;
        if (!force && shown && shown[0] === min && shown[1] === max) return;
        shown = [min, max];
        holdX = true;
        u.setData(viewFor(min, max));
        holdX = false;
    };
    const u = new uPlot({
        ...opts,
        scales: {
            ...opts.scales,
            x: { ...opts.scales.x, auto: () => !holdX },
            y: { ...opts.scales.y, range: (self, dmin, dmax) => userY ? [userY.min, userY.max] : autoY(self, dmin, dmax) },
        },
        cursor: { drag: { x: false, y: false } },
        hooks: { ...opts.hooks, setScale: [(self, key) => { if (key === "x") queueMicrotask(() => refresh(self)); }] },
    }, viewFor(-Infinity, Infinity), el);
    bindGestures(u, {
        minSpan: isTime ? MIN_TIME_SPAN_S : MIN_INDEX_SPAN,
        getView: () => ({ start: u.scales.x.min, end: u.scales.x.max }),
        setView: ({ start, end }) => u.setScale("x", { min: start, max: end }),
        setY: (min, max) => { userY = { min, max }; u.setScale("y", userY); },
        resetY: () => { userY = null; refresh(u, true); },
    });
    return u;
}

// Resolve the y (bin) axis from DEPEND_1: its values, log/linear (SCALETYP), units.
// Falls back to a linear bin index when DEPEND_1 is absent/non-monotonic.
function resolveBins(cdf, spec, rows) {
    if (spec.depend1) {
        try {
            const dv = cdf.get_variable(spec.depend1);
            if (dv && dv.copy_values && dv.copy_values.length >= rows) {
                const centers = Array.from(dv.copy_values.slice(0, rows), Number);
                if (isMonotonic(centers)) {
                    const log = scaleTypeOf(dv.attributes, "linear") === "log" && centers.every(c => c > 0);
                    const units = typeof dv.attributes?.UNITS === "string" ? dv.attributes.UNITS : "";
                    return { centers, log, units };
                }
            }
        } catch { /* fall back to bin index */ }
    }
    return { centers: Array.from({ length: rows }, (_, i) => i), log: false, units: "bin" };
}

// Vertical viridis gradient + max/scale/min labels.
function colorbar(min, max, scale, units) {
    const el = document.createElement("div");
    el.className = "colorbar";
    const canvas = document.createElement("canvas");
    canvas.className = "colorbar-grad";
    canvas.width = 14;
    canvas.height = PLOT_HEIGHT;
    const ctx = canvas.getContext("2d");
    for (let y = 0; y < canvas.height; y++) {
        const [R, G, B] = viridis(1 - y / (canvas.height - 1)); // top = max
        ctx.fillStyle = `rgb(${R},${G},${B})`;
        ctx.fillRect(0, y, canvas.width, 1);
    }
    const labels = document.createElement("div");
    labels.className = "colorbar-labels";
    const fmt = (v) => Number.isFinite(v) ? v.toPrecision(3) : String(v);
    const top = document.createElement("span"); top.textContent = fmt(max);
    const mid = document.createElement("span"); mid.className = "cb-scale";
    mid.textContent = scale + (units ? " · " + units : "");
    const bot = document.createElement("span"); bot.textContent = fmt(min);
    labels.append(top, mid, bot);
    el.append(canvas, labels);
    return el;
}

function drawSpectro(target, cdf, meta, spec, x, values, scale) {
    const rows = Math.max(1, spec.components);
    const masked = applyMask(values, spec);

    const full = { centers: x.values, grid: masked, rows };
    const fullEdges = cellEdges(x.values, false);
    let shown = spectroView(full, -Infinity, Infinity, MAX_COLS);
    const grid = shown.grid;   // colour range from the full view, as the colour bar shows it

    let min = Infinity, max = -Infinity;
    for (let i = 0; i < grid.length; i++) {
        const v = grid[i];
        if (!Number.isFinite(v)) continue;
        if (scale === "log" && v <= 0) continue;
        if (v < min) min = v;
        if (v > max) max = v;
    }
    if (!Number.isFinite(min) || min === max) {
        target.appendChild(note("no finite values to display at this scale"));
        return;
    }

    const bins = resolveBins(cdf, spec, rows);
    const yEdges = cellEdges(bins.centers, bins.log);
    const yLo = Math.min(yEdges[0], yEdges[rows]);
    const yHi = Math.max(yEdges[0], yEdges[rows]);
    const units = typeof meta.attributes?.UNITS === "string" ? meta.attributes.UNITS : "";

    const wrap = document.createElement("div");
    wrap.className = "spectro-wrap";
    const plotEl = document.createElement("div");
    plotEl.className = "spectro-plot";
    wrap.append(plotEl, colorbar(min, max, scale, units));
    target.appendChild(wrap);

    // Paint one filled rect per (time-cell x bin-cell) of the shown view over the uPlot draw area.
    const paint = (u) => {
        const { ctx } = u;
        const spans = columnSpans(shown.centers);
        ctx.save();
        ctx.beginPath();
        ctx.rect(u.bbox.left, u.bbox.top, u.bbox.width, u.bbox.height);
        ctx.clip();
        for (let c = 0; c < spans.length; c++) {
            const xl = u.valToPos(spans[c][0], "x", true);
            const xr = u.valToPos(spans[c][1], "x", true);
            const left = Math.min(xl, xr), w = Math.abs(xr - xl) + 1;
            for (let r = 0; r < rows; r++) {
                const t = normalizeLevel(shown.grid[c * rows + r], min, max, scale);
                if (Number.isNaN(t)) continue;
                const yb = u.valToPos(yEdges[r], "y", true);
                const yt = u.valToPos(yEdges[r + 1], "y", true);
                const [R, G, B] = viridis(t);
                ctx.fillStyle = `rgb(${R},${G},${B})`;
                ctx.fillRect(left, Math.min(yt, yb), w, Math.abs(yb - yt) + 1);
            }
        }
        ctx.restore();
    };

    // The variable's outer cell edges stay in the x data (with no y) so the edge columns
    // render fully and double-click resets to the whole variable.
    const viewFor = (lo, hi) => {
        shown = spectroView(full, lo, hi, MAX_COLS);
        const xs = [fullEdges[0], ...shown.centers, fullEdges[fullEdges.length - 1]];
        return [xs, new Array(xs.length).fill(null)];
    };

    const opts = {
        width: (plotEl.clientWidth || 700),
        height: PLOT_HEIGHT,
        scales: { x: { time: x.isTime }, y: { distr: bins.log ? 3 : 1 } },
        axes: [themeAxis(), themeAxis(bins.units ? { label: bins.units } : undefined)],
        series: [{}, { paths: () => null, points: { show: false } }],
        legend: { show: false },
        hooks: { draw: [paint] },
    };
    zoomableChart(opts, plotEl, x.isTime, { viewFor, autoY: () => [yLo, yHi] });
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

    // Gate from metadata BEFORE materializing values: copy_values copies the whole
    // variable, so a high-rank/oversized distribution would OOM here. plotSpec (no
    // override) returns kind "none" for char, empty, >2-D and over-size variables.
    const gate = plotSpec(meta);
    if (gate.kind === "none") { mount.appendChild(note(gate.reason || "not plottable")); return; }

    const values = v.copy_values ? Float64Array.from(v.copy_values, Number) : new Float64Array(0);
    const x = resolveXAxis(cdf, meta);

    let override;          // undefined | "line" | "spectrogram"
    let scale = scaleTypeOf(meta.attributes, "log");   // spectrogram color scale (ISTP default)

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
