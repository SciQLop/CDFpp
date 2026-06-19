# Reverse handoff — "Open in CDFpp Explorer" from AstraLint — design + plan

**Date:** 2026-06-19
**Status:** Approved
**Component:** `wacdfpp/` (receive side) + AstraLint demo (send side). Branches: `wasm-demo-app` (CDFpp), new branch in AstraLint.

## Goal

The mirror of the Validate handoff: from AstraLint, open the currently-loaded CDF
in **CDFpp Explorer**. URL-sourced files deep-link via `?url=` (Explorer already
supports it); local/dropped files transfer via `postMessage`.

## CDFpp Explorer — receive side (`wacdfpp/app.js`)

- `openerOrigin()` helper: opener origin from `document.referrer`, `'*'` fallback.
- After WASM init (end of `init()` success), if `globalThis.opener`:
  `opener.postMessage({ type: "cdfpp-ready" }, openerOrigin())`.
- A `message` listener (registered at module load): ignore unless
  `e.source === globalThis.opener`; on `{ type: "cdfpp-file", name, bytes }` (and
  `Module` ready, not `busy`), load via the existing `inspect(bytes, name, null)`.
- No deep-link change needed — `init()` already handles `?url=`/`?cdf=`.

## AstraLint — send side (`docs/demo/index.html`, new branch/PR off `main`)

- Constants `CDFPP_URL = "https://sciqlop.github.io/CDFpp/"`, `CDFPP_ORIGIN`.
- Track the loaded file: `lastBytes`/`lastName` (set in `validateBytes`) and
  `lastUrl` (set in `processFileFromUrl`; cleared when a local file is validated).
- An **"Open in CDFpp Explorer"** button (near the suite selector), disabled until a
  file is loaded. Click:
  - `lastUrl` present → `window.open(CDFPP_URL + "?url=" + encodeURIComponent(lastUrl))`.
  - else `lastBytes` → `window.open(CDFPP_URL)`, await `cdfpp-ready` (origin +
    `e.source === win` checked), then
    `win.postMessage({ type: "cdfpp-file", name, bytes }, CDFPP_ORIGIN, [buf])`.
  - 60 s timeout; popup-block tolerated.

## Testing

- **wacdfpp receive** (Playwright, same-origin opener): a small opener page on the
  Explorer's origin `window.open`s `wacdfpp.html`, waits for `cdfpp-ready`, posts a
  fixture's bytes; assert the Explorer loads it (file name + variable list populate),
  zero console errors.
- **AstraLint send** (Playwright, mock CDFpp receiver via route interception): the
  button deep-links for a URL file, and for a local file opens CDFpp and posts a
  `cdfpp-file` payload (name + non-empty bytes).

## Out of scope

- Auto-selecting a variable after a received load (Explorer shows the list; user picks).
- Any change to the forward (Validate) handoff.
