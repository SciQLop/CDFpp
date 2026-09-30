// Pure Node test for wacdfpp/plot-model.js and the pure helpers of spectrogram.js.
//   node test.mjs
import {
    MAX_LINES, MAX_PLOT_DIMENSIONS, MAX_PLOT_POINTS,
    recordLength, plotSpec, applyMask, decimateMinMax, toCSV, toJSON, dropMissingX, lineView, gapThreshold, spectroView, columnSpans,
} from "../../wacdfpp/plot-model.js";
import { normalizeWheelDelta, wheelIntent, zoomToward, panRange, pinchRange, yRangeFromPixels }
    from "../../wacdfpp/plot-gestures.js";
import { viridis, normalizeLevel, cellEdges, scaleTypeOf, isMonotonic } from "../../wacdfpp/spectrogram.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// recordLength = product of non-record dims
check("recordLength 1D", recordLength([100]) === 1);
check("recordLength scalar", recordLength([]) === 1);
check("recordLength vector", recordLength([100, 3]) === 3);
check("recordLength 2D grid", recordLength([100, 8, 4]) === 32);

const fillAttr = new Float64Array([-1e31]);

// DISPLAY_TYPE wins
check("DISPLAY_TYPE time_series -> line", plotSpec(
    { type: 21, shape: [10, 32], attributes: { DISPLAY_TYPE: "time_series" } }).kind === "line");
check("DISPLAY_TYPE spectrogram -> spectrogram", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "spectrogram" } }).kind === "spectrogram");
check("DISPLAY_TYPE case-insensitive", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "Time_Series" } }).kind === "line");

// shape inference when DISPLAY_TYPE missing
check("infer 1D -> line", plotSpec({ type: 21, shape: [10], attributes: {} }).kind === "line");
check("infer small vector -> line", plotSpec({ type: 21, shape: [10, 3], attributes: {} }).kind === "line");
check("infer wide 2D -> spectrogram", plotSpec({ type: 21, shape: [10, 32], attributes: {} }).kind === "spectrogram");
check("infer boundary MAX_LINES -> line",
    plotSpec({ type: 21, shape: [10, MAX_LINES], attributes: {} }).kind === "line");
check("infer above MAX_LINES -> spectrogram",
    plotSpec({ type: 21, shape: [10, MAX_LINES + 1], attributes: {} }).kind === "spectrogram");

// override beats everything
check("override forces spectrogram", plotSpec(
    { type: 21, shape: [10, 3], attributes: { DISPLAY_TYPE: "time_series" } }, "spectrogram").kind === "spectrogram");
check("override forces line", plotSpec(
    { type: 21, shape: [10, 32], attributes: { DISPLAY_TYPE: "spectrogram" } }, "line").kind === "line");

// not plottable
check("char type -> none", plotSpec({ type: 51, shape: [10, 16], attributes: {} }).kind === "none");
check("zero records -> none", plotSpec({ type: 21, shape: [0, 3], attributes: {} }).kind === "none");
check("empty shape var still plottable as line",
    plotSpec({ type: 21, shape: [5], attributes: {} }).kind === "line");

// dimension + size gating: high-D (e.g. 4-5D particle distributions) and oversized
// variables would crash copy_values / produce meaningless plots — gate to none.
check("3-D var -> none", plotSpec({ type: 21, shape: [10, 16, 32], attributes: {} }).kind === "none");
check("5-D particle dist -> none",
    plotSpec({ type: 21, shape: [100, 32, 16, 32, 8], attributes: {} }).kind === "none");
check("high-D none carries a reason",
    typeof plotSpec({ type: 21, shape: [10, 16, 32], attributes: {} }).reason === "string");
check("high-D gate ignores override",
    plotSpec({ type: 21, shape: [10, 16, 32], attributes: {} }, "line").kind === "none");
check("oversized 2-D var -> none",
    plotSpec({ type: 21, shape: [MAX_PLOT_POINTS, 2], attributes: {} }).kind === "none");
check("oversized gate ignores override",
    plotSpec({ type: 21, shape: [MAX_PLOT_POINTS, 2], attributes: {} }, "spectrogram").kind === "none");
check("within rank + size still plottable",
    plotSpec({ type: 21, shape: [1000, 32], attributes: {} }).kind !== "none");
