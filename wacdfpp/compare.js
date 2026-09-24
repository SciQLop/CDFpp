// Compare mode: diff two CDF files on structure + metadata.
// Each file is loaded -> model -> CdfFile deleted immediately (no values needed),
// so we never hold two live CdfFiles and sidestep nomap reference invalidation.
import { loadModule } from "./wasm.js";
import { rawFromCdfFile, buildModel } from "./cdf-model.js";
import { diffModels, diffSummary, buildLines, lineRows, wordParts, STATUS } from "./cdf-diff.js";
import { esc } from "./render.js";
// `Diff` is the vendored jsdiff global (wacdfpp/jsdiff.js, loaded as a classic
// <script> in wacdfpp.html, before this module) -- not an ES import, since
// jsdiff ships UMD/CJS builds only, no single-file ESM bundle.

const SECTION_LABELS = {
    global: "Global Attributes", data: "Data", support_data: "Support", metadata: "Metadata",
};
const SIGN = { added: "+", removed: "−", changed: "~", renamed: "↦", same: " " };

async function bytesToModel(bytes) {
    const Module = await loadModule();
    const cdf = Module.load(bytes);
    if (!cdf.is_valid()) { cdf.delete(); return null; }
    const { rawVars, rawGlobals } = rawFromCdfFile(cdf);
    const model = buildModel(rawVars, rawGlobals);
    cdf.delete();
    return model;
}

function readFile(file) {
    return new Promise((resolve, reject) => {
        const r = new FileReader();
        r.onload = (e) => resolve(new Uint8Array(e.target.result));
        r.onerror = () => reject(new Error(`read ${file.name}`));
        r.readAsArrayBuffer(file);
    });
}

async function fetchBytes(url) {
    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    return new Uint8Array(await resp.arrayBuffer());
}

// --- rendering ----------------------------------------------------------------
// A detail value is null when absent on that side; otherwise show "label: value".
const cellText = (label, value) => (value === null ? "" : `${label}: ${value}`);

const STATUS_CLASS = {
    [STATUS.ADDED]: "add", [STATUS.REMOVED]: "del", [STATUS.CHANGED]: "chg", [STATUS.RENAMED]: "ren",
};
const statusClass = (status) => STATUS_CLASS[status] ?? "ctx";

// HTML for each side of an edited line, the differing words (wordParts) wrapped
// in a highlight span layered on the line's own tint (see the .wd-add/.wd-del
// CSS) — so a small change in a long, mostly-identical line stands out.
function wordDiffHtml(parts) {
    let delHtml = "", addHtml = "";
    for (const p of parts) {
        if (p.added) addHtml += `<mark class="wd-add">${esc(p.value)}</mark>`;
        else if (p.removed) delHtml += `<mark class="wd-del">${esc(p.value)}</mark>`;
        else { const t = esc(p.value); delHtml += t; addHtml += t; }
    }
    return { delHtml, addHtml };
}

// Rows of a changed field, one per line (see lineRows): only an edited line
// similar enough to its counterpart gets word-level marks (see wordParts).
const changedRows = (ln) => lineRows(cellText(ln.label, ln.a), cellText(ln.label, ln.b));
const isContext = (r) => r.a === r.b;
const rowSides = (r) => {
    const parts = r.a !== null && r.b !== null ? wordParts(r.a, r.b) : null;
    return parts ? wordDiffHtml(parts) : { delHtml: esc(r.a ?? ""), addHtml: esc(r.b ?? "") };
};

function hunk(label, span) {
    const el = document.createElement("div");
    el.className = "diff-hunk" + (span ? " span" : "");
    el.textContent = label;
    return el;
}

// Unified view: a removed (red) line above an added (green) line for a change.
function renderInline(lines) {
    const root = document.createElement("div");
    root.className = "diff-view diff-inline";
    const row = (sign, cls, text) => {
        const el = document.createElement("div");
        el.className = `dl ${cls}`;
        el.innerHTML = `<span class="dsign">${sign}</span><span class="dtext">${esc(text)}</span>`;
        root.appendChild(el);
    };
    const rowHtml = (sign, cls, html) => {
        const el = document.createElement("div");
        el.className = `dl ${cls}`;
        el.innerHTML = `<span class="dsign">${sign}</span><span class="dtext">${html}</span>`;
        root.appendChild(el);
    };
    // Like GitHub's unified view: each run of changed lines lists all its
    // removed lines, then all its added lines; context lines appear once.
    const changedInline = (ln) => {
        let run = [];
        const flushRun = () => {
            for (const r of run) if (r.a !== null) rowHtml("−", "del", rowSides(r).delHtml);
            for (const r of run) if (r.b !== null) rowHtml("+", "add", rowSides(r).addHtml);
            run = [];
        };
        for (const r of changedRows(ln)) {
            if (isContext(r)) { flushRun(); rowHtml(" ", "ctx", esc(r.a)); }
            else run.push(r);
        }
        flushRun();
    };
    for (const ln of lines) {
        if (ln.type === "section") { root.appendChild(hunk(SECTION_LABELS[ln.section])); }
        else if (ln.type === "item") { row(SIGN[ln.status], `dl-item ${statusClass(ln.status)}`, ln.label); }
        else if (ln.status === STATUS.CHANGED) { changedInline(ln); }
        else if (ln.status === STATUS.ADDED) { row("+", "add", cellText(ln.label, ln.b)); }
        else if (ln.status === STATUS.REMOVED) { row("−", "del", cellText(ln.label, ln.a)); }
        else { row(" ", "ctx", cellText(ln.label, ln.a)); }
    }
    return root;
}

