// Hand off the loaded CDF to the canonical AstraLint web app for ISTP validation.
// Stage 1: deep-link a URL-sourced CDF via ?url= (astralintUrl is pure, unit-tested).
// Stage 2: transfer raw bytes to AstraLint via postMessage for local/drag-drop files.
// The window/postMessage helpers are exercised in the browser, not unit tests.

export const ASTRALINT_URL = "https://sciqlop.github.io/AstraLint/";
export const ASTRALINT_ORIGIN = new URL(ASTRALINT_URL).origin;

// Build the AstraLint deep-link for a CDF reachable at cdfUrl. suite defaults to ISTP.
export function astralintUrl(cdfUrl, suite = "ISTP") {
    const u = new URL(ASTRALINT_URL);
    u.searchParams.set("url", cdfUrl);
    u.searchParams.set("suite", suite);
    return u.toString();
}

// Open AstraLint in a new tab for the given CDF source URL. No-op (returns false)
// when cdfUrl is falsy (e.g. a local/drag-drop file with no URL).
export function openValidation(cdfUrl, suite = "ISTP") {
    if (!cdfUrl) return false;
    globalThis.open(astralintUrl(cdfUrl, suite), "_blank", "noopener");
    return true;
}

// Stage 2: open AstraLint and transfer the CDF bytes once it signals readiness.
// Used for local/drag-drop files (no URL to deep-link). `bytes` is a Uint8Array;
// a copy's buffer is transferred so the caller's view is not detached. Returns
// false if the popup was blocked.
export function openValidationBytes(bytes, name, suite = "ISTP") {
    const win = globalThis.open(ASTRALINT_URL, "_blank");
    if (!win) return false;
    const payload = bytes.slice();
    const cleanup = () => { globalThis.removeEventListener("message", onMessage); clearTimeout(timer); };
    const onMessage = (e) => {
        if (e.origin !== ASTRALINT_ORIGIN || e.source !== win) return;
        if (e.data && e.data.type === "astralint-ready") {
            cleanup();
            win.postMessage({ type: "astralint-file", name, suite, bytes: payload },
                ASTRALINT_ORIGIN, [payload.buffer]);
        }
    };
    const timer = setTimeout(cleanup, 60000);   // stop waiting for ready after 60s
    globalThis.addEventListener("message", onMessage);
    return true;
}
