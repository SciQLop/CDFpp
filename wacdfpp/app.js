// Orchestration: load (file/URL/?url=/drag-drop), hold current CdfFile + model +
// selection, wire search and list selection to re-render.
import { loadModule } from "./wasm.js";
import { rawFromCdfFile, buildModel, filterModel } from "./cdf-model.js";
import { renderList, renderDetail, setSelected } from "./render.js";
import { renderPlot } from "./plot.js";
import { openValidation, openValidationBytes } from "./astralint.js";
import { runCompare, setView, setFilter } from "./compare.js";

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
    app: document.querySelector(".app"),
    resizer: document.getElementById("resizer"),
    validateBtn: document.getElementById("validateBtn"),
    modeToggle: document.getElementById("modeToggle"),
    compareInputs: document.getElementById("compareInputs"),
    compareBar: document.getElementById("compareBar"),
    compareResult: document.getElementById("compareResult"),
    fileA: document.getElementById("fileA"),
    urlA: document.getElementById("urlA"),
    fileB: document.getElementById("fileB"),
    urlB: document.getElementById("urlB"),
    compareBtn: document.getElementById("compareBtn"),
    viewToggle: document.getElementById("viewToggle"),
    filterToggle: document.getElementById("filterToggle"),
};

let Module;
let currentCdf;          // live CdfFile — delete before replacing
let currentModel;        // built once per file
let currentUrl = null;   // source URL of the loaded file (null for local/drag-drop)
let currentBytes = null; // raw bytes of the loaded file (for the postMessage handoff)
let currentName = null;  // file name of the loaded file
let selectedName = null;
let searchQuery = "";
let busy = false;

function updateValidate() {
    const ready = !!(currentUrl || currentBytes);
    els.validateBtn.disabled = !ready;
    els.validateBtn.title = ready
        ? "Validate this CDF against ISTP in AstraLint"
        : "Load a CDF to validate it against ISTP in AstraLint";
}

function setStatus(cls, text) { els.status.className = cls; els.statusText.textContent = text; }

function refreshList() {
    const view = filterModel(currentModel, searchQuery);
    renderList(els.varlist, view, { selected: selectedName, onSelect: selectVariable });
}

function selectVariable(name) {
    selectedName = name;
    renderDetail(els.detail, currentCdf, name);
    const mount = els.detail.querySelector(".plot-panel");
    if (mount && currentCdf) renderPlot(mount, currentCdf, name);
    setSelected(els.varlist, name);   // highlight in place; don't rebuild (keeps groups' open state)
}

function inspect(data, name, sourceUrl) {
    if (currentCdf) { currentCdf.delete(); currentCdf = undefined; }
    const t0 = performance.now();
    const cdf = Module.load(data);
    const dt = (performance.now() - t0).toFixed(1);
    if (!cdf.is_valid()) {
        cdf.delete();
        currentModel = buildModel([], []);
        selectedName = null;
        searchQuery = "";
        els.search.value = "";
        currentUrl = null;
        currentBytes = null;
        currentName = null;
        updateValidate();
        refreshList();
        els.detail.innerHTML = `<div class="log-err">ERROR: failed to parse CDF</div>`;
        setStatus("error", `Failed to parse ${name}`);
        return;
    }
    currentCdf = cdf;
    const { rawVars, rawGlobals } = rawFromCdfFile(cdf);
    currentModel = buildModel(rawVars, rawGlobals);
    selectedName = null;
    searchQuery = "";
    els.search.value = "";
    currentUrl = sourceUrl ?? null;
    currentBytes = data;
    currentName = name;
    updateValidate();
    els.fileName.textContent = name;
    els.parseTime.textContent = `parsed in ${dt} ms`;
    els.detail.innerHTML = `<div class="log-dim">Select a variable to inspect.</div>`;
    refreshList();
    setStatus("ready", `Loaded ${name} (${data.length.toLocaleString()} bytes)`);
}

// Reflect the current source into the address bar so it is shareable/bookmarkable
// and survives reload. Local files have no URL, so they clear the query.
function syncUrl(params) {
    const qs = params.toString();
    globalThis.history.replaceState(null, "", globalThis.location.pathname + (qs ? `?${qs}` : ""));
}

function loadFile(file) {
    if (!Module || busy) return;
    busy = true;
    setStatus("loading", `Loading ${file.name}...`);
    const reader = new FileReader();
    reader.onload = (e) => {
        try { inspect(new Uint8Array(e.target.result), file.name, null); syncUrl(new URLSearchParams()); }
        finally { busy = false; }
    };
    reader.onerror = () => { setStatus("error", `Failed to read ${file.name}`); busy = false; };
    reader.readAsArrayBuffer(file);
}