check("MAX_PLOT_DIMENSIONS is 2", MAX_PLOT_DIMENSIONS === 2);

// resolved fields surface DEPEND_*/LABL_PTR_1 and numeric FILLVAL
const spec = plotSpec({
    type: 21, shape: [10, 3],
    attributes: { DEPEND_0: "Epoch", DEPEND_1: "v_bins", LABL_PTR_1: "labels",
                  FILLVAL: fillAttr, VALIDMIN: new Float32Array([-100]), VALIDMAX: new Float32Array([100]) },
});
check("spec.depend0", spec.depend0 === "Epoch");
check("spec.depend1", spec.depend1 === "v_bins");
check("spec.labelPtr1", spec.labelPtr1 === "labels");
check("spec.components", spec.components === 3);
check("spec.fill from typed array", spec.fill === -1e31);
check("spec.validMin/Max", spec.validMin === -100 && spec.validMax === 100);

// 1000-point sawtooth; decimation must shrink it yet keep the global extremes.
const N = 1000;
const bigX = Array.from({ length: N }, (_, i) => i);
const bigY = Array.from({ length: N }, (_, i) => (i % 10) - 5); // min -5, max 4
const dec = decimateMinMax(bigX, bigY, 50);
check("decimate shrinks", dec.y.length <= 50 * 2 && dec.y.length < N);
check("decimate keeps global min", Math.min(...dec.y.filter(Number.isFinite)) === -5);
check("decimate keeps global max", Math.max(...dec.y.filter(Number.isFinite)) === 4);
check("decimate x aligned to y", dec.x.length === dec.y.length);

const small = decimateMinMax([0, 1, 2], [10, 20, 30], 50);
check("decimate passthrough when small", eq(small.y, [10, 20, 30]));

const cols = [
    { name: "time", values: ["2020-01-01T00:00:00Z", "2020-01-01T00:00:01Z"] },
    { name: "Bx", values: [1.5, 2.5] },
    { name: "lab,el", values: [10, 20] },
];
const csv = toCSV(cols);
check("toCSV header", csv.split("\n")[0] === 'time,Bx,"lab,el"');
check("toCSV first row", csv.split("\n")[1] === "2020-01-01T00:00:00Z,1.5,10");
check("toCSV trailing newline", csv.endsWith("\n"));

const json = JSON.parse(toJSON(cols));
check("toJSON keys", eq(Object.keys(json), ["time", "Bx", "lab,el"]));
check("toJSON values", eq(json.Bx, [1.5, 2.5]));

const masked = applyMask([1, -1e31, 5, 200, -200, NaN], { fill: -1e31, validMin: -100, validMax: 100 });
check("applyMask keeps in-range", masked[0] === 1 && masked[2] === 5);
check("applyMask drops fill", Number.isNaN(masked[1]));
check("applyMask drops above validMax", Number.isNaN(masked[3]));
check("applyMask drops below validMin", Number.isNaN(masked[4]));
check("applyMask drops non-finite", Number.isNaN(masked[5]));
check("applyMask returns Float64Array", masked instanceof Float64Array);

const noBounds = applyMask([1, 999], {});
check("applyMask no bounds keeps all", noBounds[0] === 1 && noBounds[1] === 999);

check("viridis low end is dark purple", eq(viridis(0), [68, 1, 84]));
check("viridis high end is yellow", eq(viridis(1), [253, 231, 37]));
const mid = viridis(0.5);
check("viridis mid is in range", mid.every(c => c >= 0 && c <= 255));
check("viridis clamps below 0", eq(viridis(-1), viridis(0)));
check("viridis clamps above 1", eq(viridis(2), viridis(1)));

check("normalizeLevel linear midpoint", normalizeLevel(50, 0, 100, "linear") === 0.5);
check("normalizeLevel log decade", Math.abs(normalizeLevel(10, 1, 100, "log") - 0.5) < 1e-9);
check("normalizeLevel NaN passthrough", Number.isNaN(normalizeLevel(NaN, 0, 100, "linear")));
check("normalizeLevel log rejects non-positive", Number.isNaN(normalizeLevel(0, 1, 100, "log")));

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

