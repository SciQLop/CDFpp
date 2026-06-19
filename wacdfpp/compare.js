// Compare mode: diff two CDF files on structure + metadata.
// Each file is loaded -> model -> CdfFile deleted immediately (no values needed),
// so we never hold two live CdfFiles and sidestep nomap reference invalidation.
import { loadModule } from "./wasm.js";
import { rawFromCdfFile, buildModel } from "./cdf-model.js";
import { diffModels, diffSummary, STATUS } from "./cdf-diff.js";
import { esc } from "./render.js";

const GROUP_LABELS = { data: "Data", support_data: "Support", metadata: "Metadata" };
const BADGE = { added: "+", removed: "−", changed: "~", same: "·" };

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

function diffRow(label, status, detail) {
    const row = document.createElement("div");
    row.className = `diff-row st-${status}`;
    row.dataset.status = status;
    row.innerHTML = `<span class="diff-badge">${BADGE[status]}</span> ` +
        `<span class="diff-name">${esc(label)}</span>` + (detail ? ` ${detail}` : "");
    return row;
}

function changePair(a, b) {
    if (a === null) return `<span class="log-key">${esc(b)}</span>`;
    if (b === null) return `<span class="log-err">${esc(a)}</span>`;
    return `<span class="log-err">${esc(a)}</span> → <span class="log-key">${esc(b)}</span>`;
}

function variableBlock(v) {
    const block = document.createElement("div");
    block.className = "diff-block";
    block.dataset.status = v.status;
    const shapeNote = v.fields.find(f => f.field === "shape");
    block.appendChild(diffRow(v.name, v.status,
        shapeNote ? `<span class="log-dim">[${esc(shapeNote.a)} → ${esc(shapeNote.b)}]</span>` : ""));
    const details = document.createElement("div");
    details.className = "diff-details";
    for (const f of v.fields)
        details.appendChild(detailLine(f.field, changePair(f.a, f.b)));
    for (const a of v.attributes)
        details.appendChild(detailLine(a.name, changePair(a.a, a.b)));
    if (details.children.length) block.appendChild(details);
    return block;
}

function globalBlock(a) {
    const block = document.createElement("div");
    block.className = "diff-block";
    block.dataset.status = a.status;
    block.appendChild(diffRow(a.name, a.status, ""));
    const details = document.createElement("div");
    details.className = "diff-details";
    for (const e of a.entries)
        details.appendChild(detailLine(`[${e.index}]`, changePair(e.a, e.b)));
    if (details.children.length) block.appendChild(details);
    return block;
}

function detailLine(label, html) {
    const el = document.createElement("div");
    el.className = "diff-detail";
    el.innerHTML = `<span class="log-dim">${esc(label)}:</span> ${html}`;
    return el;
}

function section(title, blocks) {
    const wrap = document.createElement("div");
    wrap.className = "diff-section";
    const head = document.createElement("div");
    head.className = "section-label";
    head.textContent = title;
    wrap.appendChild(head);
    for (const b of blocks) wrap.appendChild(b);
    return wrap;
}

export function renderDiff(container, diff) {
    container.innerHTML = "";
    const s = diffSummary(diff);
    const summary = document.createElement("div");
    summary.className = "diff-summary";
    summary.innerHTML =
        `<span class="st-added">${s.added} added</span> · ` +
        `<span class="st-removed">${s.removed} removed</span> · ` +
        `<span class="st-changed">${s.changed} changed</span>`;
    container.appendChild(summary);

    const gBlocks = diff.globalAttributes.filter(a => a.status !== STATUS.SAME).map(globalBlock);
    if (gBlocks.length) container.appendChild(section("Global Attributes", gBlocks));

    for (const g of ["data", "support_data", "metadata"]) {
        const rows = diff.groups[g].filter(v => v.status !== STATUS.SAME).map(variableBlock);
        if (rows.length) container.appendChild(section(GROUP_LABELS[g], rows));
    }

    if (container.children.length === 1)
        container.appendChild(diffRow("No structural differences", "same", ""));
    applyFilter(container, container.dataset.filter || "changes");
}

// Filter: "all" shows everything; "changes" hides same-status blocks (default).
export function applyFilter(container, mode) {
    container.dataset.filter = mode;
    for (const block of container.querySelectorAll(".diff-block"))
        block.style.display = (mode === "all" || block.dataset.status !== STATUS.SAME) ? "" : "none";
}

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
        renderDiff(container, diffModels(a.model, b.model));
        setStatus("ready", `Compared ${a.name} ↔ ${b.name}`);
    } catch (err) {
        setStatus("error", `Compare error: ${err.message}`);
    }
}
