# Validate via AstraLint handoff (Stage 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (inline). Steps use checkbox (`- [ ]`) syntax.

**Goal:** A "Validate (ISTP)" button in the wacdfpp demo that deep-links a URL-sourced CDF to the canonical AstraLint web app (`?url=`), plus the matching `?url=` auto-run support in the AstraLint repo.

**Architecture:** Pure `astralintUrl()` builder + `openValidation()` in a new `wacdfpp/astralint.js`; `app.js` tracks the file's source URL and wires the button; a header button in `wacdfpp.html`. Separate small change in the AstraLint repo to read `?url=`/`?suite=` on load. No C++ changes.

**Tech Stack:** Vanilla ES modules, `window.open`, Node pure test; AstraLint side is Pyodide HTML/JS.

---

### Task 1: `astralint.js` — pure URL builder + open handler (TDD)

**Files:** Create `wacdfpp/astralint.js`, `tests/wacdfpp_astralint/test.mjs`; modify `tests/meson.build`.

- [ ] **Step 1: Failing test** — create `tests/wacdfpp_astralint/test.mjs`:
```javascript
// Pure Node test for wacdfpp/astralint.js (URL builder only; window.open is DOM).
import { astralintUrl, ASTRALINT_URL } from "../../wacdfpp/astralint.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

const u = astralintUrl("https://example.com/a b/data.cdf?x=1");
check("base is AstraLint", u.startsWith(ASTRALINT_URL));
check("round-trips the cdf url", new URL(u).searchParams.get("url") === "https://example.com/a b/data.cdf?x=1");
check("encodes the space in the query string", u.includes("a%20b"));
check("default suite ISTP", new URL(u).searchParams.get("suite") === "ISTP");
check("explicit suite honored",
    new URL(astralintUrl("https://x/y.cdf", "PDS4")).searchParams.get("suite") === "PDS4");

process.exit(failures ? 1 : 0);
```

- [ ] **Step 2:** `node tests/wacdfpp_astralint/test.mjs` → FAIL (module missing).

- [ ] **Step 3: Implement** — create `wacdfpp/astralint.js`:
```javascript
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
```

- [ ] **Step 4:** `node tests/wacdfpp_astralint/test.mjs` → all `ok`.

- [ ] **Step 5: Register test** — in `tests/meson.build`, inside the `if node.found()` block (after the `wacdfpp_plot` test), add:
```meson
    test('wacdfpp_astralint', node,
        args: [files('wacdfpp_astralint/test.mjs')],
        timeout: 10,
    )
```

- [ ] **Step 6: Commit** — `git add wacdfpp/astralint.js tests/wacdfpp_astralint/test.mjs tests/meson.build && git commit -m "wacdfpp: astralint.js deep-link builder for ISTP validation handoff"`

---

### Task 2: Wire the Validate button into the demo

**Files:** Modify `wacdfpp/app.js`, `wacdfpp/wacdfpp.html`, `wacdfpp/meson.build`.

- [ ] **Step 1: HTML button** — in `wacdfpp/wacdfpp.html`, inside `<div id="output-header">` (which holds `#fileName` and `#parseTime`), add as the last child:
```html
        <button id="validateBtn" class="header-btn" disabled>Validate (ISTP)</button>
```
And add CSS near the other button rules:
```css
        #output-header { gap: 0.75rem; }
        .header-btn { font-family: var(--sans); font-size: 0.72rem; font-weight: 500;
            padding: 0.3em 0.8em; background: var(--surface2); color: var(--text);
            border: 1px solid var(--border); border-radius: 4px; cursor: pointer; }
        .header-btn:hover:not(:disabled) { background: var(--border); }
        .header-btn:disabled { opacity: 0.4; cursor: not-allowed; }
```

- [ ] **Step 2: app.js — track source URL + wire button.**
  - Add import after the render imports: `import { openValidation } from "./astralint.js";`
  - Add to the `els` object: `validateBtn: document.getElementById("validateBtn"),`
  - Add module state near `let selectedName`: `let currentUrl = null;`
  - Add a helper:
