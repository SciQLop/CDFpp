// Orchestration: load (file/URL/?url=/drag-drop), hold current CdfFile + model +
// selection, wire search and list selection to re-render.
import createCdfModule from "./cdfpp.js";
import { rawFromCdfFile, buildModel, filterModel } from "./cdf-model.js";
import { renderList, renderDetail } from "./render.js";

const els = {
    fileInput: document.getElementById("fileInput"),
    loadBtn: document.getElementById("loadBtn"),
    fetchBtn: document.getElementById("fetchBtn"),
    urlInput: document.getElementById("urlInput"),
    status: document.getElementById("status"),
    statusText: document.getElementById("statusText"),
    fileName: document.getElementById("fileName"),
    parseTime: document.getElementById("parseTime"),
    search: document.getElementById("search"),
    varlist: document.getElementById("varlist"),
    detail: document.getElementById("detail-body"),
    dropzone: document.getElementById("dropzone"),
};

let Module;
let currentCdf;          // live CdfFile — delete before replacing
let currentModel;        // built once per file
let selectedName = null;
let searchQuery = "";

function setStatus(cls, text) { els.status.className = cls; els.statusText.textContent = text; }

function refreshList() {
    const view = filterModel(currentModel, searchQuery);
    renderList(els.varlist, view, { selected: selectedName, onSelect: selectVariable });
}

function selectVariable(name) {
    selectedName = name;
    renderDetail(els.detail, currentCdf, name);
    refreshList();   // update selection highlight
}

function inspect(data, name, dt) {
    if (currentCdf) { currentCdf.delete(); currentCdf = undefined; }
    const cdf = Module.load(data);
    if (!cdf.is_valid()) {
        setStatus("error", `Failed to parse ${name}`);
        els.detail.innerHTML = `<div class="log-err">ERROR: failed to parse CDF</div>`;
        cdf.delete();
        return;
    }
    currentCdf = cdf;
    const { rawVars, rawGlobals } = rawFromCdfFile(cdf);
    currentModel = buildModel(rawVars, rawGlobals);
    selectedName = null;
    searchQuery = "";
    els.search.value = "";
    els.fileName.textContent = name;
    els.parseTime.textContent = `parsed in ${dt} ms`;
    els.detail.innerHTML = `<div class="log-dim">Select a variable to inspect.</div>`;
    refreshList();
    setStatus("ready", `Loaded ${name} (${data.length.toLocaleString()} bytes)`);
}

function loadFile(file) {
    setStatus("loading", `Loading ${file.name}...`);
    const reader = new FileReader();
    reader.onload = (e) => {
        const data = new Uint8Array(e.target.result);
        const t0 = performance.now();
        inspect(data, file.name, (performance.now() - t0).toFixed(1));
    };
    reader.readAsArrayBuffer(file);
}

async function fetchUrl(url) {
    if (!url || !Module) return;
    setStatus("loading", "Fetching...");
    try {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const data = new Uint8Array(await resp.arrayBuffer());
        const name = url.split("/").pop().split("?")[0].split("#")[0] || "remote.cdf";
        const t0 = performance.now();
        inspect(data, name, (performance.now() - t0).toFixed(1));
    } catch (err) {
        setStatus("error", `Fetch error: ${err.message}`);
    }
}

els.loadBtn.addEventListener("click", () => {
    const file = els.fileInput.files[0];
    if (file) loadFile(file);
});
els.fetchBtn.addEventListener("click", () => fetchUrl(els.urlInput.value.trim()));
els.urlInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") fetchUrl(els.urlInput.value.trim());
});
els.search.addEventListener("input", () => {
    if (!currentModel) return;
    searchQuery = els.search.value;
    refreshList();
});

async function init() {
    try {
        Module = await createCdfModule();
        setStatus("ready", "Ready");
        els.loadBtn.disabled = false;
        els.fetchBtn.disabled = false;
        const params = new URLSearchParams(globalThis.location.search);
        const url = params.get("url") || params.get("cdf");
        if (url) { els.urlInput.value = url; fetchUrl(url); }
    } catch (err) {
        setStatus("error", `Init failed: ${err.message}`);
    }
}

await init();

export { loadFile };   // used by drag-drop wiring (next task)
