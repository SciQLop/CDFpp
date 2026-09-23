// Runs the converter's codec round trips off the main thread, in this worker's own WASM
// instance. Receives { bytes, keys }; posts { type: "run", run } per codec (converted bytes
// transferred, not copied), then { type: "done" }, or { type: "error", message }.
import { loadModule } from "./wasm.js";

function measure(Module, cdf, key) {
    const t0 = performance.now();
    const bytes = cdf.save_as(Module.CompressionType[key]);
    const t1 = performance.now();
    const reloaded = Module.load_eager(bytes);
    const t2 = performance.now();
    const identical = reloaded.is_valid() && reloaded.same_values(cdf);
    reloaded.delete();
    return { key, bytes, size: bytes.length, writeMs: t1 - t0, readMs: t2 - t1, identical };
}

self.onmessage = async ({ data: { bytes, keys } }) => {
    let cdf;
    try {
        const Module = await loadModule();
        // Eager: decoding the original up front keeps it out of the first codec's write time.
        cdf = Module.load_eager(bytes);
        if (!cdf.is_valid()) throw new Error("failed to parse CDF");
        for (const key of keys) {
            const run = measure(Module, cdf, key);
            self.postMessage({ type: "run", run }, [run.bytes.buffer]);
        }
        self.postMessage({ type: "done" });
    } catch (err) {
        self.postMessage({ type: "error", message: err?.message ?? String(err) });
    } finally {
        cdf?.delete();
    }
};
