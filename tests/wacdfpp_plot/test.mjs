// Pure Node test for wacdfpp/plot-model.js and the pure helpers of spectrogram.js.
//   node test.mjs
import {
    MAX_LINES, recordLength, plotSpec,
} from "../../wacdfpp/plot-model.js";

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

process.exit(failures ? 1 : 0);
