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
