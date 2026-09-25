// Live Python playground: a "Run" button on every Python example of the docs.
// Runs in the reader's browser with Pyodide (Python compiled to WebAssembly), loaded on
// the first click only. All blocks of a page share one Python session, like notebook
// cells; running a block first runs the earlier blocks of the page that haven't run yet.
"use strict";

const PYODIDE_URL = "https://cdn.jsdelivr.net/pyodide/v314.0.7/full/";

// The pycdfpp version these docs describe; the latest release when it isn't on PyPI yet.
// Sphinx declares DOCUMENTATION_OPTIONS with a top-level const: a global name, but not a
// property of globalThis.
const DOCS_VERSION = typeof DOCUMENTATION_OPTIONS === "undefined" ? "" : DOCUMENTATION_OPTIONS.VERSION;

const SETUP = `
import micropip
_playground_note = ""
try:
    await micropip.install("pycdfpp==${DOCS_VERSION}" if "${DOCS_VERSION}" else "pycdfpp")
except Exception:
    await micropip.install("pycdfpp")
    import pycdfpp
    _playground_note = (f"Note: pycdfpp ${DOCS_VERSION} isn't on PyPI yet, running "
                        f"{pycdfpp.__version__}: some examples may fail.")
await micropip.install("pyodide-http")
import pyodide_http
pyodide_http.patch_all()
import warnings
warnings.filterwarnings("ignore", message="FigureCanvasAgg is non-interactive")
`;

// Figures are drawn off-screen (Agg) and shown as PNG images under the block.
const COLLECT_FIGURES = `
def _playground_figures():
    import sys
    if "matplotlib.pyplot" not in sys.modules:
        return []
    import base64, io
    import matplotlib.pyplot as plt
    images = []
    for number in plt.get_fignums():
        buffer = io.BytesIO()
        plt.figure(number).savefig(buffer, format="png", bbox_inches="tight")
        images.append(base64.b64encode(buffer.getvalue()).decode())
    plt.close("all")
    return images
`;

let pyodideReady = null;
let setupNote = "";  // shown once, in the first output

function loadScript(src) {
    return new Promise((resolve, reject) => {
        const script = document.createElement("script");
        script.src = src;
        script.onload = resolve;
        script.onerror = () => reject(new Error(`could not load ${src}`));
        document.head.appendChild(script);
    });
}

function loadPython() {
    pyodideReady ??= (async () => {
        await loadScript(PYODIDE_URL + "pyodide.js");
        const pyodide = await globalThis.loadPyodide({ indexURL: PYODIDE_URL, env: { MPLBACKEND: "Agg" } });
        await pyodide.loadPackage(["micropip", "numpy"]);
        await pyodide.runPythonAsync(SETUP);
        await pyodide.runPythonAsync(COLLECT_FIGURES);
        setupNote = pyodide.globals.get("_playground_note");
        return pyodide;
    })();
    return pyodideReady;
}

const codeOf = (block) => block.querySelector("pre").innerText;

function outputArea(block) {
    let out = block.nextElementSibling;
    if (!out?.classList.contains("playground-output")) {
        out = document.createElement("div");
        out.className = "playground-output";
        block.after(out);
    }
    out.replaceChildren();
    return out;
}

function appendText(out, text, cls) {
    const pre = document.createElement("pre");
    pre.className = cls;
    pre.textContent = text;
    out.appendChild(pre);
}

function appendFigures(out, images) {
    for (const png of images) {
        const img = document.createElement("img");
        img.src = `data:image/png;base64,${png}`;
        img.alt = "figure produced by the example";
        out.appendChild(img);
    }
}

async function execute(pyodide, block) {
    const out = outputArea(block);
    const lines = setupNote ? [setupNote] : [];
    setupNote = "";
    pyodide.setStdout({ batched: (s) => lines.push(s) });
    pyodide.setStderr({ batched: (s) => lines.push(s) });
    try {
        const code = codeOf(block);
        await pyodide.loadPackagesFromImports(code, { messageCallback: () => {} });
        const result = await pyodide.runPythonAsync(code);
        if (result !== undefined) lines.push(String(result));
        result?.destroy?.();
        if (lines.length) appendText(out, lines.join("\n"), "playground-stdout");
        appendFigures(out, pyodide.globals.get("_playground_figures")().toJs());
        block.dataset.ran = "true";
        return true;
    } catch (err) {
        if (lines.length) appendText(out, lines.join("\n"), "playground-stdout");
        appendText(out, String(err.message ?? err), "playground-error");
        return false;
    }
}

async function run(blocks, index, button) {
    const label = button.textContent;
    button.disabled = true;
    button.textContent = pyodideReady ? "Running…" : "Loading Python…";
    try {
        const pyodide = await loadPython();
        button.textContent = "Running…";
        for (const block of blocks.slice(0, index)) {
            if (block.dataset.ran !== "true" && !(await execute(pyodide, block))) return;
        }
        await execute(pyodide, blocks[index]);
    } catch (err) {
        appendText(outputArea(blocks[index]), `Could not start Python: ${err.message}`, "playground-error");
    } finally {
        button.disabled = false;
        button.textContent = label;
    }
}

function addRunButton(blocks, index) {
    const block = blocks[index];
    const pre = block.querySelector("pre");
    pre.contentEditable = "plaintext-only";
    pre.spellcheck = false;
    pre.addEventListener("input", () => { block.dataset.ran = "false"; });
    const button = document.createElement("button");
    button.className = "playground-run";
    button.type = "button";
    button.textContent = "▶ Run";
    button.title = "Run this example in your browser. You can edit the code first.";
    button.addEventListener("click", () => run(blocks, index, button));
    block.appendChild(button);
}

document.addEventListener("DOMContentLoaded", () => {
    const blocks = [...document.querySelectorAll("div.highlight-python:not(.no-playground)")];
    blocks.forEach((_, index) => addRunButton(blocks, index));
});
