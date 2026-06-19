# Validate via AstraLint — Stage 2 (postMessage) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline). Steps use checkbox (`- [ ]`).

**Goal:** Validate **any** loaded CDF (local / drag-drop, not just URL-sourced) by opening the AstraLint web app and transferring the file bytes via `postMessage`. URL-sourced files keep using the Stage 1 deep-link.

**Architecture:** `wacdfpp/astralint.js` gains `ASTRALINT_ORIGIN` + `openValidationBytes(bytes, name, suite)` (open AstraLint, await `astralint-ready`, `postMessage` the file with the ArrayBuffer transferred). `app.js` retains the loaded bytes + name, enables Validate for any file, and routes URL→deep-link / local→postMessage. AstraLint (separate branch/PR off `main`): emit `astralint-ready` to `window.opener` after init, add a `message` listener that lints received bytes via a small `validateBytes()` extracted from `processFile`.

**Tech Stack:** `window.open` + `postMessage` (Transferable ArrayBuffer), vanilla ES modules; AstraLint Pyodide HTML/JS.

---

### Task 1: `astralint.js` — postMessage handoff

**Files:** Modify `wacdfpp/astralint.js`.

- [ ] **Step 1: Implement.** Append `ASTRALINT_ORIGIN` and `openValidationBytes`:
```javascript
export const ASTRALINT_ORIGIN = new URL(ASTRALINT_URL).origin;

// Stage 2: open AstraLint and transfer the CDF bytes once it signals readiness.
// Used for local/drag-drop files (no URL to deep-link). Returns false if the
// popup was blocked. `bytes` is a Uint8Array; its buffer is transferred.
export function openValidationBytes(bytes, name, suite = "ISTP") {
    const win = globalThis.open(ASTRALINT_URL, "_blank");
    if (!win) return false;
    const payload = bytes.slice();   // own copy so we can transfer without detaching caller's view
    const onMessage = (e) => {
        if (e.origin !== ASTRALINT_ORIGIN || e.source !== win) return;
        if (e.data && e.data.type === "astralint-ready") {
            cleanup();
            win.postMessage({ type: "astralint-file", name, suite, bytes: payload },
                ASTRALINT_ORIGIN, [payload.buffer]);
        }
    };
    const cleanup = () => { globalThis.removeEventListener("message", onMessage); clearTimeout(timer); };
    const timer = setTimeout(cleanup, 60000);   // give up waiting for ready after 60s
    globalThis.addEventListener("message", onMessage);
    return true;
}
```

- [ ] **Step 2: Syntax + existing tests.** `node --check wacdfpp/astralint.js` and `node tests/wacdfpp_astralint/test.mjs` (astralintUrl unchanged) → green. (No new pure unit test — `openValidationBytes` is DOM/window; covered by Playwright in Task 3.)

- [ ] **Step 3: Commit** — `git add wacdfpp/astralint.js && git commit -m "wacdfpp: astralint.js postMessage byte-transfer handoff (Stage 2)"`

---

### Task 2: `app.js` — retain bytes, enable for any file, route handoff

**Files:** Modify `wacdfpp/app.js`.

- [ ] **Step 1:** Import update: `import { openValidation, openValidationBytes } from "./astralint.js";`
- [ ] **Step 2:** Add state near `currentUrl`: `let currentBytes = null;` and `let currentName = null;`
- [ ] **Step 3:** `updateValidate()` — enable when any file is loaded:
```javascript
function updateValidate() {
    const ready = !!(currentUrl || currentBytes);
    els.validateBtn.disabled = !ready;
    els.validateBtn.title = ready
        ? "Validate this CDF against ISTP in AstraLint"
        : "Load a CDF to validate it against ISTP in AstraLint";
}
```
- [ ] **Step 4:** In `inspect(data, name, sourceUrl)`: on the failed-parse branch set `currentBytes = null; currentName = null;` (alongside `currentUrl = null`). On success set `currentBytes = data; currentName = name;` (alongside `currentUrl = sourceUrl ?? null`).
- [ ] **Step 5:** Replace the click handler:
```javascript
els.validateBtn.addEventListener("click", () => {
    if (currentUrl) openValidation(currentUrl);
    else if (currentBytes) openValidationBytes(currentBytes, currentName ?? "file.cdf");
});
```
- [ ] **Step 6:** Verify: `node --check wacdfpp/app.js`; `node tests/wacdfpp_astralint/test.mjs && node tests/wacdfpp_format/test.mjs` green.
- [ ] **Step 7: Commit** — `git add wacdfpp/app.js && git commit -m "wacdfpp: Validate works for local files via postMessage; URL still deep-links"`

