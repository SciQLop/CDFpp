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
