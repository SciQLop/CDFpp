// Pure helpers for the spectrogram (no DOM, Node unit-tested): viridis colormap,
// normalizeLevel (log/linear), cellEdges (bin/time cell boundaries), scaleTypeOf
// (ISTP SCALETYP), isMonotonic. The heatmap itself is painted in plot.js via a
// uPlot draw hook using these helpers.

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
    const mid = log ? (a, b) => Math.sqrt(a * b) : (a, b) => (a + b) / 2;
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
