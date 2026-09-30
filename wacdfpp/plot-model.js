// Pure data view for plotting. No DOM, no Emscripten. plotSpec decides what to
// draw from a variable's own metadata; sibling resolution + value fetching live
// in plot.js. Unit-tested in Node (tests/wacdfpp_plot).

export const MAX_LINES = 8;
// Only 1-D time series and 2-D spectrograms are drawable. Higher-rank variables
// (e.g. 4-5D particle distributions) have no sensible 2-D rendering and would
// crash when their whole value array is materialized — so they are gated out.
export const MAX_PLOT_DIMENSIONS = 2;     // max shape rank (record dim + 1)
export const MAX_PLOT_POINTS = 10_000_000; // max total elements (product of shape)
export const TIME_TYPES = new Set([31, 32, 33]); // CDF_EPOCH, EPOCH16, TT2000
export const CHAR_TYPES = new Set([51, 52]);     // CDF_CHAR, CDF_UCHAR

// Total element count = product of all dims (cheap; computed from shape, never
// from the materialized values).
export function totalElements(shape) {
    if (!shape || shape.length === 0) return 0;
    return shape.reduce((a, b) => a * b, 1);
}

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
    if (variable.shape.length > MAX_PLOT_DIMENSIONS)
        return { ...base, kind: "none",
            reason: `${variable.shape.length}-dimensional data is not plottable (only time series and spectrograms are supported)` };
    if (totalElements(variable.shape) > MAX_PLOT_POINTS)
        return { ...base, kind: "none",
            reason: `too large to plot (${totalElements(variable.shape).toLocaleString()} values)` };

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

// Min/max decimation for line plots: bucket into ~targetCols columns, emitting the
// per-bucket min and max (in index order) so spikes survive. NaN-only buckets emit
// a single NaN gap. Returns parallel { x, y } arrays. Pass-through when already small.
// Records without an x value (NaT times give NaN seconds) can't be placed on the axis:
// drop them from x and from every series.
export function dropMissingX(x, series) {
    const keep = [];
    for (let i = 0; i < x.length; i++) if (Number.isFinite(x[i])) keep.push(i);
    if (keep.length === x.length) return { x: Array.from(x), series: series.map((s) => Array.from(s)) };
    return { x: keep.map((i) => x[i]), series: series.map((s) => keep.map((i) => s[i])) };
}

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

const GAP_FACTOR = 10;
const GAP_SAMPLE_STEPS = 10_000;

// A step longer than GAP_FACTOR typical (median) steps is a data gap, e.g. between burst segments.
// simplify: the median is taken over ~GAP_SAMPLE_STEPS evenly spaced steps, not all of them; a
// variable where gaps outnumber regular steps would get a gap-sized median. Use every step then.
export function gapThreshold(x) {
    const n = x.length - 1;
    if (n < 1) return Infinity;
    const stride = Math.max(1, Math.floor(n / GAP_SAMPLE_STEPS));
    const steps = [];
    for (let i = 0; i < n; i += stride) steps.push(x[i + 1] - x[i]);
    steps.sort((a, b) => a - b);
    const median = steps[steps.length >> 1];
    return median > 0 ? GAP_FACTOR * median : Infinity;
}

// First index whose x is > v (or >= v when `inclusive`), by binary search on sorted x.
function bound(x, v, inclusive) {
    let lo = 0, hi = x.length;
    while (lo < hi) {
        const mid = (lo + hi) >> 1;
        if (inclusive ? x[mid] < v : x[mid] <= v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

// Indices [start, end) covering [lo, hi], plus one neighbour on each side so lines reach the edges.
function visibleRange(x, lo, hi) {
    return [Math.max(0, bound(x, lo, true) - 1), Math.min(x.length, bound(x, hi, false) + 1)];
}

function splitAtGaps(x, start, end, gap) {
    const segments = [];
    let s = start;
    for (let i = start + 1; i < end; i++) {
        if (x[i] - x[i - 1] > gap) { segments.push([s, i]); s = i; }
    }
    if (s < end) segments.push([s, end]);
    return segments;
}

function sampleByStep(arr, step) {
    const out = [];
    for (let i = 0; i < arr.length; i += step) out.push(arr[i]);
    return out;
}

// One segment as uPlot columns [x, ...series]: min/max for a single series (keeps spikes),
// a shared stride for several (they must share x).
function reduceSegment(full, s, e, budget) {
    const x = full.x.slice(s, e);
    const ys = full.series.map((y) => y.slice(s, e));
    if (ys.length === 1) {
        const r = decimateMinMax(x, ys[0], Math.max(1, Math.floor(budget / 2)));
        return [r.x, r.y];
    }
    const step = Math.max(1, Math.ceil(x.length / budget));
    return [sampleByStep(x, step), ...ys.map((y) => sampleByStep(y, step))];
}

// Concatenate segments with a null point between them, which uPlot draws as a break.
function joinWithBreaks(parts, width) {
    const out = Array.from({ length: width }, () => []);
    parts.forEach((cols, p) => {
        if (p > 0) {
            out[0].push((out[0].at(-1) + cols[0][0]) / 2);
            for (let c = 1; c < width; c++) out[c].push(null);
        }
        cols.forEach((col, c) => { for (const v of col) out[c].push(v); });
    });
    return out;
}

// uPlot's zoom reset autoscales x to the data it holds: keep the variable's first and last x
// (with no y) so a zoomed view still resets to the whole variable.
function withFullExtent(cols, full, start, end) {
    const n = full.x.length;
    if (start > 0) cols.forEach((col, c) => col.unshift(c === 0 ? full.x[0] : null));
    if (end < n) cols.forEach((col, c) => col.push(c === 0 ? full.x[n - 1] : null));
    return cols;
}

// uPlot data for the x range [lo, hi] of full = { x, series }: at most ~maxPoints points
// spread over the visible range, broken at gaps. Recomputed on zoom, so zooming in shows
// every sample again.
export function lineView(full, lo, hi, maxPoints, gap) {
    const [start, end] = visibleRange(full.x, lo, hi);
    const total = Math.max(1, end - start);
    const parts = splitAtGaps(full.x, start, end, gap).map(([s, e]) =>
        reduceSegment(full, s, e, Math.max(2, Math.floor(maxPoints * (e - s) / total))));
    return withFullExtent(joinWithBreaks(parts, full.series.length + 1), full, start, end);
}

// columns: [{ name, values: any[] }, ...] with equal-length value arrays.
function csvCell(v) {
    const s = v == null ? "" : String(v);
    return /[",\n\r]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
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
