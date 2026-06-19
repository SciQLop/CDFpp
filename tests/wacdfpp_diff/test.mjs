// Pure Node test for wacdfpp/cdf-diff.js — no WASM required.
//   node test.mjs
import { buildModel } from "../../wacdfpp/cdf-model.js";
import { diffModels, diffSummary } from "../../wacdfpp/cdf-diff.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}

const V = (name, over = {}) => ({
    name, shape: [3], typeName: "CDF_FLOAT", type: 21, isNrv: false,
    varType: "data", attributes: { VAR_TYPE: "data" }, ...over,
});

// Identical models -> every variable "same", no field/attr diffs.
{
    const a = buildModel([V("B")], []);
    const b = buildModel([V("B")], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    check("identical var -> same", row.status === "same");
    check("identical var -> no field diffs", row.fields.length === 0);
    check("identical var -> no attr diffs", row.attributes.length === 0);
}

// Added / removed variables.
{
    const a = buildModel([V("only_a")], []);
    const b = buildModel([V("only_b")], []);
    const d = diffModels(a, b);
    check("removed var", d.groups.data.find(v => v.name === "only_a").status === "removed");
    check("added var", d.groups.data.find(v => v.name === "only_b").status === "added");
}

// Changed shape / type / isNrv.
{
    const a = buildModel([V("B")], []);
    const b = buildModel([V("B", { shape: [4], typeName: "CDF_DOUBLE", type: 22, isNrv: true })], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    const fieldNames = row.fields.map(f => f.field).sort();
    check("changed var -> changed status", row.status === "changed");
    check("changed shape/type/isNrv fields", fieldNames.join() === "isNrv,shape,type");
    const shape = row.fields.find(f => f.field === "shape");
    check("shape field old->new", shape.a === "3" && shape.b === "4");
}

// Per-variable attribute add / remove / change.
{
    const a = buildModel([V("B", { attributes: { VAR_TYPE: "data", UNITS: "nT", FILLVAL: "-1e31" } })], []);
    const b = buildModel([V("B", { attributes: { VAR_TYPE: "data", UNITS: "T", SCALE: "log" } })], []);
    const d = diffModels(a, b);
    const row = d.groups.data.find(v => v.name === "B");
    const byName = Object.fromEntries(row.attributes.map(x => [x.name, x]));
    check("attr changed (UNITS)", byName.UNITS.status === "changed" && byName.UNITS.a === "nT" && byName.UNITS.b === "T");
    check("attr removed (FILLVAL)", byName.FILLVAL.status === "removed" && byName.FILLVAL.b === null);
    check("attr added (SCALE)", byName.SCALE.status === "added" && byName.SCALE.a === null);
    check("unchanged attr not listed (VAR_TYPE absent)", byName.VAR_TYPE === undefined);
}

// Variable placed by effective group (uses B's group; here VAR_TYPE support_data).
{
    const a = buildModel([V("E", { varType: "support_data", attributes: { VAR_TYPE: "support_data" } })], []);
    const b = buildModel([V("E", { varType: "support_data", attributes: { VAR_TYPE: "support_data" } })], []);
    const d = diffModels(a, b);
    check("grouped under support_data", d.groups.support_data.some(v => v.name === "E"));
}

// Global attributes: added / removed / per-entry changed, index-aligned.
{
    const ga = [
        { name: "Project", entries: ["THEMIS"], types: [51] },
        { name: "TimeRes", entries: ["1s", "3s"], types: [51, 51] },
        { name: "OnlyA", entries: ["x"], types: [51] },
    ];
    const gb = [
        { name: "Project", entries: ["THEMIS"], types: [51] },           // same
        { name: "TimeRes", entries: ["1s", "5s", "10s"], types: [51, 51, 51] }, // entry 1 changed, entry 2 added
        { name: "OnlyB", entries: ["y"], types: [51] },                  // added
    ];
    const d = diffModels(buildModel([], ga), buildModel([], gb));
    const by = Object.fromEntries(d.globalAttributes.map(a => [a.name, a]));
    check("global same attr", by.Project.status === "same" && by.Project.entries.length === 0);
    check("global removed attr", by.OnlyA.status === "removed");
    check("global added attr", by.OnlyB.status === "added");
    check("global changed attr", by.TimeRes.status === "changed");
    const e = Object.fromEntries(by.TimeRes.entries.map(x => [x.index, x]));
    check("entry 1 changed 3s->5s", e[1].status === "changed" && e[1].a === "3s" && e[1].b === "5s");
    check("entry 2 added", e[2].status === "added" && e[2].a === null && e[2].b === "10s");
    check("entry 0 unchanged not listed", e[0] === undefined);
}

// Summary counts across globals + variables.
{
    const a = buildModel([V("keep"), V("gone")], [{ name: "A", entries: ["1"], types: [51] }]);
    const b = buildModel([V("keep", { shape: [9] }), V("new")], [{ name: "B", entries: ["2"], types: [51] }]);
    const s = diffSummary(diffModels(a, b));
    check("summary added (new var + B attr)", s.added === 2);
    check("summary removed (gone var + A attr)", s.removed === 2);
    check("summary changed (keep var shape)", s.changed === 1);
}

process.exit(failures ? 1 : 0);
