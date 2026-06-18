// Plot panel: reads a CdfFile variable, draws a uPlot line chart or a canvas
// spectrogram per plotSpec, and wires the override/scale toggles and CSV/JSON
// export. Imports uPlot (vendored) and the pure plot-model/spectrogram helpers.
// Reuses nsToISO from render.js for faithful time export (render.js has no import
// side effects, so this stays one-directional: render.js never imports plot.js).
import uPlot from "./uPlot.esm.js";
import { plotSpec, applyMask, decimateMinMax, toCSV, toJSON } from "./plot-model.js";
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

// Keep every step-th element. step <= 1 returns a plain copy.
function sampleByStep(arr, step) {
    if (step <= 1) return Array.from(arr);
    const out = [];
    for (let i = 0; i < arr.length; i += step) out.push(arr[i]);
    return out;
}

// Resolve the x-axis: DEPEND_0 time -> seconds (uPlot's time axis works in Unix
// seconds by default); else DEPEND_0 numeric values; else record index.
// Returns { values: number[], isTime, nsValues|null }.
function resolveXAxis(cdf, meta) {
    const dep0 = meta.attributes?.DEPEND_0;
    const nRec = meta.shape[0] ?? 0;
    if (dep0) {
        try {
            const ns = cdf.time_values_as_ns_since_1970(dep0);
            if (ns && ns.length) {
                const secs = new Array(ns.length);
                for (let i = 0; i < ns.length; i++) secs[i] = Number(ns[i]) / 1e9;
                return { values: secs, isTime: true, nsValues: ns };
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
    const data = [];
    if (comps === 1) {
        // Single series: min/max decimation preserves spikes against a shared x.
        const masked = applyMask(deinterleave(values, comps, recCount, 0), spec);
        const reduced = decimateMinMax(xs, masked, MAX_POINTS / 2);
        data.push(reduced.x, reduced.y);
        series.push({ label: labels[0], stroke: LINE_COLORS[0], width: 1, spanGaps: false });
    } else {
        // Multi series: one uniform stride shared by x and every component.
        const step = recCount > MAX_POINTS ? Math.ceil(recCount / MAX_POINTS) : 1;
        data.push(sampleByStep(xs, step));
        for (let c = 0; c < comps; c++) {
            const masked = applyMask(deinterleave(values, comps, recCount, c), spec);
            data.push(sampleByStep(masked, step));
            series.push({ label: labels[c], stroke: LINE_COLORS[c % LINE_COLORS.length], width: 1, spanGaps: false });
        }
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

// Block-max decimate a column-major grid (grid[col*rows+row]) down to `outCols`
// columns, taking the per-bin max over each block so bright features survive.
function decimateGridCols(grid, fullCols, rows, step, outCols) {
    const out = new Float64Array(outCols * rows).fill(NaN);
    for (let oc = 0; oc < outCols; oc++) {
        for (let sc = oc * step; sc < Math.min(fullCols, (oc + 1) * step); sc++) {
            for (let r = 0; r < rows; r++) {
                const v = grid[sc * rows + r];
                if (Number.isNaN(v)) continue;
                const cur = out[oc * rows + r];
                if (Number.isNaN(cur) || v > cur) out[oc * rows + r] = v;
            }
        }
    }
    return out;
}

function drawSpectro(target, cdf, meta, spec, x, values, scale) {
    const fullCols = x.values.length;
    const rows = Math.max(1, spec.components);
    const masked = applyMask(values, spec);

    // Cap the column count so the offscreen ImageData stays bounded on huge files.
    const step = fullCols > MAX_COLS ? Math.ceil(fullCols / MAX_COLS) : 1;
    const cols = step === 1 ? fullCols : Math.ceil(fullCols / step);
    const grid = step === 1 ? masked : decimateGridCols(masked, fullCols, rows, step, cols);

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

    const canvas = document.createElement("canvas");
    canvas.className = "spectro";
    canvas.width = cols * 2;
    canvas.height = PLOT_HEIGHT;
    target.appendChild(canvas);
    drawSpectrogram(canvas, { grid, cols, rows, min, max, scale });
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
