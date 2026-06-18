# WASM demo → useful CDF web app — design

**Date:** 2026-06-18
**Status:** Approved (roadmap + Phase 1 detailed)
**Component:** `wacdfpp/` (Emscripten/WASM wrapper + browser demo)

## Goal

Evolve the WASM demo from a read-only metadata dumper into a genuinely useful,
client-side CDF web app for both scientists (consumers, quick-look) and engineers
(producers, validators). Everything runs in the browser; no file ever leaves the
client.

## Constraints

- **No build step.** Stay vanilla: static files served as-is on GitHub Pages.
  Only small, focused libraries, vendored locally (e.g. uPlot ~50 KB in Phase 2).
- Honor KISS / small-units / locality-of-reasoning: split logic into a few small
  ES modules rather than one growing inline script.
- Preserve existing entry points: load local file, fetch URL, `?url=` permalink.

## Roadmap (phased)

Each phase is a separate spec → plan → implementation cycle. Only Phase 1 is
detailed here.

| Phase | Theme | Ships |
|---|---|---|
| **1 — UX foundation** | Master/detail shell | Searchable, grouped variable/attr list; detail panel; drag-and-drop. No C++ changes. |
| **2 — Quick-look** | Plotting | uPlot line plots + custom canvas spectrogram, ISTP-driven; fill/validmin masking; CSV/JSON export. |
| **3 — Validate + edit + save** | Producer tools | ISTP compliance linter; metadata editing; re-compress / re-majority; download via existing `save()`. |

Rationale for ordering: Phase 1 is the scaffold both later pillars plug into and
is needed regardless (real files have hundreds of variables). Phase 2 changes the
app's identity from "viewer" to "I can look at my data." Phase 3 is producer-facing
and reuses the already-working `save()` binding.

## Phase 1 — UX foundation

### Architecture

Replace the single inline-script `wacdfpp.html` with a buildless ES-module split,
served statically (loaded via `<script type="module">`):

```
wacdfpp/
  wacdfpp.html      # shell markup + CSS only
  app.js            # wiring: load/fetch/drag-drop, app state, selection
  cdf-model.js      # thin JS view over CdfFile: VAR_TYPE grouping, search index
  render.js         # list rendering, detail-panel rendering, value formatting
  vendor/           # (empty in Phase 1; uPlot lands here in Phase 2)
```

Module responsibilities (each independently understandable/testable):

- **`cdf-model.js`** — given a loaded `CdfFile`, builds a plain-data model: groups
  variables by ISTP `VAR_TYPE` (`data` / `support_data` / `metadata`; missing →
  `data`), exposes global attributes, and provides a search/filter function over
  variable names, attribute names, and attribute values. No DOM, no Emscripten
  specifics beyond the existing `CdfFile` getters.
- **`render.js`** — pure rendering: list groups, detail panel, value formatting
  (reuses today's time/char/numeric formatters: `nsToISO`, `decodeChars`,
  `stripPadding`). Takes model data + a selection, returns/updates DOM. No I/O.
- **`app.js`** — orchestration only: load (file/URL/drag-drop/`?url=`), hold
  current model + selected variable, wire search box and list clicks to re-render.
- **`wacdfpp.html`** — markup for the two-pane shell and all CSS.

**No `wacdfpp.cpp` changes in Phase 1.** `VAR_TYPE` is already reachable through
`get_variable().attributes`. The existing bindings (`variable_names`,
`attribute_names`, `get_variable`, `get_attribute`, `time_values_as_ns_since_1970`)
cover everything Phase 1 needs.

### Layout (master/detail)

Two-pane shell:

- **Left (master):** a sticky **search box** over a scrollable list with
  collapsible groups:
  - **Global Attributes**
  - **Data** (`VAR_TYPE=data` and untyped variables)
  - **Support** (`VAR_TYPE=support_data`)
  - **Metadata** (`VAR_TYPE=metadata`)

  Each variable row shows: name · `[shape]` · type name (`nrv` marker preserved).
  Group headers show counts. Live search filters rows across variable names,
  attribute names, and attribute values; empty groups hide while filtering.
- **Right (detail):** for the selected variable — formatted attribute list (all
  attributes; time-typed values rendered as ISO strings as today), shape / type /
  compression / `nrv`, and a **value preview table** (first 20 records, matching
  the spirit of today's 5-value inline preview but in tabular form, using the
  existing formatters). A clearly-labeled **placeholder block** reserves the spot
  where the Phase 2 plot will render.
- **Header/status:** preserve current file name + parse-time + byte-count status
  line and the WASM-ready indicator.

### Features in scope

- Two-pane master/detail layout.
- Load local file, fetch URL, `?url=` permalink — **preserved**.
- **Drag-and-drop** a `.cdf` file onto the page to load it.
- VAR_TYPE-grouped, collapsible variable list + Global Attributes group.
- Live search/filter across variable names, attribute names, attribute values.
- Detail panel: metadata + value preview table + plot placeholder.

### Explicitly out of scope (later phases / YAGNI)

- Plotting (Phase 2).
- Value export to CSV/JSON/npy (Phase 2).
- Metadata editing, ISTP linter, re-save (Phase 3).

### Error handling

- Invalid / unparseable CDF: keep current behavior — surface an error in the
  status line and detail panel; never crash the page.
- Empty / oversized buffers and fetch failures: existing handling preserved.
- Respect the documented WASM heap-growth / detached-ArrayBuffer rules — attribute
  values already arrive as owned copies; variable value views are read for the
  preview before any further allocation.

### Testing

- Existing WASM CI tests (`tests/wasm_*`, save round-trip) must keep passing —
  Phase 1 does not touch C++, so they are unaffected.
- Manual verification via the WASM dev loop:
  `source ~/Documents/prog/emsdk/emsdk_env.sh` → `ninja -C build_wasm` →
  `meson test -C build_wasm`. Load a real multi-variable CDF (e.g. a THEMIS file)
  and confirm: grouping is correct, search filters across names/attrs/values,
  drag-and-drop works, detail panel renders metadata + value preview.
- Never run the `full_corpus` test (network download).
- `cdf-model.js` (grouping + search) is pure data transformation and is the
  natural unit to cover with a small Node test if a JS test harness is added; not
  required for Phase 1 sign-off.

## Open questions / deferred decisions

- Plot library confirmed direction (uPlot, vendored) but finalized in Phase 2 spec.
- Spectrogram rendering approach (custom canvas heatmap) detailed in Phase 2 spec.
- Whether to add a JS unit-test harness — decide when Phase 1 lands if `cdf-model.js`
  warrants it.
