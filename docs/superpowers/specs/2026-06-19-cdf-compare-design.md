# CDF Compare (structural diff) — Design

**Date:** 2026-06-19
**Component:** `wacdfpp` (CDFpp Explorer, the WebAssembly browser app)
**Status:** Approved design, pending implementation plan

## Goal

Let a user compare **two CDF files** in the browser and see, at a glance, how
their **structure and metadata** differ: which variables and attributes were
added, removed, or changed.

## Scope

In scope (v1):

- **Variables**: added / removed / changed. A variable is *changed* if its
  shape, type, NRV flag, or any per-variable attribute differs.
- **Global attributes**: added / removed / changed, compared **per entry**
  (index-aligned).
- Unified, color-coded diff view with a filter (All / Changes only).
- Shareable diffs via URL params.

Out of scope (YAGNI — may follow later):

- **Value-level diff** (element-by-element comparison of variable data).
- **Rename detection** — a renamed variable shows as one removed + one added.
- **Type-only attribute diffs** — comparison is on the canonical value; a type
  change that does not alter the displayed value is not flagged. (`types` is
  preserved in the model, so type-aware diffing can be added later.)
- Plotting / value preview inside the compare view.

## Architecture

The existing app already separates a pure, DOM-free **model**
(`cdf-model.js`) from **rendering** (`render.js`) and **orchestration**
(`app.js`). The diff feature follows the same split.

### New / changed modules

- **`wasm.js`** *(new, tiny refactor)* — memoized `loadModule()` extracted from
  `app.js`'s `createCdfModule()` init, so the single-file explorer and the
  compare view share one WASM instance.

- **`cdf-model.js`** *(changed)* — `rawFromCdfFile` keeps the full `entries`
  and `types` arrays for each global attribute instead of collapsing them to
  `entries.join(", ")`. Global-attribute model record becomes:
  ```js
  { name, entries: Array<string|TypedArray|string[]>, types: number[] }
  ```
  This is a model-holds-data / render-decides-display change. No behavior change
  for the existing explorer (render joins for display — see below).

- **`render.js`** *(changed)* — the single-file global-attribute rows build their
  display string as `a.entries.map(formatAttrValue).join(", ")` instead of
  reading the pre-joined `a.value`. Pure formatters unchanged.

- **`cdf-diff.js`** *(new, pure — Node-testable like `cdf-model.js`)* — computes
  a plain diff structure from two models. No DOM.

- **`compare.js`** *(new)* — compare-mode orchestration: loads two files, builds
  two models, calls `diffModels`, and renders the unified diff DOM. Reuses the
  group / `esc` / CSS conventions from `render.js`.

- **`app.js`** *(changed)* — uses `wasm.js` for init; gains a **Compare** toggle
  that swaps the single-file explorer for the A/B compare layout in place, and
  honors the new URL params.

- **`wacdfpp.html`** *(changed)* — adds the Compare toggle, the A/B input row,
  and the compare result container. Diff colors reuse existing CSS vars
  (`--green`, `--red`, `--yellow`, `--text-dim`).

### Diff data structure

`diffModels(modelA, modelB)` returns:

```js
{
  globalAttributes: [
    { name, status,                       // added | removed | changed | same
      entries: [ { index, status, a, b } ] }   // per-entry, index-aligned
  ],
  groups: {
    data:         [ varDiff ],
    support_data: [ varDiff ],
    metadata:     [ varDiff ],
  }
}

// varDiff:
{ name, status,                           // added | removed | changed | same
  fields:     [ { field, status, a, b } ],     // field: 'shape' | 'type' | 'isNrv'
  attributes: [ { name,  status, a, b } ] }     // per-variable attrs, keyed
```

`a` / `b` hold the canonical display string for each side (or `null` when absent
on that side).

### Status rules

- **Variable**: in A only → `removed`; in B only → `added`; in both → compare
  `fields` + `attributes`, `changed` if any differs/added/removed, else `same`.
