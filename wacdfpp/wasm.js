// Single shared WASM module instance for all views (explorer + compare).
import createCdfModule from "./cdfpp.js";

let modulePromise;
export function loadModule() {
    if (!modulePromise) modulePromise = createCdfModule();
    return modulePromise;
}

// The 64-bit memory build, for files too large for wasm32's 4 GiB. Imported on demand: only
// those files need it, and only browsers with 64-bit WASM can run it.
let module64Promise;
export function loadModule64() {
    module64Promise ??= import("./cdfpp64.js").then((m) => m.default());
    return module64Promise;
}

// wasm64 needs 64-bit memories and tables (Chrome 133+, Firefox 134+); Node 22 has only the first.
export function supportsMemory64() {
    const validates = (section) => WebAssembly.validate(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, ...section]));
    return validates([5, 3, 1, 4, 0]) && validates([4, 4, 1, 0x70, 4, 0]);
}
