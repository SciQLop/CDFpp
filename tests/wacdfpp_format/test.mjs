// Pure Node test for wacdfpp/render.js formatters — no WASM, no DOM.
//   node test.mjs
import { nsToISO, stripPadding, chunkRecords, decodeChars, formatAttrValue } from "../../wacdfpp/render.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

check("nsToISO whole second",
    nsToISO(1700000000000000000n) === "2023-11-14T22:13:20.000000000Z");
check("nsToISO sub-ms precision",
    nsToISO(1700000000123456789n) === "2023-11-14T22:13:20.123456789Z");

check("stripPadding trims nulls then space",
    stripPadding("hello    ") === "hello");
check("stripPadding empty", stripPadding("  ") === "");

check("chunkRecords 2x3", eq(chunkRecords([1, 2, 3, 4, 5, 6], 3), [[1, 2, 3], [4, 5, 6]]));
check("chunkRecords recLen<=0 -> single row", eq(chunkRecords([1, 2, 3], 0), [[1, 2, 3]]));
check("chunkRecords ragged tail kept", eq(chunkRecords([1, 2, 3, 4, 5], 2), [[1, 2], [3, 4], [5]]));

check("formatAttrValue string quotes+trims", formatAttrValue("  hi  ") === '"hi"');
check("formatAttrValue array joins", formatAttrValue([1, 2, 3]) === "1, 2, 3");
check("formatAttrValue scalar passthrough", formatAttrValue(42) === 42);
check("formatAttrValue typed array spaced",
    formatAttrValue(new Float64Array([1, 2, 3])) === "1, 2, 3");

const twoRec = new Uint8Array([72, 101, 108, 108, 111, 87, 111, 114, 108, 100]); // "Hello","World"
check("decodeChars 2 records", eq(decodeChars(twoRec, [5]).strings, ["Hello", "World"]));
check("decodeChars total", decodeChars(twoRec, [5]).total === 2);
check("decodeChars max clamp", decodeChars(twoRec, [5], 1).strings.length === 1);
const padded = new Uint8Array([72, 101, 0, 0, 0, 87, 0, 0, 0, 0]); // "He","W" null-padded
check("decodeChars strips null padding", eq(decodeChars(padded, [5]).strings, ["He", "W"]));

process.exit(failures ? 1 : 0);
