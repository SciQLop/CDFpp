# WASM demo — Validate via AstraLint handoff — design

**Date:** 2026-06-19
**Status:** Approved
**Component:** `wacdfpp/` (browser demo) + a small change in the **AstraLint** repo
**Builds on:** Phase 2 plotting. Branch `wasm-demo-app` (CDFpp); separate branch in AstraLint.

## Goal

Let a user validate the loaded CDF against ISTP from the wacdfpp demo by handing
it off to the **existing canonical AstraLint web app**
(`https://sciqlop.github.io/AstraLint/`, a Pyodide build of the real linter) —
instead of reimplementing AstraLint's rule engine in JS. No C++ changes, no new
runtime deps, no Pyodide in the wacdfpp app.

## Why handoff (not reimplement)

AstraLint's ISTP suite is 42 declarative YAML rules driven by an engine of ~11
assertion primitives (path resolver over the CDF tree + Jinja2-templated
messages, ~1300 LOC). Reimplementing that faithfully in JS would be large and
would drift from canonical. AstraLint already runs in the browser via Pyodide and
accepts a CDF by file-drop, file-picker, and fetch-from-URL. So the wacdfpp
"Validate" feature is a small **handoff**, not a linter.

Current AstraLint web app (`docs/demo/index.html`) does **not** yet read a `?url=`
query param on load and has **no** `postMessage` listener — both stages below add
that on the AstraLint side.

## Stage 1 — deep-link `?url=` (this cycle)

### wacdfpp side
- Add a **"Validate (ISTP)"** button to the header/status row.
- Track the current file's **source URL** in app state (`currentUrl`): set it on
  URL-fetch and on `?url=`/`?cdf=` load; clear it (`null`) for local file / drag-drop.
- Button behavior:
  - **URL source present** → enabled; opens, in a new tab,
    `https://sciqlop.github.io/AstraLint/?url=<encodeURIComponent(cdfUrl)>&suite=ISTP`
    via `window.open(url, "_blank", "noopener")`.
  - **No URL (local/drag-drop)** → disabled, with a title/hint: "Load from a URL to
    validate — local-file validation coming soon."
- Pure builder `astralintUrl(cdfUrl, suite)` (unit-tested) constructs the link
  (encodes the CDF URL, defaults `suite` to `ISTP`). Base URL is a module constant
  `ASTRALINT_URL`.

### AstraLint side (separate branch/PR)
- On load, read `URLSearchParams`. If `url` is present: prefill the existing URL
  input, pre-select `suite` if given (else leave default), and — after Pyodide +
  astralint finish initializing — auto-run the existing URL-load/lint path.
- No behavior change when the params are absent. ~15 lines.

## Stage 2 — postMessage byte transfer (next cycle, outlined)

### wacdfpp side
- "Validate" becomes enabled for **any** loaded CDF.
- If a URL source exists, keep using the Stage 1 deep-link. Otherwise: `window.open`
  AstraLint, wait for its `astralint-ready` message, then
  `postMessage({ type: "astralint-file", name, suite, bytes }, ASTRALINT_ORIGIN, [bytes.buffer])`
  (transfer the ArrayBuffer). Handle popup-blocked and timeout gracefully.

### AstraLint side
- After init, `postMessage({ type: "astralint-ready" }, "*")` to its opener.
- Add a `message` listener (origin-checked) that, on `astralint-file`, runs the lint
  on the received bytes via the existing file-processing path.

This covers local, drag-drop, and future edited-in-memory files.

## Architecture / units (wacdfpp side)

Small, no C++:
- New `wacdfpp/astralint.js`: pure `astralintUrl(cdfUrl, suite)` + `ASTRALINT_URL`
  constant + an `openValidation(currentUrl)` handler (does the `window.open`).
- `wacdfpp/app.js`: add `currentUrl` state, set/clear it in the load paths, add the
  Validate button element + enable/disable + click wiring.
- `wacdfpp/wacdfpp.html`: the button markup in the status/header row + minimal CSS
  (reuse existing button styles; a disabled state).
- `wacdfpp/meson.build`: copy `astralint.js` into the build.

## Error handling

- Local file with no URL (Stage 1): button disabled, explanatory title — never a
  dead click.
- `window.open` returns null (popup blocked): surface a brief status message asking
  to allow popups (Stage 1 is a user-gesture click so usually allowed).
- AstraLint `?url=` change is additive; absent params = today's behavior.

## Testing

- **Pure Node test** (`tests/wacdfpp_astralint/test.mjs`, registered only in
  `tests/meson.build`): `astralintUrl()` — encodes the CDF URL, defaults suite to
  ISTP, honors an explicit suite, uses the right base.
- **Playwright** (wacdfpp): Validate button is **disabled** after a local-file load
  and **enabled** after a URL load; clicking the enabled button calls `window.open`
  (stubbed) with the expected `…/AstraLint/?url=…&suite=ISTP`.
- **AstraLint** `?url=` change: verified manually in-browser (Pyodide) — a
  `?url=<public cdf>` auto-runs the lint. Out of scope for the wacdfpp test suite.

## Out of scope (later / YAGNI)

- Stage 2 postMessage (next cycle).
- Embedding AstraLint results inside the wacdfpp page (we open AstraLint's own UI).
- Suite selection UI in wacdfpp (default ISTP; suite is a constant for now).
- Re-compress/re-majority and metadata editing (separate Phase 3 sub-projects).
