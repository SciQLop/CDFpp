// Converter panel: re-encodes the loaded CDF with every codec this build offers,
// reloads each output eagerly, checks the values round-trip bit-exactly, and lets
// the user download any of the outputs. The work runs in convert-worker.js so the
// page stays responsive on large files.
import { availableCodecs, summarizeRuns, outputName, fitsInBrowser } from "./convert-model.js";

const HEAD = ["Codec", "Size", "vs original", "vs GZIP", "Write", "Read", "Values", ""];

let activeWorker = null;

function formatBytes(n) {
    if (n < 1024) return `${n} B`;
    if (n < 2 ** 20) return `${(n / 1024).toFixed(1)} KiB`;
    if (n < 2 ** 30) return `${(n / 2 ** 20).toFixed(2)} MiB`;
    return `${(n / 2 ** 30).toFixed(2)} GiB`;
}
const formatMs = (ms) => (ms < 1000 ? `${ms.toFixed(ms < 10 ? 1 : 0)} ms` : `${(ms / 1000).toFixed(2)} s`);
function formatChange(ratio) {
    if (ratio === null) return "…";
    const pct = Math.round((ratio - 1) * 100);
    return pct === 0 ? "same" : `${pct > 0 ? "+" : "−"}${Math.abs(pct)}%`;
}

function download(bytes, name) {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([bytes], { type: "application/x-cdf" }));
    a.download = name;
    a.click();
    URL.revokeObjectURL(a.href);
}

function cell(tr, text, cls) {
    const td = document.createElement("td");
    td.textContent = text;
    if (cls) td.className = cls;
    tr.append(td);
    return td;
}

function panelHtml() {
    return `
        <h2>Convert codec</h2>
        <p class="log-dim">Re-encodes every variable of <b></b> with each codec, in your browser,
            then reloads the result and checks every value is bit-identical.
            <em>Write</em> is compress + serialize; <em>read</em> is parse + decompress every value.</p>
        <p class="convert-warn">Zstd and Blosc2 are experimental and not part of the CDF standard:
            only CDFpp reads them. To get a standard file back, load it here and download the GZIP version.</p>
        <p class="log-err" hidden></p>
        <div class="convert-too-large" hidden>
            <p></p>
            <pre>import pycdfpp

cdf = pycdfpp.load("file.cdf")
for name in cdf:
    cdf[name].compression = pycdfpp.CompressionType.blosc2_compression
pycdfpp.save(cdf, "file.blosc2.cdf")</pre>
        </div>
        <div class="convert-scroll"><table class="convert-table"><thead><tr>${HEAD.map((h) => `<th>${h}</th>`).join("")}</tr></thead><tbody></tbody></table></div>`;
}

function codecRow(codec) {
    const tr = document.createElement("tr");
    const label = cell(tr, codec.label, "codec");
    const note = document.createElement("small");
    note.textContent = codec.note;
    label.append(note);
    HEAD.slice(1).forEach(() => cell(tr, "…", "pending"));
    return tr;
}

function fillRow(tr, row, name) {
    const cells = tr.querySelectorAll("td");
    const values = [formatBytes(row.size), formatChange(row.vsOriginal), formatChange(row.vsGzip),
        formatMs(row.writeMs), formatMs(row.readMs), row.identical ? "✓ identical" : "✗ changed"];
    values.forEach((text, i) => { cells[i + 1].textContent = text; cells[i + 1].className = ""; });
    cells[6].className = row.identical ? "ok" : "bad";
    tr.classList.toggle("best", row.best);
    if (!cells[7].querySelector("button")) {
        cells[7].textContent = "";
        cells[7].className = "";
        const button = document.createElement("button");
        button.className = "header-btn";
        button.textContent = "Download";
        button.addEventListener("click", () => download(row.bytes, outputName(name, row.key)));
        cells[7].append(button);
    }
}

function showError(panel, message) {
    const p = panel.querySelector(".log-err");
    p.textContent = `Conversion failed: ${message}`;
    p.hidden = false;
    panel.querySelectorAll("td.pending").forEach((td) => { td.textContent = "–"; });
}

function showTooLarge(panel, decodedBytes) {
    const box = panel.querySelector(".convert-too-large");
    box.querySelector("p").textContent =
        `This file decodes to ${formatBytes(decodedBytes)}. Converting it here needs about ` +
        `${formatBytes(3 * decodedBytes)} of memory, more than the 4 GiB a browser gives WebAssembly. ` +
        "Convert it with pycdfpp instead:";
    box.hidden = false;
    panel.querySelectorAll("td.pending").forEach((td) => { td.textContent = "–"; });
}

/** Renders the panel into `mount` and converts `bytes` with every codec in a worker. */
export function renderConverter(mount, Module, { name, bytes, decodedBytes }) {
    activeWorker?.terminate();
    const panel = document.createElement("div");
    panel.className = "convert";
    panel.innerHTML = panelHtml();   // static markup; the file name goes in via textContent below
    panel.querySelector("b").textContent = name;
    mount.replaceChildren(panel);

    const codecs = availableCodecs(Module);
    const tbody = panel.querySelector("tbody");
    const trs = new Map(codecs.map((c) => [c.key, tbody.appendChild(codecRow(c))]));
    if (!fitsInBrowser(bytes.length, decodedBytes)) return showTooLarge(panel, decodedBytes);
    const runs = [];
    const worker = new Worker(new URL("./convert-worker.js", import.meta.url), { type: "module" });
    activeWorker = worker;
    const stop = () => { worker.terminate(); if (activeWorker === worker) activeWorker = null; };
    worker.onmessage = ({ data }) => {
        if (!panel.isConnected) return stop();   // user moved on to another view
        if (data.type === "run") {
            runs.push(data.run);
            summarizeRuns(runs, bytes.length).forEach((row) => fillRow(trs.get(row.key), row, name));
        } else {
            if (data.type === "error") showError(panel, data.message);
            stop();
        }
    };
    worker.onerror = (e) => { showError(panel, e.message || "worker error"); stop(); };
    worker.postMessage({ bytes, keys: codecs.map((c) => c.key) });   // structured clone: page keeps its copy
}
