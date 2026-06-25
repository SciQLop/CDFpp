// Pure Node test for wacdfpp/cdf-yaml.js — no WASM required.
//   node test.mjs
import { toYaml, buildSkeleton, CDF_TYPE_NAMES } from "../../wacdfpp/cdf-yaml.js";

let failures = 0;
function check(name, ok) {
    if (ok) console.log(`ok   ${name}`);
    else { console.error(`FAIL ${name}`); failures += 1; }
}
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// --- toYaml: scalars ----------------------------------------------------
check("string plain", toYaml({ a: "hello" }) === "a: hello\n");
check("string with space stays plain", toYaml({ a: "hello world" }) === "a: hello world\n");
check("number-like string quoted", toYaml({ a: "123" }) === 'a: "123"\n');
check("colon string quoted", toYaml({ a: "x: y" }) === 'a: "x: y"\n');
check("comma string quoted", toYaml({ a: "a,b" }) === 'a: "a,b"\n');
check("empty string quoted", toYaml({ a: "" }) === 'a: ""\n');
check("newline string escaped", toYaml({ a: "x\ny" }) === 'a: "x\\ny"\n');
check("reserved word quoted", toYaml({ a: "true" }) === 'a: "true"\n');
check("bool", toYaml({ a: true }) === "a: true\n");
check("null", toYaml({ a: null }) === "a: null\n");
check("integer", toYaml({ a: 42 }) === "a: 42\n");
check("nan -> .nan", toYaml({ a: NaN }) === "a: .nan\n");
check("inf -> .inf", toYaml({ a: Infinity }) === "a: .inf\n");
check("-inf -> -.inf", toYaml({ a: -Infinity }) === "a: -.inf\n");

// --- toYaml: arrays -----------------------------------------------------
check("scalar array -> flow", toYaml({ a: [1, 2, 3] }) === "a: [1, 2, 3]\n");
check("single-scalar array -> flow", toYaml({ a: ["mV"] }) === "a: [mV]\n");
check("empty array -> []", toYaml({ a: [] }) === "a: []\n");
check("typed array -> flow", toYaml({ a: new Float64Array([1, 2]) }) === "a: [1, 2]\n");
check("array-of-array -> block",
    toYaml({ a: [[1, 2], "x"] }) === "a:\n  - [1, 2]\n  - x\n");

// --- toYaml: maps -------------------------------------------------------
check("nested map", toYaml({ a: { b: 1 } }) === "a:\n  b: 1\n");
check("empty map -> {}", toYaml({ a: {} }) === "a: {}\n");

// --- toYaml: escaping must produce round-trippable YAML -----------------
// (keys are emitted verbatim historically; values with leading YAML
// indicators or YAML-1.1 number forms must be quoted or they re-parse wrong.)
check("special key quoted", toYaml({ "min: max": 1 }) === '"min: max": 1\n');
check("leading-dash value quoted", toYaml({ a: "- pending" }) === 'a: "- pending"\n');
check("leading-question value quoted", toYaml({ a: "? x" }) === 'a: "? x"\n');
check("underscore-number string quoted", toYaml({ a: "1_000" }) === 'a: "1_000"\n');
check("digit-leading string quoted", toYaml({ a: "3things" }) === 'a: "3things"\n');
check("tilde string quoted", toYaml({ a: "~" }) === 'a: "~"\n');
check("bare dash quoted", toYaml({ a: "-" }) === 'a: "-"\n');

// --- CDF_TYPE_NAMES -----------------------------------------------------
check("type map double", CDF_TYPE_NAMES[45] === "CDF_DOUBLE");
check("type map tt2000", CDF_TYPE_NAMES[33] === "CDF_TIME_TT2000");
check("type map char", CDF_TYPE_NAMES[51] === "CDF_CHAR");

// --- buildSkeleton ------------------------------------------------------
const rawVars = [{
    name: "v",
    typeName: "CDF_FLOAT",
    shape: [3],
    isNrv: false,
    compression: "none",
    attributes: { units: "mV", scale: new Int32Array([1, 2, 3]) },
    attributeTypes: { units: 51, scale: 4 },
}];
const rawGlobals = [{ name: "Project", entries: ["THEMIS", [1, 2]], types: [51, 11] }];
const skel = buildSkeleton(rawVars, rawGlobals, { compression: "gzip" });

check("top-level compression", skel.compression === "gzip");
check("top-level key order", eq(Object.keys(skel), ["compression", "attributes", "variables"]));
check("global attr values pass-through", eq(skel.attributes.Project.values, ["THEMIS", [1, 2]]));
check("global attr type names", eq(skel.attributes.Project.types, ["CDF_CHAR", "CDF_UINT1"]));

const v = skel.variables.v;
check("var key order", eq(Object.keys(v), ["type", "shape", "is_nrv", "compression", "attributes"]));
check("var type name", v.type === "CDF_FLOAT");
check("var shape", eq(v.shape, [3]));
check("var is_nrv", v.is_nrv === false);
check("var compression", v.compression === "none");
check("var attr units single-entry values", eq(v.attributes.units.values, ["mV"]));
check("var attr units type", eq(v.attributes.units.types, ["CDF_CHAR"]));
check("var attr scale wrapped single entry",
    v.attributes.scale.values.length === 1 && eq(Array.from(v.attributes.scale.values[0]), [1, 2, 3]));
check("var attr scale type", eq(v.attributes.scale.types, ["CDF_INT4"]));

// --- end to end ---------------------------------------------------------
const yaml = toYaml(skel);
check("e2e has variables block", yaml.includes("variables:\n"));
check("e2e var name indented", yaml.includes("\n  v:\n"));
check("e2e renders var type name", yaml.includes("type: CDF_FLOAT\n"));
check("e2e renders global char type", yaml.includes("CDF_CHAR"));

process.exit(failures === 0 ? 0 : 1);
