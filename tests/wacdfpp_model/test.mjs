// Pure Node test for wacdfpp/cdf-model.js — no WASM required.
//   node test.mjs
import {
    VAR_GROUPS, normalizeVarType, buildModel, filterModel, entryText,
} from "../../wacdfpp/cdf-model.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

check("VAR_GROUPS order", eq(VAR_GROUPS, ["data", "support_data", "metadata"]));
check("normalizeVarType default", normalizeVarType(undefined) === "data");
check("normalizeVarType case-insensitive", normalizeVarType("Support_Data") === "support_data");
check("normalizeVarType unknown -> data", normalizeVarType("weird") === "data");

const rawVars = [
    { name: "Epoch", shape: [3], typeName: "CDF_EPOCH", type: 31, isNrv: false,
      varType: "support_data", attributes: { VAR_TYPE: "support_data", UNITS: "ns" } },
    { name: "B_gse", shape: [3, 3], typeName: "CDF_FLOAT", type: 21, isNrv: false,
      varType: "data", attributes: { VAR_TYPE: "data", UNITS: "nT", DEPEND_0: "Epoch" } },
    { name: "labl", shape: [3], typeName: "CDF_CHAR", type: 51, isNrv: true,
      varType: "metadata", attributes: { VAR_TYPE: "metadata" } },
    { name: "noType", shape: [1], typeName: "CDF_INT4", type: 4, isNrv: false,
      varType: undefined, attributes: {} },
];
const rawGlobals = [
    { name: "Project", entries: ["THEMIS"], types: [51] },
    { name: "Discipline", entries: ["Space Physics"], types: [51] },
];
const model = buildModel(rawVars, rawGlobals);

check("data group holds B_gse + untyped noType",
    eq(model.groups.data.map(v => v.name), ["B_gse", "noType"]));
check("support_data group holds Epoch",
    eq(model.groups.support_data.map(v => v.name), ["Epoch"]));
check("metadata group holds labl",
    eq(model.groups.metadata.map(v => v.name), ["labl"]));
check("globalAttributes passed through", eq(model.globalAttributes, rawGlobals));

check("filter empty returns same model", filterModel(model, "  ") === model);

const byName = filterModel(model, "b_gse");
check("filter by var name", eq(byName.groups.data.map(v => v.name), ["B_gse"]));
check("filter by var name empties others",
    byName.groups.support_data.length === 0 && byName.groups.metadata.length === 0);

const byAttrValue = filterModel(model, "nT");
check("filter by attr value matches B_gse", eq(byAttrValue.groups.data.map(v => v.name), ["B_gse"]));
check("filter by attr value case-insensitive",
    filterModel(model, "NT").groups.data.map(v => v.name).join() === "B_gse");

const byAttrName = filterModel(model, "depend_0");
check("filter by attr name matches B_gse", eq(byAttrName.groups.data.map(v => v.name), ["B_gse"]));

const byGlobal = filterModel(model, "themis");
check("filter narrows global attributes",
    eq(byGlobal.globalAttributes.map(a => a.name), ["Project"]));

const noMatch = filterModel(model, "zzzzz");
check("no match empties all var groups",
    noMatch.groups.data.length === 0 && noMatch.groups.support_data.length === 0 &&
    noMatch.groups.metadata.length === 0);

check("entryText trims strings", entryText("  THEMIS  ") === "THEMIS");
check("entryText joins numeric array", entryText(new Float64Array([1, 2, 3])) === "1, 2, 3");
check("entryText joins string array", entryText(["a", "b"]) === "a, b");
check("entryText scalar", entryText(42) === "42");
check("entryText nullish -> empty", entryText(undefined) === "" && entryText(null) === "");

process.exit(failures ? 1 : 0);
