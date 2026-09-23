// Converter panel: re-encodes the loaded CDF with every codec this build offers,
// reloads each output eagerly, checks the values round-trip bit-exactly, and lets
// the user download any of the outputs.
import { availableCodecs, summarizeRuns, outputName } from "./convert-model.js";

const HEAD = ["Codec", "Size", "vs original", "vs GZIP", "Write", "Read", "Values", ""];

const yieldToBrowser = () => new Promise((resolve) => requestAnimationFrame(() => setTimeout(resolve)));

function formatBytes(n) {
    if (n < 1024) return `${n} B`;
    if (n < 2 ** 20) return `${(n / 1024).toFixed(1)} KiB`;
    return `${(n / 2 ** 20).toFixed(2)} MiB`;
}
const formatMs = (ms) => (ms < 1000 ? `${ms.toFixed(ms < 10 ? 1 : 0)} ms` : `${(ms / 1000).toFixed(2)} s`);
function formatChange(ratio) {
    if (ratio === null) return "…";
    const pct = Math.round((ratio - 1) * 100);
    return pct === 0 ? "same" : `${pct > 0 ? "+" : "−"}${Math.abs(pct)}%`;
}

function measure(Module, cdf, codec) {
    const t0 = performance.now();
    const bytes = cdf.save_as(Module.CompressionType[codec.key]);
    const t1 = performance.now();
    const reloaded = Module.load_eager(bytes);
    const t2 = performance.now();
    const identical = reloaded.is_valid() && reloaded.same_values(cdf);
    reloaded.delete();
    return { key: codec.key, bytes, size: bytes.length, writeMs: t1 - t0, readMs: t2 - t1, identical };
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

/** Renders the panel into `mount` and runs every codec; stops if the panel is replaced. */
export async function renderConverter(mount, Module, cdf, { name, originalSize }) {
    const panel = document.createElement("div");
    panel.className = "convert";
    panel.innerHTML = panelHtml();   // static markup; the file name goes in via textContent below
    panel.querySelector("b").textContent = name;
    mount.replaceChildren(panel);

    const codecs = availableCodecs(Module);
    const tbody = panel.querySelector("tbody");
    const trs = new Map(codecs.map((c) => [c.key, tbody.appendChild(codecRow(c))]));
    const runs = [];
    for (const codec of codecs) {
        await yieldToBrowser();
        if (!panel.isConnected) return;   // user moved on; cdf may already be freed
        runs.push(measure(Module, cdf, codec));
        summarizeRuns(runs, originalSize).forEach((row) => fillRow(trs.get(row.key), row, name));
    }
}
