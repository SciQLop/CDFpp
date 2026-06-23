import { entryText } from "./cdf-model.js";

// Rendering for the wacdfpp master/detail UI. The formatters at the top are pure
// (no DOM) and are unit-tested in Node; the renderList/renderDetail functions
// (added in a later task) build DOM and are verified manually via the dev loop.

export const CDF_EPOCH = 31, CDF_EPOCH16 = 32, CDF_TIME_TT2000 = 33;
export const CDF_CHAR = 51, CDF_UCHAR = 52;

export const isTimeType = (t) => t === CDF_EPOCH || t === CDF_EPOCH16 || t === CDF_TIME_TT2000;
export const isCharType = (t) => t === CDF_CHAR || t === CDF_UCHAR;

// NOTE: does not remap CDF fill sentinels — TT2000 INT64_MIN renders as a 1677
// date and EPOCH -1e31 as a large number / NaN fallback. This mirrors the demo's
// existing value-preview behavior; canonical sentinel display (9999-12-31) lives
// in the C++ repr/ISO path, not here.
// ns-since-1970 (BigInt, leap-second corrected) -> ISO 8601 with ns precision.
export function nsToISO(ns) {
    const NS_PER_MS = 1000000n;
    let ms = ns / NS_PER_MS;
    let rem = ns - ms * NS_PER_MS;
    if (rem < 0n) { rem += NS_PER_MS; ms -= 1n; }
    const d = new Date(Number(ms));
    if (Number.isNaN(d.getTime())) return ns.toString();
    return d.toISOString().replace("Z", "") + rem.toString().padStart(6, "0") + "Z";
}

// Drop trailing null padding then trailing whitespace, without regex.
export function stripPadding(s) {
    let end = s.length;
    while (end > 0 && s.codePointAt(end - 1) === 0) end--;
    return s.slice(0, end).trimEnd();
}

// Split a flat values array into one row per record. recLen<=0 -> one row.
export function chunkRecords(arr, recLen) {
    if (!recLen || recLen <= 0) return [Array.from(arr)];
    const out = [];
    for (let i = 0; i < arr.length; i += recLen)
        out.push(Array.from(arr.slice(i, i + recLen)));
    return out;
}

// Decode a CDF_CHAR/UCHAR byte buffer into one string per record (last shape dim
// is the fixed string length). Only the first `max` records are decoded; `total`
// reports the full record count.
export function decodeChars(bytes, shape, max) {
    const dec = new TextDecoder("utf-8", { fatal: false });
    const strLen = shape.length ? shape[shape.length - 1] : 0;
    if (strLen > 0 && bytes.length >= strLen && bytes.length % strLen === 0) {
        const total = bytes.length / strLen;
        const n = Math.min(max ?? total, total);
        const strings = [];
        for (let i = 0; i < n; i++)
            strings.push(stripPadding(dec.decode(bytes.subarray(i * strLen, (i + 1) * strLen))));
        return { strings, total };
    }
    return { strings: [stripPadding(dec.decode(bytes))], total: 1 };
}