```javascript
function updateValidate() {
    els.validateBtn.disabled = !currentUrl;
    els.validateBtn.title = currentUrl
        ? "Validate this CDF against ISTP in AstraLint"
        : "Load from a URL to validate — local-file validation coming soon";
}
```
  - Change `inspect(data, name)` to `inspect(data, name, sourceUrl)`. In the failed-parse branch set `currentUrl = null;` then `updateValidate();` before returning. On success (end of function) set `currentUrl = sourceUrl ?? null;` then `updateValidate();`.
  - In `loadFile`'s reader.onload, call `inspect(new Uint8Array(e.target.result), file.name, null);`
  - In `fetchUrl(url)`, change the success call to `inspect(data, name, url);`
  - Wire the click near the other listeners: `els.validateBtn.addEventListener("click", () => openValidation(currentUrl));`
  - In `init()`, after WASM ready, call `updateValidate();` once so the disabled title is set.

- [ ] **Step 3: meson copy** — in `wacdfpp/meson.build` add `'astralint.js'` to `extra_files` and add `fs.copyfile('astralint.js', 'astralint.js')` after the other copyfiles.

- [ ] **Step 4: Verify** — `node tests/wacdfpp_astralint/test.mjs && node tests/wacdfpp_format/test.mjs` green; `grep -n validateBtn wacdfpp/app.js wacdfpp/wacdfpp.html` shows both wired.

- [ ] **Step 5: Commit** — `git add wacdfpp/app.js wacdfpp/wacdfpp.html wacdfpp/meson.build && git commit -m "wacdfpp: header Validate (ISTP) button -> AstraLint deep-link"`

---

### Task 3: AstraLint `?url=` auto-run (separate branch in AstraLint repo)

**Files:** Modify `/var/home/jeandet/Documents/prog/AstraLint/docs/demo/index.html` on a new branch.

- [ ] **Step 1: Branch** — in the AstraLint repo, create `git checkout -b feat/url-param-autorun`.
- [ ] **Step 2: Read** the demo's init flow (`initPyodide`, the URL-load function ~line 615, the DOM ids for the URL input + suite radios).
- [ ] **Step 3: Add `?url=`/`?suite=` handling** — after Pyodide + astralint finish initializing (end of `initPyodide`'s success path), read `new URLSearchParams(location.search)`. If `url` is set: assign it to the URL `<input>`, select the matching `suite` radio when `suite` is provided and valid, then invoke the existing URL-load/lint function. Guard so nothing happens when `url` is absent.
- [ ] **Step 4: Manual verify** — serve `docs/demo/` (or use the built site) and open `index.html?url=<a public .cdf>&suite=ISTP`; confirm it auto-loads and lints. Also confirm opening with no params behaves exactly as before.
- [ ] **Step 5: Commit + push branch + open PR** — commit on `feat/url-param-autorun`, push, `gh pr create` against AstraLint's default branch. (Per user prefs: feature branch, upstream SciQLop/AstraLint.)

---

### Task 4: Build + browser verification (wacdfpp)

**Files:** none.

- [ ] **Step 1:** `source /home/jeandet/Documents/prog/emsdk/emsdk_env.sh && ninja -C build_wasm` (recopies JS incl. astralint.js).
- [ ] **Step 2: Playwright** against the served build dir:
  - Stub `window.open` (record calls). Load a local fixture via the file input → assert `#validateBtn` is **disabled**.
  - Then load via the URL field (point it at the locally-served fixture URL, e.g. `http://localhost:PORT/<fixture>.cdf`) → assert `#validateBtn` becomes **enabled**; click it → assert `window.open` was called once with a URL that starts with `https://sciqlop.github.io/AstraLint/` and whose `url` param equals the fixture URL and `suite` is `ISTP`.
  - Assert zero console errors.
- [ ] **Step 3: Commit** any fixes.

---

## Self-Review

- Spec coverage: header Validate button (T2), source-URL tracking + enable/disable (T2), pure `astralintUrl` + `openValidation` (T1), deep-link `?url=&suite=ISTP` (T1), AstraLint `?url=` auto-run (T3), pure test registered only in tests/meson.build (T1), meson copy (T2), Playwright enable/disable + window.open assertion (T4), no C++ changes. ✓
- Placeholder scan: none; all code shown. ✓
- Type consistency: `astralintUrl(cdfUrl, suite="ISTP")`, `openValidation(cdfUrl, suite="ISTP")`, `inspect(data, name, sourceUrl)`, `currentUrl`, `updateValidate()` consistent across tasks. ✓
