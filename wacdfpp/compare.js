// Compare mode: diff two CDF files on structure + metadata.
// Each file is loaded -> model -> CdfFile deleted immediately (no values needed),
// so we never hold two live CdfFiles and sidestep nomap reference invalidation.
import { loadModule } from "./wasm.js";
import { rawFromCdfFile, buildModel } from "./cdf-model.js";
import { diffModels, diffSummary, buildLines, STATUS } from "./cdf-diff.js";
import { esc } from "./render.js";

const SECTION_LABELS = {
    global: "Global Attributes", data: "Data", support_data: "Support", metadata: "Metadata",
};
const SIGN = { added: "+", removed: "−", changed: "~", same: " " };

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

const statusClass = (status) =>
    status === STATUS.ADDED ? "add"
        : status === STATUS.REMOVED ? "del"
            : status === STATUS.CHANGED ? "chg" : "ctx";

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
    for (const ln of lines) {
        if (ln.type === "section") { root.appendChild(hunk(SECTION_LABELS[ln.section])); }
        else if (ln.type === "item") { row(SIGN[ln.status], `dl-item ${statusClass(ln.status)}`, ln.label); }
        else if (ln.status === STATUS.CHANGED) {
            row("−", "del", cellText(ln.label, ln.a));
            row("+", "add", cellText(ln.label, ln.b));
        } else if (ln.status === STATUS.ADDED) { row("+", "add", cellText(ln.label, ln.b)); }
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
    const itemRow = (status, label) => {
        const el = document.createElement("div");
        el.className = `dl span dl-item ${statusClass(status)}`;
        el.innerHTML = `<span class="dsign">${SIGN[status]}</span><span class="dtext">${esc(label)}</span>`;
        root.appendChild(el);
    };
    for (const ln of lines) {
        if (ln.type === "section") { const h = hunk(SECTION_LABELS[ln.section], true); root.appendChild(h); }
        else if (ln.type === "item") { itemRow(ln.status, ln.label); }
        else if (ln.status === STATUS.CHANGED) {
            cell("del", cellText(ln.label, ln.a));
            cell("add right", cellText(ln.label, ln.b));
        } else if (ln.status === STATUS.ADDED) {
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
        `<span class="st-changed">${s.changed} changed</span>`;
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