// Records without a time (NaT, so NaN seconds) can't sit on a time axis.
const kept = dropMissingX([1, NaN, 3], [[10, 20, 30], [4, 5, 6]]);
check("dropMissingX drops records without x", eq(kept, { x: [1, 3], series: [[10, 30], [4, 6]] }));
const all = dropMissingX([1, 2], [[5, 6]]);
check("dropMissingX keeps complete data", eq(all, { x: [1, 2], series: [[5, 6]] }));

// Zooming must show full resolution again: the view is reduced from the visible range only.
const ramp = { x: Array.from({ length: 10000 }, (_, i) => i), series: [Array.from({ length: 10000 }, (_, i) => i)] };
const zoomed = lineView(ramp, 100, 200, 1000, Infinity);
const inside = zoomed[0].filter((x) => x >= 100 && x <= 200);
check("lineView zoom shows every visible sample", inside.length === 101);
check("lineView keeps the full x extent for zoom reset",
    zoomed[0][0] === 0 && zoomed[0].at(-1) === 9999 && zoomed[1][0] === null && zoomed[1].at(-1) === null);
check("lineView x stays sorted", zoomed[0].every((x, i, a) => i === 0 || a[i - 1] <= x));
const wide = lineView(ramp, -Infinity, Infinity, 1000, Infinity);
check("lineView reduces the full view", wide[0].length <= 1000 + 2 && wide[1].length === wide[0].length);
check("lineView full view keeps the ends", wide[1].includes(0) && wide[1].includes(9999));

// A time gap breaks the line instead of drawing a straight segment over it.
const gappy = { x: [0, 1, 2, 3, 100, 101, 102], series: [[5, 6, 7, 8, 9, 10, 11]] };
check("gapThreshold is a multiple of the median step", gapThreshold(gappy.x) === 10);
check("gapThreshold without steps", gapThreshold([1]) === Infinity);
const broken = lineView(gappy, -Infinity, Infinity, 1000, gapThreshold(gappy.x));
const at = (x) => broken[0].indexOf(x);
check("lineView breaks the line at a gap", broken[1].slice(at(3) + 1, at(100)).includes(null));
check("lineView does not break inside a segment", !broken[1].slice(at(0), at(3) + 1).includes(null));

const multi = { x: ramp.x, series: [ramp.series[0], ramp.series[0].map((v) => -v)] };
const mv = lineView(multi, -Infinity, Infinity, 1000, Infinity);
check("lineView multi-series shares x", mv.length === 3 && mv[1].length === mv[0].length && mv[2].length === mv[0].length);
check("lineView multi-series reduces", mv[0].length <= 1000 + 2);

// Zooming a spectrogram shows every visible column again; thinning keeps the brightest cell.
const specCols = 10000, specRows = 2;
const specGrid = new Float64Array(specCols * specRows).map((_, i) => Math.floor(i / specRows));
specGrid[5000 * specRows + 1] = 1e6;
const spec10k = { centers: Array.from({ length: specCols }, (_, i) => i), grid: specGrid, rows: specRows };
const specZoom = spectroView(spec10k, 100, 200, 4000);
check("spectroView zoom shows every visible column", specZoom.centers.length === 103 && specZoom.grid.length === 103 * specRows);
check("spectroView zoom keeps the column values", specZoom.grid[0] === 99 && specZoom.centers[0] === 99);
const specWide = spectroView(spec10k, -Infinity, Infinity, 4000);
check("spectroView thins the full view", specWide.centers.length <= 4000 && specWide.grid.length === specWide.centers.length * specRows);
check("spectroView thinning keeps the brightest cell", Math.max(...specWide.grid) === 1e6);

const gapCenters = [...Array.from({ length: 1001 }, (_, i) => i), ...Array.from({ length: 1000 }, (_, i) => 5000 + i)];
const gapSpec = { centers: gapCenters, grid: new Float64Array(gapCenters.length), rows: 1 };
const thinnedGap = spectroView(gapSpec, -Infinity, Infinity, 7);
check("spectroView thins each side of a gap on its own",
    thinnedGap.centers.every((c) => c <= 1000 || c >= 5000));

