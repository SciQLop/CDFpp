// Pure Node test for wacdfpp/presets.js.
//   node test.mjs
import { PRESETS, presetHref } from "../../wacdfpp/presets.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

const link = new URLSearchParams(presetHref({ url: "https://h/f.cdf?file=a&b=1", variable: "B" }));
check("presetHref keeps the whole file URL", link.get("url") === "https://h/f.cdf?file=a&b=1");
check("presetHref opens the variable", link.get("var") === "B");
check("presetHref without a variable", !new URLSearchParams(presetHref({ url: "u" })).has("var"));

check("presets exist", PRESETS.length > 0);
for (const p of PRESETS)
    check(`preset ${p.title} is complete and https`,
        ["title", "description", "url", "variable", "size"].every((k) => typeof p[k] === "string" && p[k])
        && p.url.startsWith("https://"));
check("preset titles are unique", new Set(PRESETS.map((p) => p.title)).size === PRESETS.length);

process.exit(failures ? 1 : 0);
