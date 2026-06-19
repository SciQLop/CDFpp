// Single shared WASM module instance for all views (explorer + compare).
import createCdfModule from "./cdfpp.js";

let modulePromise;
export function loadModule() {
    if (!modulePromise) modulePromise = createCdfModule();
    return modulePromise;
}