- **Global attribute**: in A only → `removed`; in B only → `added`; in both →
  per-entry compare, `changed` if any entry differs/added/removed, else `same`.
- **Per-entry / per-field / per-attribute**: present on one side → `added` /
  `removed`; both → `changed` if canonical values differ, else `same`.

### Variable grouping in the diff

Variables are grouped by effective `VAR_TYPE` (data / support_data / metadata),
using B's group when the variable exists in B, otherwise A's. A `VAR_TYPE`
change surfaces naturally as a changed attribute. Within each group, variables
are sorted: changed/added/removed first, then `same`, alphabetical within each.

### Canonicalization

A pure `canonical(value)` helper in `cdf-diff.js` maps each value to a
comparable string:

- `string` → trimmed string
- numeric `TypedArray` / `Array` → comma-joined element list
- `string[]` (time ISO entries) → comma-joined list

Two values are equal iff their canonical strings are equal.

## Key data-flow decision: delete each `CdfFile` after building its model

Compare mode needs only structure + metadata, so each file is processed as
`load → rawFromCdfFile → buildModel → cdf.delete()`. Only the two plain-JS
models are kept alive; **never two live `CdfFile`s**. This sidesteps the nomap
reference-invalidation limitation entirely and keeps memory flat.

Known wart (pre-existing, not fixed here): `get_variable` force-loads values
even when only metadata is needed, so building a model still reads all values
once. Flagged as a future optimization (metadata-only binding).

## UI

- **Entry**: a **Compare** toggle in the header swaps the single-file explorer
  for an A/B layout (two file/URL inputs) in place. Toggling back restores the
  explorer. One page, one WASM load.
- **Result**: a summary line (`N added · M removed · K changed`), a
  filter (All / Changes only), then **Global Attributes** followed by the three
  variable groups. Each row is badged `+` / `−` / `~` and color-coded. Changed
  variables expand to show field diffs and per-attribute diffs as `old → new`;
  changed global attributes expand to show per-entry diffs.
- **URL params** (consistent with the existing `?url=`):
  - `?compare` opens the compare view.
  - `?a=<url>&b=<url>` opens compare and auto-fetches both files → shareable diff.

## Testing

- **Node unit tests** for `cdf-diff.js` (new `tests/wacdfpp_diff/`, mirroring
  `tests/wacdfpp_model`):
  - identical models → everything `same`
  - variable added / removed
  - variable changed: shape, type, NRV flag
  - per-variable attribute added / removed / changed
  - global attribute added / removed
  - global attribute changed: per-entry add / remove / change, index alignment
  - `canonical()` for string / numeric-array / time-ISO-array
- **Model regression**: a Node test that `rawFromCdfFile`'s new global-attr
  shape still renders the same joined display string (guards the `render.js`
  change).
- **DOM / visual**: verified manually via the existing dev loop.

## Files touched (summary)

| File | Change |
|------|--------|
| `wacdfpp/wasm.js` | new — memoized module loader |
| `wacdfpp/cdf-diff.js` | new — pure diff + canonicalizer |
| `wacdfpp/compare.js` | new — compare orchestration + diff DOM |
| `wacdfpp/cdf-model.js` | global-attr record keeps `entries` + `types` |
| `wacdfpp/render.js` | global-attr rows join entries for display |
| `wacdfpp/app.js` | use `wasm.js`; Compare toggle; URL params |
| `wacdfpp/wacdfpp.html` | Compare toggle, A/B inputs, result container |
| `wacdfpp/meson.build` | add `wasm.js`, `cdf-diff.js`, `compare.js` to `extra_files` + `fs.copyfile` (deploy copy) |
| `tests/wacdfpp_diff/test.mjs` | new — Node unit tests for the diff |
| `tests/meson.build` | register the `wacdfpp_diff` Node test (alongside `wacdfpp_model`) |
