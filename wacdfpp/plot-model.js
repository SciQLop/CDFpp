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
