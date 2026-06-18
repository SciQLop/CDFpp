// Hand off the loaded CDF to the canonical AstraLint web app for ISTP validation.
// Stage 1: deep-link a URL-sourced CDF via ?url=. astralintUrl() is pure (unit-tested);
// openValidation() does the window.open and is exercised in the browser.

export const ASTRALINT_URL = "https://sciqlop.github.io/AstraLint/";

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