// Side-by-side view: a 2-column grid; section/item headers span both columns,
// detail lines fill left (old) and right (new).
function renderSplit(lines) {
    const root = document.createElement("div");
    root.className = "diff-view diff-split";
    const cell = (cls, text) => {
        const el = document.createElement("div");
        el.className = `dcell ${cls}`;
        el.textContent = text;
        root.appendChild(el);
    };
    const cellHtml = (cls, html) => {
        const el = document.createElement("div");
        el.className = `dcell ${cls}`;
        el.innerHTML = html;
        root.appendChild(el);
    };
    const itemRow = (status, label) => {
        const el = document.createElement("div");
        el.className = `dl span dl-item ${statusClass(status)}`;
        el.innerHTML = `<span class="dsign">${SIGN[status]}</span><span class="dtext">${esc(label)}</span>`;
        root.appendChild(el);
    };
    // One grid row per line; continuation lines of the same field drop the
    // row separator so a multi-line value still reads as one block.
    const changedSplit = (ln) => changedRows(ln).forEach((r, i) => {
        const cont = i ? " cont" : "";
        if (isContext(r)) {
            cellHtml(`ctx${cont}`, esc(r.a));
            cellHtml(`ctx right${cont}`, esc(r.b));
            return;
        }
        const { delHtml, addHtml } = rowSides(r);
        cellHtml(`${r.a === null ? "empty" : "del"}${cont}`, delHtml);
        cellHtml(`${r.b === null ? "empty" : "add"} right${cont}`, addHtml);
    });
    for (const ln of lines) {
        if (ln.type === "section") { const h = hunk(SECTION_LABELS[ln.section], true); root.appendChild(h); }
        else if (ln.type === "item") { itemRow(ln.status, ln.label); }
        else if (ln.status === STATUS.CHANGED) { changedSplit(ln); }
        else if (ln.status === STATUS.ADDED) {
            cell("empty", "");
            cell("add right", cellText(ln.label, ln.b));
        } else if (ln.status === STATUS.REMOVED) {
            cell("del", cellText(ln.label, ln.a));
            cell("empty right", "");
        } else {
            cell("ctx", cellText(ln.label, ln.a));
            cell("ctx right", cellText(ln.label, ln.b));
        }
    }
    return root;
}

function summaryEl(diff) {
    const s = diffSummary(diff);
    const el = document.createElement("div");
    el.className = "diff-summary";
    el.innerHTML =
        `<span class="st-added">${s.added} added</span> · ` +
        `<span class="st-removed">${s.removed} removed</span> · ` +
        `<span class="st-changed">${s.changed} changed</span> · ` +
        `<span class="st-renamed">${s.renamed} renamed</span>`;
    return el;
}

// --- state + public API -------------------------------------------------------
// view: "inline" | "split"; filter: "changes" | "all". Re-rendered on toggle from
// the cached diff (no reload needed).
let currentDiff = null, mountEl = null, view = "inline", filter = "changes";

function render() {
    if (!mountEl || !currentDiff) return;
    mountEl.innerHTML = "";
    mountEl.appendChild(summaryEl(currentDiff));
    const lines = buildLines(currentDiff, filter === "all");
    mountEl.appendChild(view === "split" ? renderSplit(lines) : renderInline(lines));
    if (!lines.length) {
        const note = document.createElement("div");
        note.className = "diff-note";
        note.textContent = "No structural differences";
        mountEl.appendChild(note);
    }
}

export function setView(mode) { view = mode; render(); }
export function setFilter(mode) { filter = mode; render(); }

// Build a model from a source: { file } or { url }. Returns { model, name }.
async function loadSource(src) {
    if (src.file) return { model: await bytesToModel(await readFile(src.file)), name: src.file.name };
    if (src.url) {
        const name = src.url.split("/").pop().split("?")[0] || "remote.cdf";
        return { model: await bytesToModel(await fetchBytes(src.url)), name };
    }
    return { model: null, name: "" };
}

// Public entry: diff two sources into `container`, report status via setStatus.
export async function runCompare(container, srcA, srcB, setStatus) {
    setStatus("loading", "Loading A & B…");
    try {
        const [a, b] = await Promise.all([loadSource(srcA), loadSource(srcB)]);
        if (!a.model || !b.model) { setStatus("error", "Failed to parse one of the files"); return; }
        currentDiff = diffModels(a.model, b.model);
        mountEl = container;
        render();
        setStatus("ready", `Compared ${a.name} ↔ ${b.name}`);
    } catch (err) {
        setStatus("error", `Compare error: ${err.message}`);
    }
}