---

### Task 3: Build + browser verification (wacdfpp, mock AstraLint receiver)

**Files:** none (build + Playwright with a stubbed AstraLint window).

- [ ] **Step 1:** `source …/emsdk_env.sh && ninja -C build_wasm`.
- [ ] **Step 2: Playwright.** Serve `build_wasm/wacdfpp`. Stub `window.open` (via `add_init_script`) to return a fake window that records `postMessage` calls into `window.__sent`. After loading a **local** fixture (file input), assert `#validateBtn` is **enabled**. Click it; then simulate AstraLint readiness by dispatching on `window` a `MessageEvent("message", { data:{type:"astralint-ready"}, origin:"https://sciqlop.github.io", source: <the fake win> })`. Assert the fake window received one `postMessage` with `data.type==="astralint-file"`, `data.name===<fixture name>`, `data.suite==="ISTP"`, and `data.bytes.length>0`. Also assert a **URL-loaded** file still routes to the deep-link (`window.open` called with `…/AstraLint/?url=…`, no file message). Zero console errors.
- [ ] **Step 3: Commit** any fixes.

---

### Task 4: AstraLint — emit ready + accept file via postMessage (separate branch/PR off main)

**Files:** Modify `docs/demo/index.html` in the AstraLint repo, on `feat/postmessage-file` off `origin/main`.

- [ ] **Step 1: Branch** — `git fetch origin main && git checkout -b feat/postmessage-file origin/main`.
- [ ] **Step 2: Extract `validateBytes`** — refactor `processFile(file)` so the pyodide call + render + group-header wiring live in a new `async function validateBytes(uint8Array, name, suiteName)` (suiteName optional → defaults to the checked radio). `processFile` reads the file then calls `validateBytes(new Uint8Array(await file.arrayBuffer()), file.name)`.
- [ ] **Step 3: Emit ready** — in `initPyodide`'s success path (next to `autoRunFromQuery()`), add: `if (window.opener) window.opener.postMessage({ type: "astralint-ready" }, "*");`
- [ ] **Step 4: Message listener** — register once (DOMContentLoaded): on `message` events with `e.data?.type === "astralint-file"`, ignore if `!pyodide`; select the suite radio by value if `e.data.suite` matches one; then `validateBytes(e.data.bytes instanceof Uint8Array ? e.data.bytes : new Uint8Array(e.data.bytes), e.data.name || "file.cdf", e.data.suite)`. (Accept from any origin — benign: it only lints bytes the sender supplies and shows them in this tab.)
- [ ] **Step 5: Manual verify** — locally serve `docs/demo`, open the wacdfpp demo (served build dir) pointing `ASTRALINT_URL` at the local AstraLint (temporary override) OR verify the handshake with the wacdfpp Playwright flow against a locally-served AstraLint: load a local CDF, click Validate, confirm the AstraLint tab auto-lints the transferred bytes. Confirm opening AstraLint directly (no opener) still works.
- [ ] **Step 6: Commit + push + PR** against `SciQLop/AstraLint:main`.

---

## Self-Review

- Spec coverage (Stage 2): postMessage byte transfer for any file (T1/T2), URL still deep-links (T2 routing), AstraLint ready + listener reusing existing validation (T4), graceful popup-block (`openValidationBytes` returns false) + ready timeout (T1). ✓
- Placeholder scan: T4 steps describe the AstraLint edits in prose (cross-repo, applied against the live `main` at implementation time) rather than pasting full context-dependent code — acceptable for an inline-executed plan by the author; behavior fully specified. ✓
- Type consistency: `openValidationBytes(bytes, name, suite="ISTP")`, `ASTRALINT_ORIGIN`, `currentBytes`/`currentName`, AstraLint `validateBytes(uint8Array, name, suiteName)` consistent across tasks. ✓
