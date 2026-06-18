// Pure Node test for wacdfpp/astralint.js (URL builder only; window.open is DOM).
import { astralintUrl, ASTRALINT_URL } from "../../wacdfpp/astralint.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

const u = astralintUrl("https://example.com/a b/data.cdf?x=1");
check("base is AstraLint", u.startsWith(ASTRALINT_URL));
check("round-trips the cdf url", new URL(u).searchParams.get("url") === "https://example.com/a b/data.cdf?x=1");
check("no raw space in the produced url (encoded)", !u.includes(" "));
check("default suite ISTP", new URL(u).searchParams.get("suite") === "ISTP");
check("explicit suite honored",
    new URL(astralintUrl("https://x/y.cdf", "PDS4")).searchParams.get("suite") === "PDS4");

process.exit(failures ? 1 : 0);