async function fetchUrl(url) {
    if (!url || !Module || busy) return;
    busy = true;
    setStatus("loading", "Fetching...");
    try {
        const resp = await fetch(url);
        if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
        const data = new Uint8Array(await resp.arrayBuffer());
        const name = url.split("/").pop().split("?")[0].split("#")[0] || "remote.cdf";
        inspect(data, name, url);
        syncUrl(new URLSearchParams({ url }));
    } catch (err) {
        setStatus("error", `Fetch error: ${err.message}`);
    } finally {
        busy = false;
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
els.validateBtn.addEventListener("click", () => {
    if (currentUrl) openValidation(currentUrl);
    else if (currentBytes) openValidationBytes(currentBytes, currentName ?? "file.cdf");
});

// Reverse handoff: load a CDF handed to us by an opener (e.g. AstraLint's
// "Open in CDFpp Explorer"). We post 'cdfpp-ready' after init (see init()).
function openerOrigin() {
    try { return document.referrer ? new URL(document.referrer).origin : "*"; }
    catch { return "*"; }
}
globalThis.addEventListener("message", (e) => {
    if (e.source !== globalThis.opener) return;
    const d = e.data;
    if (!d || d.type !== "cdfpp-file" || !d.bytes || !Module || busy) return;
    const bytes = d.bytes instanceof Uint8Array ? d.bytes : new Uint8Array(d.bytes);
    busy = true;
    try { inspect(bytes, d.name || "file.cdf", null); }
    finally { busy = false; }
});

let compareMode = false;

function setCompareMode(on) {
    compareMode = on;
    els.modeToggle.textContent = on ? "Inspect ↩" : "Compare ↔";
    els.compareInputs.style.display = on ? "grid" : "none";
    els.compareBar.style.display = on ? "flex" : "none";
    els.compareResult.style.display = on ? "block" : "none";
    // single-file controls
    document.querySelector(".input-section").style.display = on ? "none" : "grid";
    els.app.style.display = on ? "none" : "grid";
    document.getElementById("output-header").style.display = on ? "none" : "flex";
}

function runCompareFromInputs() {
    const urlA = els.urlA.value.trim(), urlB = els.urlB.value.trim();
    runCompare(
        els.compareResult,
        { file: els.fileA.files[0], url: urlA || null },
        { file: els.fileB.files[0], url: urlB || null },
        setStatus,
    );
    // Share the remote sides; keep ?compare so a reload reopens compare mode even
    // when a side is a local file (which cannot be encoded in the URL).
    const params = new URLSearchParams();
    if (urlA) params.set("a", urlA);
    if (urlB) params.set("b", urlB);
    if (!(urlA && urlB)) params.set("compare", "1");
    syncUrl(params);
}

let cmpView = "inline", cmpFilter = "changes";

els.modeToggle.addEventListener("click", () => setCompareMode(!compareMode));
els.compareBtn.addEventListener("click", runCompareFromInputs);
els.viewToggle.addEventListener("click", () => {
    cmpView = cmpView === "split" ? "inline" : "split";
    setView(cmpView);
    els.viewToggle.textContent = cmpView === "split" ? "Inline ⇄" : "Side-by-side ⇄";
});
els.filterToggle.addEventListener("click", () => {
    cmpFilter = cmpFilter === "all" ? "changes" : "all";
    setFilter(cmpFilter);
    els.filterToggle.textContent = cmpFilter === "all" ? "Show changes" : "Show all";
});

async function init() {
    try {
        Module = await loadModule();
        setStatus("ready", "Ready");
        els.loadBtn.disabled = false;
        els.fetchBtn.disabled = false;
        updateValidate();
        // Tell an opener (e.g. AstraLint) we can receive a CDF via postMessage.
        if (globalThis.opener) globalThis.opener.postMessage({ type: "cdfpp-ready" }, openerOrigin());
        const params = new URLSearchParams(globalThis.location.search);
        const a = params.get("a"), b = params.get("b");
        if (params.has("compare") || a || b) {
            setCompareMode(true);
            if (a) els.urlA.value = a;
            if (b) els.urlB.value = b;
            if (a && b) runCompareFromInputs();
        } else {
            const url = params.get("url") || params.get("cdf");
            if (url) { els.urlInput.value = url; fetchUrl(url); }
        }
    } catch (err) {
        setStatus("error", `Init failed: ${err.message}`);
    }
}

function setDrop(active) { els.dropzone.classList.toggle("active", active); }

["dragenter", "dragover"].forEach(ev =>
    globalThis.addEventListener(ev, (e) => { e.preventDefault(); setDrop(true); }));
globalThis.addEventListener("dragleave", (e) => {
    e.preventDefault();
    if (e.relatedTarget) return; // ignore leaves into inner elements
    setDrop(false);
});
globalThis.addEventListener("drop", (e) => {
    e.preventDefault();
    setDrop(false);
    const file = e.dataTransfer?.files?.[0];
    if (file) loadFile(file);
});

// Draggable splitter: pointer drag sets the sidebar column width (clamped).
function setupResizer() {
    const { app, resizer } = els;
    if (!app || !resizer) return;
    const onMove = (e) => {
        const w = Math.min(Math.max(e.clientX - app.getBoundingClientRect().left, 180), 900);
        app.style.setProperty("--sidebar-w", `${w}px`);
    };
    resizer.addEventListener("pointerdown", (e) => {
        e.preventDefault();
        resizer.setPointerCapture(e.pointerId);
        resizer.classList.add("dragging");
        resizer.addEventListener("pointermove", onMove);
    });
    const stop = (e) => {
        resizer.releasePointerCapture(e.pointerId);
        resizer.classList.remove("dragging");
        resizer.removeEventListener("pointermove", onMove);
    };
    resizer.addEventListener("pointerup", stop);
    resizer.addEventListener("pointercancel", stop);
}
setupResizer();

await init();