export function esc(s) {
    return String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// Render one attribute value: string -> quoted/trimmed, array/typed-array ->
// comma list, anything else (scalar) -> as-is.
export function formatAttrValue(value) {
    if (typeof value === "string") return `"${value.trim()}"`;
    if (Array.isArray(value) || ArrayBuffer.isView(value))
        return Array.from(value).map(String).join(", ");
    return value;
}

// --- DOM rendering ------------------------------------------------------------

const GROUP_LABELS = { data: "Data", support_data: "Support", metadata: "Metadata" };

function varRow(v, selected, onSelect) {
    const row = document.createElement("div");
    row.className = "var-row" + (v.name === selected ? " sel" : "");
    row.dataset.name = v.name;
    row.innerHTML =
        `<span class="log-key">${esc(v.name)}</span> ` +
        `<span class="log-dim">[${esc(v.shape.join(", "))}]</span> ` +
        `<span class="log-val">${esc(v.typeName)}</span>` +
        (v.isNrv ? `<span class="log-dim"> nrv</span>` : "");
    row.addEventListener("click", () => onSelect(v.name));
    return row;
}

function group(title, count, children, startOpen) {
    const wrap = document.createElement("div");
    wrap.className = "group";
    const head = document.createElement("div");
    head.className = "group-head";
    head.innerHTML = `<span class="caret">${startOpen ? "▾" : "▸"}</span> ` +
        `<span class="group-title">${esc(title)}</span> <span class="log-dim">(${count})</span>`;
    const body = document.createElement("div");
    body.className = "group-body";
    body.style.display = startOpen ? "block" : "none";
    for (const c of children) body.appendChild(c);
    head.addEventListener("click", () => {
        const open = body.style.display === "none";
        body.style.display = open ? "block" : "none";
        head.querySelector(".caret").textContent = open ? "▾" : "▸";
    });
    wrap.append(head, body);
    return wrap;
}

// Render the left list. container is cleared and repopulated.
export function renderList(container, model, { selected, onSelect }) {
    container.innerHTML = "";

    const gAttrRows = model.globalAttributes.map(a => {
        const row = document.createElement("div");
        row.className = "attr-row";
        row.innerHTML = `<span class="log-key">${esc(a.name)}</span> ` +
            `<span class="log-dim">${esc(a.entries.map(entryText).join(", "))}</span>`;
        return row;
    });
    if (gAttrRows.length)
        container.appendChild(group("Global Attributes", gAttrRows.length, gAttrRows, false));

    for (const g of ["data", "support_data", "metadata"]) {
        const vars = model.groups[g];
        if (!vars.length) continue;
        const rows = vars.map(v => varRow(v, selected, onSelect));
        container.appendChild(group(GROUP_LABELS[g], vars.length, rows, g === "data"));
    }

    if (!container.children.length)
        container.innerHTML = `<div class="log-dim" style="padding:0.5rem">No matches</div>`;
}

// Move the selection highlight without rebuilding the list, so manually-expanded
// groups keep their open/closed state when a variable is selected.
export function setSelected(container, name) {
    for (const row of container.querySelectorAll(".var-row"))
        row.classList.toggle("sel", row.dataset.name === name);
}

// Record length = product of non-record dims (shape[1:]); 1 for 0/1-D vars.
function recordLength(shape) {
    if (shape.length <= 1) return 1;
    return shape.slice(1).reduce((a, b) => a * b, 1);
}

// Value-preview gate: the table reads the WHOLE variable (copy_values) and joins
// every record's elements, so high-rank (e.g. 4-5D particle distributions) or huge
// variables would crash / produce unreadable rows. Decided from shape alone, before
// any values are read.
export const MAX_PREVIEW_DIMENSIONS = 2;     // max shape rank (record dim + 1)
export const MAX_PREVIEW_POINTS = 2_000_000; // max total elements (product of shape)

export function previewability(shape) {
    if (shape && shape.length > MAX_PREVIEW_DIMENSIONS)
        return { ok: false, reason: `${shape.length}-dimensional data — value preview not shown` };
    const total = shape && shape.length ? shape.reduce((a, b) => a * b, 1) : 0;
    if (total > MAX_PREVIEW_POINTS)
        return { ok: false, reason: `too large to preview (${total.toLocaleString()} values)` };
    return { ok: true };
}

const PREVIEW_RECORDS = 20;

function previewTable(cdf, v) {
    const table = document.createElement("table");
    table.className = "preview";

    if (isTimeType(v.type)) {
        const ns = cdf.time_values_as_ns_since_1970(v.name);
        const total = ns ? ns.length : 0;
        const n = Math.min(PREVIEW_RECORDS, total);
        for (let i = 0; i < n; i++) {
            const tr = table.insertRow();
            tr.insertCell().textContent = i;
            tr.insertCell().textContent = nsToISO(ns[i]);
        }
        return { table, total, shown: n };
    }

    if (isCharType(v.type) && v.copy_values !== undefined) {
        const { strings, total } = decodeChars(v.copy_values, v.shape, PREVIEW_RECORDS);
        strings.forEach((s, i) => {
            const tr = table.insertRow();
            tr.insertCell().textContent = i;
            tr.insertCell().textContent = s;
        });
        return { table, total, shown: strings.length };
    }

    const prev = previewability(v.shape);
    if (!prev.ok) return { table: null, total: v.shape[0] ?? 0, shown: 0, reason: prev.reason };

    const values = v.copy_values;
    if (values === undefined || values.length === 0) return { table, total: 0, shown: 0 };
    const recLen = recordLength(v.shape);
    const records = chunkRecords(values, recLen);
    const total = records.length;
    const n = Math.min(PREVIEW_RECORDS, total);
    for (let i = 0; i < n; i++) {
        const tr = table.insertRow();
        tr.insertCell().textContent = i;
        tr.insertCell().textContent = records[i].join(", ");
    }
    return { table, total, shown: n };
}

function sectionLabel(text) {
    const el = document.createElement("div");
    el.className = "section-label";
    el.textContent = text;
    return el;
}

// Render the right detail panel for variable `name`. Re-fetches the variable so
// values are read immediately (avoids holding stale views across heap growth).
export function renderDetail(container, cdf, name) {
    container.innerHTML = "";
    if (!cdf) return;
    const v = cdf.get_variable(name);
    if (v === undefined) {
        container.innerHTML = `<div class="log-err">Variable not found: ${esc(name)}</div>`;
        return;
    }

    const head = document.createElement("div");
    head.className = "detail-title";
    head.innerHTML = `<span class="log-key">${esc(name)}</span> ` +
        `<span class="log-dim">[${esc(Array.from(v.shape).join(", "))}]</span> ` +
        `<span class="log-val">${esc(v.type_name)}</span>` +
        (v.is_nrv ? `<span class="log-dim"> nrv</span>` : "");
    container.appendChild(head);

    // Plot-first: an empty mount right under the title. app.js fills it via plot.js
    // (kept out of render.js so render.js stays free of the uPlot import and its
    // pure Node test is unaffected).
    const plotPanel = document.createElement("div");
    plotPanel.className = "plot-panel";
    container.appendChild(plotPanel);

    const meta = document.createElement("div");
    meta.className = "log-dim";
    meta.textContent = `compression: ${v.compression}`;
    container.appendChild(meta);

    const attrs = document.createElement("div");
    attrs.className = "detail-attrs";
    for (const an of v.attribute_names) {
        const row = document.createElement("div");
        row.innerHTML = `<span class="log-dim">${esc(an)}:</span> ${esc(formatAttrValue(v.attributes[an]))}`;
        attrs.appendChild(row);
    }
    container.append(sectionLabel("Attributes"), attrs);

    const { table, total, shown, reason } = previewTable(cdf, v);
    if (reason) {
        container.appendChild(sectionLabel(`Values (${total} records)`));
        const skipped = document.createElement("div");
        skipped.className = "plot-note";
        skipped.textContent = reason;
        container.appendChild(skipped);
    } else {
        container.appendChild(sectionLabel(`Values (showing ${shown} of ${total})`));
        container.appendChild(table);
    }
}
