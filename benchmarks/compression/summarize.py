"""Turns sweep.csv and end_to_end.csv into JSON summaries (stdout) for the report."""
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

KINDS = {
    "time": {"CDF_TIME_TT2000", "CDF_EPOCH", "CDF_EPOCH16"},
    "float": {"CDF_REAL4", "CDF_REAL8", "CDF_FLOAT", "CDF_DOUBLE"},
    "integer": {"CDF_INT1", "CDF_INT2", "CDF_INT4", "CDF_INT8", "CDF_UINT1", "CDF_UINT2", "CDF_UINT4",
                "CDF_BYTE"},
}


def kind_of(cdf_type: str) -> str:
    return next((kind for kind, types in KINDS.items() if cdf_type in types), "other")


def rows(path: Path) -> list[dict]:
    with path.open() as f:
        return list(csv.DictReader(f))


def totals(records, key) -> dict:
    """{key(record): {raw, compressed, compress_s, decompress_s}} summed over records."""
    acc = defaultdict(lambda: defaultdict(float))
    for r in records:
        bucket = acc[key(r)]
        bucket["raw"] += int(r["raw_bytes"])
        bucket["compressed"] += int(r["compressed_bytes"])
        bucket["compress_s"] += float(r["compress_s"])
        bucket["decompress_s"] += float(r["decompress_s"])
    return acc


def ratio_and_speed(t) -> dict:
    mib = t["raw"] / 2**20
    return {"ratio": round(t["raw"] / t["compressed"], 3),
            "compress_MiBps": round(mib / t["compress_s"], 1),
            "decompress_MiBps": round(mib / t["decompress_s"], 1)}


def sweep_summary(records) -> dict:
    overall = {config: ratio_and_speed(t) for config, t in totals(records, lambda r: r["config"]).items()}
    by_kind = defaultdict(dict)
    for (kind, config), t in totals(records, lambda r: (kind_of(r["cdf_type"]), r["config"])).items():
        by_kind[kind][config] = ratio_and_speed(t)["ratio"]
    by_source = defaultdict(dict)
    for (source, config), t in totals(records, lambda r: (r["source"], r["config"])).items():
        by_source[source][config] = ratio_and_speed(t)["ratio"]
    raw_by_kind = {kind: round(t["raw"] / 2**20, 1)
                   for (kind, config), t in totals(records, lambda r: (kind_of(r["cdf_type"]), r["config"])).items()
                   if config == "gzip6"}
    return {"overall": overall, "by_kind": by_kind, "by_source": by_source, "raw_MiB_by_kind": raw_by_kind,
            "variables": len({(r["file"], r["variable"]) for r in records})}


def end_to_end_summary(records) -> dict:
    per_file = defaultdict(dict)
    for r in records:
        entry = per_file[r["file"]]
        entry.update(source=r["source"], original_bytes=int(r["original_bytes"]),
                     original_compression=r["original_compression"])
        entry[r["codec"]] = {"bytes": int(r["bytes"]), "save_s": float(r["save_s"]), "load_s": float(r["load_s"])}
    codecs = sorted({r["codec"] for r in records})
    total = {codec: {field: round(sum(f[codec][field] for f in per_file.values()), 3)
                     for field in ("bytes", "save_s", "load_s")} for codec in codecs}
    total["original_bytes"] = sum(f["original_bytes"] for f in per_file.values())
    return {"per_file": per_file, "total": total}


if __name__ == "__main__":
    sweep_csv, e2e_csv = map(Path, sys.argv[1:3])
    summary = {"sweep": sweep_summary(rows(sweep_csv))}
    if e2e_csv.exists():
        summary["end_to_end"] = end_to_end_summary(rows(e2e_csv))
    json.dump(summary, sys.stdout, indent=1)
