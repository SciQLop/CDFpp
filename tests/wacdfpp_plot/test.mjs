// Pure Node test for wacdfpp/plot-model.js and the pure helpers of spectrogram.js.
//   node test.mjs
import {
    MAX_LINES, recordLength, plotSpec, applyMask, decimateMinMax, toCSV, toJSON,
} from "../../wacdfpp/plot-model.js";
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

process.exit(failures ? 1 : 0);
