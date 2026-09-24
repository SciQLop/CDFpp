// Runs the converter's codec round trips off the main thread, in this worker's own WASM
// instance ("wasm32" or "wasm64" build). Receives { bytes, keys, build }; posts { type: "run", run } per codec (converted file as
// chunks, transferred, not copied), then { type: "done" }, or { type: "error", message }.
import { loadModule, loadModule64 } from "./wasm.js";

// Chrome refuses single ArrayBuffers of 2 GiB or more, so outputs travel as chunks.
const CHUNK_BYTES = 2 ** 30;

function measure(Module, cdf, key) {
    const t0 = performance.now();
    const chunks = cdf.save_as_chunks(Module.CompressionType[key], CHUNK_BYTES);
    const t1 = performance.now();
    const reloaded = Module.load_eager(chunks);
    const t2 = performance.now();
    const identical = reloaded.is_valid() && reloaded.same_values(cdf);
    reloaded.delete();
    const size = chunks.reduce((n, c) => n + c.length, 0);
    return { key, chunks, size, writeMs: t1 - t0, readMs: t2 - t1, identical };
}

// Readable message for an error, including a C++ exception that reached JS undecoded.
function describe(err, Module) {
    if (typeof WebAssembly.Exception === "function" && err instanceof WebAssembly.Exception) {
        try {
            const [type, message] = Module.getExceptionMessage(err);
            return `${type}: ${message}`;
        } catch {
            return "unexpected C++ exception (the WebAssembly memory may be exhausted)";
        }
    }
    return err?.message ?? String(err);
}

self.onmessage = async ({ data: { bytes, keys, build } }) => {
    let cdf, Module;
    try {
        Module = await (build === "wasm64" ? loadModule64() : loadModule());
        // Eager: decoding the original up front keeps it out of the first codec's write time.
        cdf = Module.load_eager(bytes);
        if (!cdf.is_valid()) throw new Error("failed to parse CDF");
        for (const key of keys) {
            const run = measure(Module, cdf, key);
            self.postMessage({ type: "run", run }, run.chunks.map((c) => c.buffer));
        }
        self.postMessage({ type: "done" });
    } catch (err) {
        self.postMessage({ type: "error", message: describe(err, Module) });
    } finally {
        cdf?.delete();
    }
};