// Spectrogram columns reach halfway to their neighbours, but not across a gap.
const spans = columnSpans([0, 1, 2, 3, 100, 101, 102]);
check("columnSpans halfway to neighbours", eq(spans[1], [0.5, 1.5]));
check("columnSpans stop at a gap", eq(spans[3], [2.5, 3.5]) && eq(spans[4], [99.5, 100.5]));
check("columnSpans outer columns mirror their inner half", eq(spans[0], [-0.5, 0.5]) && eq(spans[6], [101.5, 102.5]));
check("columnSpans single column", eq(columnSpans([5]), [[4.5, 5.5]]));

// Gesture math, ported with speasy-proxy's tests (tests/js/plot-core.test.js there).
const near = (a, b) => Math.abs(a - b) < 1e-6;
const wheel = (o) => ({ deltaX: 0, deltaY: 0, deltaMode: 0, shiftKey: false, ctrlKey: false, ...o });
check("wheel delta clamps big notches", normalizeWheelDelta(5000, 0) === 120 && normalizeWheelDelta(-5000, 0) === -120);
check("vertical wheel zooms", eq(wheelIntent(wheel({ deltaY: 40 })), { kind: "zoom", px: 40 }));
check("horizontal swipe pans", eq(wheelIntent(wheel({ deltaX: 30, deltaY: 4 })), { kind: "pan", px: 30 }));
check("mostly-vertical swipe zooms", eq(wheelIntent(wheel({ deltaX: 4, deltaY: -30 })), { kind: "zoom", px: -30 }));
check("Shift+wheel pans on Y (Firefox) or X (Chrome)",
    eq(wheelIntent(wheel({ deltaY: 3, deltaMode: 1, shiftKey: true })), { kind: "pan", px: 48 })
    && eq(wheelIntent(wheel({ deltaX: 3, deltaMode: 1, shiftKey: true })), { kind: "pan", px: 48 }));
check("Ctrl+wheel is a pinch", eq(wheelIntent(wheel({ deltaY: -5, ctrlKey: true })), { kind: "pinch", px: -5 }));
check("zoomToward zooms at the cursor", eq(zoomToward(0, 100, 0.5, -0.5, 1), { start: 25, end: 75 }));
const edgeZoom = zoomToward(0, 100, 0, -0.3, 1);
check("zoomToward keeps the value under the cursor", near(edgeZoom.start, 0) && near(edgeZoom.end, 70));
check("zoomToward refuses to go below the min span", zoomToward(0, 1, 0.5, -0.5, 1) === null);
check("zoomToward always zooms out", eq(zoomToward(0, 0.5, 0.5, 1, 1), { start: -0.25, end: 0.75 }));
check("panRange shifts by a fraction of the width",
    eq(panRange(0, 100, 0.25), { start: 25, end: 125 }) && eq(panRange(100, 200, -0.5), { start: 50, end: 150 }));
check("pinch spread zooms in", eq(pinchRange({ start: 0, end: 100 }, [0.25, 0.75], [0, 1], 1), { start: 25, end: 75 }));
const pinchPan = pinchRange({ start: 0, end: 100 }, [0.2, 0.6], [0.3, 0.7], 1);
check("pinch moving together pans", near(pinchPan.start, -10) && near(pinchPan.end, 90));
check("pinch crossing, meeting or too narrow is null",
    pinchRange({ start: 0, end: 100 }, [0.2, 0.6], [0.5, 0.5], 1) === null
    && pinchRange({ start: 0, end: 100 }, [0.2, 0.6], [0.6, 0.2], 1) === null
    && pinchRange({ start: 0, end: 2 }, [0.4, 0.6], [0, 1], 1) === null);
const lin = { min: 0, max: 100, log: false, heightPx: 200 };
check("yRangeFromPixels linear",
    eq(yRangeFromPixels(lin, 200, 0), { min: 0, max: 100 }) && eq(yRangeFromPixels(lin, 150, 50), { min: 25, max: 75 }));
const logPan = yRangeFromPixels({ min: 1, max: 1000, log: true, heightPx: 300 }, 200, -100);
check("yRangeFromPixels pans a log axis by decades", near(logPan.min, 10) && Math.abs(logPan.max - 10000) < 1e-3);
check("yRangeFromPixels degenerate is null", yRangeFromPixels(lin, 50, 50) === null && yRangeFromPixels(lin, 0, 200) === null);

process.exit(failures ? 1 : 0);
