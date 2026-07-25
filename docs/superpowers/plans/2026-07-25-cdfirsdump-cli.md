# cdfirsdump CLI (Phase A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a new `cdfirsdump` console script — a modern, double-dash-flag, semantically-parity replacement for NASA's own `cdfirsdump` tool — built on the existing byte-for-byte `nasa_compat_repr.hpp` engine.

**Architecture:** Extend `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp` in place (radix-aware offsets, an offset-starting entry point, a `-data` hex dump, and a new record-type summary table), bind the extended entry points into `pycdfpp/debug.hpp`, and add a new cyclopts-based `pycdfpp/cli_irsdump.py` console script.

**Tech Stack:** C++20, fmt, cpp_utils::serde, pybind11, Python cyclopts (`pycdfpp[cli]` extra), Catch2 v3, meson.

## Global Constraints

- C++20, WebKit-based `.clang-format` (100 cols, 4-space indent, Allman braces) — run `clang-format` on every touched C++ file before committing.
- Every behavioral claim about the real `cdfirsdump` tool in this plan is backed by a real captured run of `/var/home/jeandet/Downloads/cdf39_2-dist-main/bin/cdfirsdump` (v3.9.2_0) and/or its source at `/var/home/jeandet/Downloads/cdf39_2-dist-main/src/tools/cdfirsdump.c` — never hand-derive new NASA-compat formatting from memory; re-run the real tool if a new case comes up (see `reference-nasa-cdf-toolkit` project memory for the toolkit path).
- New/changed public C++ entry points must keep every existing call site source-compatible (trailing defaulted parameters only — `tests/nasa_compat_repr/main.cpp`'s existing `SCENARIO`s and the existing `nasa_compat_dump` Python binding must keep compiling/passing unmodified).
- This project's CI does **not** auto-discover tests: any new test *file* must be added to both `tests/meson.build` and `.github/workflows/tests-with-sanitizers.yml`'s hand-enumerated list, or it silently gets zero coverage (confirmed gap fixed twice already in this feature area). This plan avoids that entirely for 5 of 6 tasks by extending existing, already-registered test files (`tests/nasa_compat_repr/main.cpp`, `tests/python_debug/test.py`); only Task 6 introduces a new test file and must register it.
- Flag syntax is modern `--flag` (cyclopts), never NASA's single-dash syntax — this is intentional, approved scope (see the design spec), not an oversight to "fix."
- Skip the `full_corpus` test when running the suite locally (network fixtures) — `ninja test -C build --print-errorlogs` and check the summary rather than grepping for one test's output.

---

### Task 1: `dump_options` (radix) threading through `nasa_compat_repr.hpp`

**Files:**
- Modify: `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`
- Test: `tests/nasa_compat_repr/main.cpp` (extend, already registered in `tests/meson.build`/CI)
- Fixture (already added, see below): `tests/resources/rvariable_cdfirsdump_hex_reference.txt`

**Interfaces:**
- Produces: `struct cdf::io::debug::nasa::dump_options { int radix = 10; bool show_data = false; };`
- Produces: `nasa_offset(int64_t value, const dump_options& opts = {})` — radix-aware (was `nasa_offset(int64_t value)`)
- Produces: every `print_nasa(...)` overload gains a trailing `const dump_options& opts = {}` parameter (full list below) — existing call sites that omit it keep compiling and keep producing identical (`radix=10`) output.
- Consumes: nothing new from other tasks.

**Fixture already prepared (verified against the real tool this session — do not regenerate):**
`tests/resources/rvariable_cdfirsdump_hex_reference.txt` is a real, captured `cdfirsdump -full -nopage -nosummary -16 rvariable` run (NASA's hex-radix flag is the bare numeral `-16`, **not** `-radix 16` or `-radix=16` — both of those error with "Unknown qualifier"; confirmed against the real binary and cross-checked in `src/tools/cdfirsdump.c:179` — `-16`/`-10` are qualifier names `HEXAqual`/`DECIqual`), with the CLI-level `Dumping "..."` line already stripped (matching what `dump()` itself produces — it never prints that line, see the existing comment on `dump()`).

- [ ] **Step 1: Write the failing radix-formatting test**

Add to `tests/nasa_compat_repr/main.cpp`, right after the existing `SCENARIO("nasa_offset formats file offsets like cdfirsdump's Deci64 (%020lld)", ...)`:

```cpp
SCENARIO("nasa_offset supports cdfirsdump's hex radix (real flag: -16, not -radix)",
    "[nasa_compat_repr]")
{
    THEN("radix=10 (default) is unchanged")
    {
        REQUIRE(nasa_offset(320, dump_options { .radix = 10 }) == "00000000000000000320");
    }
    THEN("radix=16 renders 0x + 16 uppercase hex digits, two's-complement for negatives")
    {
        REQUIRE(nasa_offset(320, dump_options { .radix = 16 }) == "0x0000000000000140");
        REQUIRE(nasa_offset(-1, dump_options { .radix = 16 }) == "0xFFFFFFFFFFFFFFFF");
        REQUIRE(nasa_offset(0, dump_options { .radix = 16 }) == "0x0000000000000000");
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "nasa_offset supports cdfirsdump's hex radix*"`
Expected: compile error (`dump_options` doesn't exist yet) — that's the "fails" signal at this stage; proceed to Step 3.

- [ ] **Step 3: Add `dump_options` and make `nasa_offset` radix-aware**

In `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`, right after the `namespace cdf::io::debug::nasa {` opening (before `nasa_offset`), add:

```cpp
// A dump()'s-worth of formatting knobs threaded through every print_nasa overload.
// radix: 10 (default, cdfirsdump's own default) or 16 - the real tool's flags are the
// bare numerals -10/-16 (qualifier names DECIqual/HEXAqual in cdfirsdump.c), not a
// named -radix flag; this project's own CLI renames them to --radix {10,16} (modern
// flag syntax, see the Phase A design spec) but the underlying feature and its exact
// "0x%016llX" hex format are real, verified against a real capture.
struct dump_options
{
    int radix = 10;
    bool show_data = false;
};
```

Replace `nasa_offset`:

```cpp
inline std::string nasa_offset(int64_t value, const dump_options& opts = {})
{
    if (opts.radix == 16)
        return fmt::format("0x{:016X}", static_cast<uint64_t>(value));
    return fmt::format("{:020d}", value);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "nasa_offset supports cdfirsdump's hex radix*"`
Expected: PASS, 2 assertions.

- [ ] **Step 5: Commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
git add include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp
git commit -m "feat(debug): add dump_options with radix support to nasa_offset"
```

- [ ] **Step 6: Thread `opts` through `print_nasa_header` and the two shared VDR/AEDR body helpers**

Replace `print_nasa_header`:

```cpp
inline void print_nasa_header(std::ostream& os, std::int64_t record_size, std::size_t offset,
    cdf::cdf_record_type type, const dump_options& opts = {})
{
    os << '\n'
       << "RecordSize: " << record_size << " (@ "
       << nasa_offset(static_cast<int64_t>(offset), opts) << ")\n";
    os << "RecordType: " << static_cast<int32_t>(type) << " (" << cdf::cdf_record_type_str(type)
       << ")\n";
}
```

In `namespace details`, replace `print_nasa_aedr_body`'s signature/body (only the `AEDRnext` line changes):

```cpp
template <typename record_t>
inline void print_nasa_aedr_body(std::ostream& os, const record_t& r, const char* next_label,
    cdf_encoding encoding, const dump_options& opts = {})
{
    os << next_label << ": " << nasa_offset(static_cast<int64_t>(r.AEDRnext), opts) << '\n';
    os << "AttrNum: " << r.AttrNum << '\n';
    os << "DataType: " << static_cast<int32_t>(r.DataType) << " ("
       << nasa_data_type_name(r.DataType) << ")\n";
    os << "Num: " << r.Num << '\n';
    os << "NumElems: " << r.NumElements << '\n';
    os << (cdf::is_string(r.DataType) ? "NumStrings: " : "rfuA: ") << r.NumStrings << '\n';
    os << "rfuB: " << 0 << '\n';
    os << "rfuC: " << 0 << '\n';
    os << "rfuD: " << -1 << '\n';
    os << "rfuE: " << -1 << '\n';
    os << "Value: "
       << nasa_attribute_value_str(
              r.DataType, r.NumElements, encoding, r.Value.data(), r.Value.size())
       << '\n';
}
```

And `print_nasa_vdr_body` (the offset-bearing lines - `VDRnext`, `VXRhead`, `VXRtail`, `CPRorSPRoffset` - all gain `, opts`):

```cpp
template <typename record_t>
inline void print_nasa_vdr_body(std::ostream& os, const record_t& r, const char* next_label,
    cdf_encoding encoding, const dump_options& opts = {})
{
    (void)encoding;
    os << next_label << ": " << nasa_offset(static_cast<int64_t>(r.VDRnext), opts) << '\n';
    os << "DataType: " << static_cast<int32_t>(r.DataType) << " ("
       << nasa_data_type_name(r.DataType) << ")\n";
    os << "MaxRec: " << r.MaxRec << '\n';
    os << "VXRhead: " << nasa_offset(static_cast<int64_t>(r.VXRhead), opts) << '\n';
    os << "VXRtail: " << nasa_offset(static_cast<int64_t>(r.VXRtail), opts) << '\n';
    os << "Flags: " << nasa_vdr_flags_str(r.Flags) << '\n';
    os << "sRecords: " << r.SRecords << " (" << nasa_srecords_name(r.SRecords) << ")\n";
    os << "rfuB: " << 0 << '\n';
    os << "rfuC: " << -1 << '\n';
    os << "rfuF: " << -1 << '\n';
    os << "NumElems: " << r.NumElems << '\n';
    os << "Num: " << r.Num << '\n';
    os << "CPRorSPRoffset: " << nasa_offset(static_cast<int64_t>(r.CPRorSPRoffset), opts) << '\n';
    os << "BlockingFactor: " << r.BlockingFactor << '\n';
    os << "Name: \"" << r.Name.value << "\"\n";
}
```

(`print_nasa_vdr_dims` and `print_nasa_vdr_padvalue` print no offset fields — leave both untouched.)

- [ ] **Step 7: Thread `opts` through every top-level `print_nasa(...)` overload**

Apply this exact mechanical change to each of the 14 overloads below: append `, const dump_options& opts = {}` as the new final parameter, and change every `print_nasa_header(os, ..., type)` call inside it to `print_nasa_header(os, ..., type, opts)`. For the four VDR/AEDR-family ones, also append `, opts` to the shared-body-helper call.

| Record type | Old signature (last param shown) | Change |
|---|---|---|
| `cdf_CDR_t` | `..., const cdf::io::cdf_CDR_t<version_t>& r)` | append `opts`; `print_nasa_header(os, ..., cdf::cdf_record_type::CDR, opts)` |
| `cdf_GDR_t` | `..., const cdf::io::cdf_GDR_t<version_t>& r)` | same pattern, `cdf_record_type::GDR` |
| `cdf_ADR_t` | `..., const cdf::io::cdf_ADR_t<version_t>& r)` | same pattern, `cdf_record_type::ADR` |
| `cdf_AgrEDR_t` | `..., const cdf::io::cdf_AgrEDR_t<version_t>& r, cdf_encoding encoding)` | append `opts` after `encoding`; header call gets `opts`; `details::print_nasa_aedr_body(os, r, "AgrEDRnext", encoding, opts)` |
| `cdf_AzEDR_t` | `..., const cdf::io::cdf_AzEDR_t<version_t>& r, cdf_encoding encoding)` | same, `"AzEDRnext"` |
| `cdf_zVDR_t` | `..., const cdf::io::cdf_zVDR_t<version_t>& r, cdf_encoding encoding)` | append `opts`; header gets `opts`; `details::print_nasa_vdr_body(os, r, "zVDRnext", encoding, opts)`; `print_nasa_vdr_dims`/`print_nasa_vdr_padvalue` calls unchanged (no offsets) |
| `cdf_rVDR_t` | `..., const cdf::io::cdf_rVDR_t<version_t>& r, cdf_encoding encoding)` | same, `"rVDRnext"` |
| `cdf_VXR_t` | `..., const cdf::io::cdf_VXR_t<version_t>& r)` | append `opts`; header gets `opts`; **also** the `Offset[i]` column: change `nasa_offset(static_cast<int64_t>(r.Offset[i]))` to `nasa_offset(static_cast<int64_t>(r.Offset[i]), opts)` |
| `cdf_VVR_t` | `..., const cdf::io::cdf_VVR_t<version_t>& r)` | append `opts`; header gets `opts` (no other offset fields) |
| `cdf_CVVR_t` | `..., const cdf::io::cdf_CVVR_t<version_t>& r)` | append `opts`; header gets `opts` |
| `cdf_CPR_t` | `..., const cdf::io::cdf_CPR_t<version_t>& r)` | append `opts`; header gets `opts` |
| `cdf_CCR_t` | `..., const cdf::io::cdf_CCR_t<version_t>& r)` | append `opts`; header gets `opts`; **also** `CPRoffset` line: `nasa_offset(static_cast<int64_t>(r.CPRoffset), opts)` |
| `cdf_UIR_t` | `..., const cdf::io::cdf_UIR_t<version_t>& r)` | append `opts`; header gets `opts`; **also** `Next`/`Prev` lines both gain `, opts` |
| `undecoded_record_t` (SPR) | `..., const cdf::io::debug::undecoded_record_t& r)` | append `opts`; header gets `opts` |

- [ ] **Step 8: Update `dump()`'s internal dispatch lambda to accept and forward `opts`**

Replace `dump(std::ostream&, const std::string&)` and `dump(const std::string&)`:

```cpp
inline void dump(std::ostream& os, const std::string& path, const dump_options& opts = {})
{
    auto buffer = cdf::io::buffers::make_shared_file_adapter(path);
    print_nasa_magic_preamble(os, buffer);
    cdf_encoding encoding = cdf_encoding::network;
    for_each_record(buffer,
        [&](std::size_t offset, const auto& record)
        {
            if constexpr (requires { record.Encoding; })
                encoding = record.Encoding;
            if constexpr (requires { print_nasa(os, offset, record, encoding, opts); })
                print_nasa(os, offset, record, encoding, opts);
            else
                print_nasa(os, offset, record, opts);
        });
}

inline std::string dump(const std::string& path, const dump_options& opts = {})
{
    std::ostringstream oss;
    dump(oss, path, opts);
    return oss.str();
}
```

- [ ] **Step 9: Build and run the whole `nasa_compat_repr` suite to confirm no regressions**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr`
Expected: every existing `SCENARIO` (all default `opts={}`, radix=10 call sites) still passes byte-for-byte, plus the 2 new assertions from Step 1.

- [ ] **Step 10: Write the end-to-end hex-radix regression test**

Add to `tests/nasa_compat_repr/main.cpp`, after the existing `SCENARIO("dump() reproduces cdfirsdump -full -nopage -nosummary byte-for-byte, end to end", ...)`:

```cpp
SCENARIO("dump() with radix=16 reproduces a real hex-radix cdfirsdump capture, end to end",
    "[nasa_compat_repr]")
{
    GIVEN("a real captured -16 run of rvariable.cdf")
    {
        const std::string cdf_path = std::string(DATA_PATH) + "/rvariable.cdf";
        const std::string reference_path
            = std::string(DATA_PATH) + "/rvariable_cdfirsdump_hex_reference.txt";
        std::ifstream f { reference_path, std::ios::binary };
        REQUIRE(f.is_open());
        std::string reference { std::istreambuf_iterator<char> { f },
            std::istreambuf_iterator<char> {} };

        THEN("dump() with radix=16 matches it exactly")
        {
            REQUIRE(dump(cdf_path, dump_options { .radix = 16 }) == reference);
        }
    }
}
```

- [ ] **Step 11: Run test to verify it passes**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "dump() with radix=16*"`
Expected: PASS.

- [ ] **Step 12: `clang-format` and commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
clang-format -i include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp
git add include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp tests/resources/rvariable_cdfirsdump_hex_reference.txt
git commit -m "feat(debug): thread dump_options radix through every nasa_compat_repr print_nasa overload"
```

---

### Task 2: Offset-starting dump

**Files:**
- Modify: `include/cdfpp/cdf-io/debug/record_stream.hpp`
- Modify: `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`
- Test: `tests/nasa_compat_repr/main.cpp` (extend)
- Fixture (already added): `tests/resources/rvariable_cdfirsdump_offset_reference.txt`

**Interfaces:**
- Consumes: `dump_options` from Task 1.
- Produces: `for_each_record(buffer_t&&, std::size_t start_offset, on_record_t&&, on_corruption_t&& = {})` — new overload in `cdf::io::debug`.
- Produces: `cdf::io::debug::nasa::dump_from_offset(std::ostream&, const std::string& path, std::size_t start_offset, const dump_options& opts = {})` and the `std::string`-returning overload.

**Fixture already prepared:** `tests/resources/rvariable_cdfirsdump_offset_reference.txt` is a real captured `cdfirsdump -full -nopage -nosummary -offset 404 rvariable` run. Verified fact: at a non-zero `-offset`, the real tool prints **no** `Magic number` lines and **no** `Dumping "..."` line (both stripped/never-reached) — it goes straight from a single blank line into the first record's own header. The fixture already has that single leading blank line and nothing else before `RecordSize: 344 (@ 00000000000000000404)`.

- [ ] **Step 1: Write the failing test**

Add to `tests/nasa_compat_repr/main.cpp`:

```cpp
SCENARIO("dump_from_offset() starts the walk mid-file with no magic preamble, matching a real "
         "-offset capture",
    "[nasa_compat_repr]")
{
    GIVEN("a real captured -offset 404 run of rvariable.cdf")
    {
        const std::string cdf_path = std::string(DATA_PATH) + "/rvariable.cdf";
        const std::string reference_path
            = std::string(DATA_PATH) + "/rvariable_cdfirsdump_offset_reference.txt";
        std::ifstream f { reference_path, std::ios::binary };
        REQUIRE(f.is_open());
        std::string reference { std::istreambuf_iterator<char> { f },
            std::istreambuf_iterator<char> {} };

        THEN("dump_from_offset(path, 404) matches it exactly")
        {
            REQUIRE(dump_from_offset(cdf_path, 404UL) == reference);
        }
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ninja -C build test-nasa_compat_repr`
Expected: compile error (`dump_from_offset` / the `start_offset`-taking `for_each_record` overload don't exist yet).

- [ ] **Step 3: Add the `start_offset`-taking `for_each_record` overload to `record_stream.hpp`**

In `include/cdfpp/cdf-io/debug/record_stream.hpp`, inside `namespace details`, change `run_compressed` and `run` to thread a `start_offset` parameter through (replacing the hardcoded `8UL`):

```cpp
template <typename version_t, typename buffer_t, typename on_record_t, typename on_corruption_t>
void run_compressed(buffer_t& buffer, std::size_t start_offset, on_record_t&& on_record,
    on_corruption_t&& on_corruption)
{
    cdf_CCR_t<version_t> ccr {};
    load_record(ccr, buffer, 8);
    cdf_CPR_t<version_t> cpr {};
    load_record(cpr, buffer, ccr.CPRoffset);
    on_record(8UL, ccr);
    on_record(static_cast<std::size_t>(ccr.CPRoffset), cpr);
    no_init_vector<char> data(8UL + ccr.uSize);
    buffer.read(data.data(), 0, 8);
    decompression::inflate(cpr.cType, ccr.data, data.data() + 8UL, std::size(data) - 8UL);
    auto decompressed = buffers::make_shared_array_adapter(std::move(data));
    for_each_record(decompressed, version_t {}, start_offset, decompressed.size(),
        std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
}

template <typename version_t, typename buffer_t, typename on_record_t, typename on_corruption_t>
void run(buffer_t& buffer, bool is_compressed, std::size_t start_offset, on_record_t&& on_record,
    on_corruption_t&& on_corruption)
{
    if (is_compressed)
        run_compressed<version_t>(buffer, start_offset, std::forward<on_record_t>(on_record),
            std::forward<on_corruption_t>(on_corruption));
    else
        for_each_record(buffer, version_t {}, start_offset, buffer.size(),
            std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
}
```

Then add a new top-level overload right before the existing `for_each_record(buffer_t&&, on_record_t&&, on_corruption_t&& = {})` (which becomes a thin wrapper calling this one with `8UL`):

```cpp
// Same auto-detecting entry point as the 2-arg overload below, but starts the walk at an
// arbitrary byte offset instead of right after the 8-byte magic. For a compressed file,
// start_offset is interpreted in the *decompressed* reconstructed buffer's own offset space
// (the same space GDRoffset/ADRhead/... already point into - see run_compressed's own
// comment) - the CCR/CPR themselves are still always reported first regardless of
// start_offset, since they're needed to even decompress the rest.
template <typename buffer_t, typename on_record_t,
    typename on_corruption_t = default_corruption_handler>
void for_each_record(buffer_t&& buffer, std::size_t start_offset, on_record_t&& on_record,
    on_corruption_t&& on_corruption = {})
{
    auto magic = details::peek_magic(buffer);
    if (!common::is_cdf(magic))
        throw std::runtime_error("cdf::io::debug::for_each_record: not a CDF file");

    const bool compressed = common::is_compressed(magic);
    if (common::is_v3x(magic))
    {
        details::run<v3x_tag>(buffer, compressed, start_offset,
            std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }
    else if (compressed)
    {
        details::run<v2x_tag>(buffer, true, start_offset, std::forward<on_record_t>(on_record),
            std::forward<on_corruption_t>(on_corruption));
    }
    else
    {
        cdf_CDR_t<v2x_tag> cdr_probe {};
        load_record(cdr_probe, buffer, 8);
        if (cdr_probe.Release >= 5)
            details::run<v2_5_or_more_tag>(buffer, false, start_offset,
                std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
        else
            details::run<v2_4_or_less_tag>(buffer, false, start_offset,
                std::forward<on_record_t>(on_record), std::forward<on_corruption_t>(on_corruption));
    }
}

template <typename buffer_t, typename on_record_t,
    typename on_corruption_t = default_corruption_handler>
void for_each_record(
    buffer_t&& buffer, on_record_t&& on_record, on_corruption_t&& on_corruption = {})
{
    for_each_record(buffer, 8UL, std::forward<on_record_t>(on_record),
        std::forward<on_corruption_t>(on_corruption));
}
```

(The `for_each_record(const std::string& path, ...)` / vector overloads below are untouched — they still forward to the 2-arg buffer overload, unaffected.)

- [ ] **Step 4: Add `dump_from_offset` to `nasa_compat_repr.hpp`**

Right after `dump(const std::string&, const dump_options&)`:

```cpp
inline void dump_from_offset(std::ostream& os, const std::string& path, std::size_t start_offset,
    const dump_options& opts = {})
{
    auto buffer = cdf::io::buffers::make_shared_file_adapter(path);
    cdf_encoding encoding = cdf_encoding::network;
    for_each_record(buffer, start_offset,
        [&](std::size_t offset, const auto& record)
        {
            if constexpr (requires { record.Encoding; })
                encoding = record.Encoding;
            if constexpr (requires { print_nasa(os, offset, record, encoding, opts); })
                print_nasa(os, offset, record, encoding, opts);
            else
                print_nasa(os, offset, record, opts);
        });
}

inline std::string dump_from_offset(
    const std::string& path, std::size_t start_offset, const dump_options& opts = {})
{
    std::ostringstream oss;
    dump_from_offset(oss, path, start_offset, opts);
    return oss.str();
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "dump_from_offset*"`
Expected: PASS.

- [ ] **Step 6: Run the full suite (record_stream + nasa_compat_repr) to confirm no regression from the `record_stream.hpp` overload set change**

Run: `ninja -C build test-record_stream test-nasa_compat_repr && ./build/tests/record_stream/test-record_stream && ./build/tests/nasa_compat_repr/test-nasa_compat_repr`
Expected: all pass (the new 3-arg `for_each_record` overload is additive; every existing 2-arg call site is unambiguous per the plan's earlier overload-resolution analysis — a callable second argument never matches the new `std::size_t start_offset` parameter).

- [ ] **Step 7: `clang-format` and commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
clang-format -i include/cdfpp/cdf-io/debug/record_stream.hpp include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp
git add include/cdfpp/cdf-io/debug/record_stream.hpp include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp tests/resources/rvariable_cdfirsdump_offset_reference.txt
git commit -m "feat(debug): add offset-starting for_each_record + nasa::dump_from_offset"
```

---

### Task 3: `--data` hex dump for VVR/CVVR/CCR payload bytes

**Files:**
- Modify: `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`
- Test: `tests/nasa_compat_repr/main.cpp` (extend)
- Fixture (already added): `tests/resources/rvariable_cdfirsdump_data_reference.txt`

**Interfaces:**
- Consumes: `dump_options.show_data` from Task 1.
- Produces: `nasa_hex_dump_lines(std::ostream&, const char* raw, std::size_t n)` (new `details::` helper — reused by VVR/CVVR/CCR).
- Produces: `print_nasa(ostream&, offset, const cdf_VVR_t<version_t>&, const char* raw_data, std::size_t raw_size, const dump_options& opts = {})` — VVR's `print_nasa` gains **two extra parameters** (not just `opts`) because `cdf_VVR_t` itself stores no payload bytes (only `data_size()`, computed from `record_size`) - see the note below. `print_nasa(CVVR)`/`print_nasa(CCR)` do **not** need new parameters - their structs already carry `.data`.

**Why VVR is different:** `cdf_VVR_t` (`include/cdfpp/cdf-io/desc-records.hpp:395-406`) has no `dynamic_array_bytes` field — by design, so that `for_each_record`'s physical-order walk never eagerly loads real variable data (often the large majority of a file's bytes) for every consumer that doesn't need it (the rich-tree `cdfdump`, the plain byte-for-byte dump, `pycdfpp.debug.for_each_record`). Adding a stored field to `cdf_VVR_t` would silently make every one of those load the full payload on every walk — a real perf regression. Instead, `dump()`'s own lambda (which already holds `buffer` in scope) reads the VVR's raw bytes **only when `opts.show_data` is true**, and passes them into `print_nasa(VVR, ...)` explicitly. `cdf_CVVR_t`/`cdf_CCR_t` already store their payload (`.data`, sized via `field_size()`) because they're needed unconditionally for decompression — no extra read is needed there.

**Fixture already prepared:** `tests/resources/rvariable_cdfirsdump_data_reference.txt` is a real captured `cdfirsdump -full -nopage -nosummary -data rvariable` run. Verified format: after a VVR's `uSize: N` line, one blank line, then the raw payload as uppercase hex, 38 bytes (76 hex chars) per line, 2-space-indented, last line short if `N` isn't a multiple of 38.

- [ ] **Step 1: Write the failing test**

Add to `tests/nasa_compat_repr/main.cpp`:

```cpp
SCENARIO("nasa_hex_dump_lines formats raw bytes like cdfirsdump's -data VVR hex dump",
    "[nasa_compat_repr]")
{
    THEN("38 bytes/line, uppercase, 2-space indented, short last line")
    {
        std::vector<char> raw(42);
        for (std::size_t i = 0; i < raw.size(); ++i)
            raw[i] = static_cast<char>(i);
        std::ostringstream oss;
        details::nasa_hex_dump_lines(oss, raw.data(), raw.size());
        const std::string expected
            = "  000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"
              "2021222324\n"
              "  25262728\n";
        REQUIRE(oss.str() == expected);
    }
}

SCENARIO("dump() with show_data=true hex-dumps a real VVR's payload, matching a real -data "
         "capture",
    "[nasa_compat_repr]")
{
    GIVEN("a real captured -data run of rvariable.cdf")
    {
        const std::string cdf_path = std::string(DATA_PATH) + "/rvariable.cdf";
        const std::string reference_path
            = std::string(DATA_PATH) + "/rvariable_cdfirsdump_data_reference.txt";
        std::ifstream f { reference_path, std::ios::binary };
        REQUIRE(f.is_open());
        std::string reference { std::istreambuf_iterator<char> { f },
            std::istreambuf_iterator<char> {} };

        THEN("dump() with show_data=true matches it exactly")
        {
            REQUIRE(dump(cdf_path, dump_options { .show_data = true }) == reference);
        }
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ninja -C build test-nasa_compat_repr`
Expected: compile error (`nasa_hex_dump_lines` doesn't exist; `dump_options` has no `show_data` case wired into `dump()` yet).

- [ ] **Step 3: Add `nasa_hex_dump_lines` and wire it into `print_nasa(CVVR)`/`print_nasa(CCR)`**

In `namespace details` (reuse the existing `nasa_hex_bytes` helper's neighborhood):

```cpp
// cdfirsdump -data's raw payload dump: BYTESperLINE=38, uppercase, 2-space indent, no
// separator between hex pairs. Shared by VVR (raw bytes supplied by the caller, see the
// nasa::print_nasa(VVR, ...) overload's own comment), CVVR, and CCR.
inline void nasa_hex_dump_lines(std::ostream& os, const char* raw, std::size_t n)
{
    constexpr std::size_t bytes_per_line = 38;
    for (std::size_t line_start = 0; line_start < n; line_start += bytes_per_line)
    {
        const std::size_t line_len = std::min(bytes_per_line, n - line_start);
        os << "  " << nasa_hex_bytes(raw + line_start, line_len, /*reversed=*/false) << '\n';
    }
}
```

Modify `print_nasa(CVVR)`:

```cpp
template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_CVVR_t<version_t>& r,
    const dump_options& opts = {})
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::CVVR, opts);
    os << "cSize: " << r.cSize << '\n';
    if (opts.show_data)
        details::nasa_hex_dump_lines(os, r.data.data(), r.data.size());
    os << '\n';
}
```

Modify `print_nasa(CCR)` — the real tool always prints `"Skipping compressed IRs...\n"` regardless of `-data` (verified: the CCR payload is a raw deflate stream the tool never inflates inline, per the existing comment on that line) — **no change needed there**, `-data` has no effect on CCR. (Only `opts` threading from Task 1 applies to CCR; leave its body otherwise as-is.)

- [ ] **Step 4: Give `print_nasa(VVR)` a raw-bytes overload and update `dump()`'s dispatch lambda**

Replace `print_nasa(VVR)`:

```cpp
// Unlike CVVR/CCR, cdf_VVR_t stores no payload bytes at all (see this task's own plan
// notes) - raw_data/raw_size are supplied by the caller (dump()'s lambda below), which
// only reads them from the buffer when opts.show_data is true, to avoid eagerly loading
// real variable data on every walk.
template <typename version_t>
inline void print_nasa(std::ostream& os, std::size_t offset, const cdf::io::cdf_VVR_t<version_t>& r,
    const char* raw_data, std::size_t raw_size, const dump_options& opts = {})
{
    print_nasa_header(
        os, static_cast<std::int64_t>(r.header.record_size), offset, cdf::cdf_record_type::VVR, opts);
    os << "uSize: " << r.data_size() << '\n';
    if (opts.show_data)
        details::nasa_hex_dump_lines(os, raw_data, raw_size);
}
```

In `dump()`'s lambda, add a VVR-specific branch **before** the generic `if constexpr` dispatch (VVR now needs 2 extra args the generic path doesn't know how to supply). Discriminate the record type with `std::is_same_v<record_t, cdf::io::cdf_VVR_t<typename record_t::cdf_version_t>>` — the same direct-type-comparison pattern already used throughout this file's own tests (`undecoded_record_t` has no `cdf_version_t` member, so this branch is only ever instantiated for a real `cdf_VVR_t<...>`, safe):

```cpp
inline void dump(std::ostream& os, const std::string& path, const dump_options& opts = {})
{
    auto buffer = cdf::io::buffers::make_shared_file_adapter(path);
    print_nasa_magic_preamble(os, buffer);
    cdf_encoding encoding = cdf_encoding::network;
    constexpr std::size_t header_size = sizeof(std::int32_t) * 2; // record_size + record_type,
                                                                   // both int32 on every version
                                                                   // this project supports
    for_each_record(buffer,
        [&](std::size_t offset, const auto& record)
        {
            using record_t = std::decay_t<decltype(record)>;
            if constexpr (requires { record.Encoding; })
                encoding = record.Encoding;
            if constexpr (std::is_same_v<record_t, cdf::io::cdf_VVR_t<typename record_t::cdf_version_t>>)
            {
                if (opts.show_data)
                {
                    no_init_vector<char> raw(record.data_size());
                    buffer.read(raw.data(), offset + header_size, raw.size());
                    print_nasa(os, offset, record, raw.data(), raw.size(), opts);
                }
                else
                    print_nasa(os, offset, record, nullptr, 0UL, opts);
            }
            else if constexpr (requires { print_nasa(os, offset, record, encoding, opts); })
                print_nasa(os, offset, record, encoding, opts);
            else
                print_nasa(os, offset, record, opts);
        });
}
```

Also add `#include <cdfpp/no_init_vector.hpp>` to `nasa_compat_repr.hpp`'s includes if not already present (check first — `record_stream.hpp`, which it already includes, pulls it in transitively; only add if the build fails without it).

- [ ] **Step 5: Run test to verify it passes**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "*hex dump*" -c "*show_data*"`
Expected: PASS.

- [ ] **Step 6: Run the full `nasa_compat_repr` suite to confirm the VVR signature change didn't break anything**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr`
Expected: all pass, including the existing `SCENARIO("print_nasa(VVR) prints uSize derived from record_size, not a real field", ...)` — that test calls `print_at_offset<cdf::io::cdf_VVR_t<...>>` via the generic `print_nasa(oss, offset, record)` 3-arg call path in the test file's own `print_at_offset` helper (`tests/nasa_compat_repr/main.cpp`), which will **not** compile anymore since `print_nasa(VVR)` now requires the raw-bytes pair. Fix that one call site: update `print_at_offset`'s generic dispatch (the `if constexpr (requires { print_nasa(oss, offset, record, encoding); })` / `else` fallback) to special-case VVR the same way `dump()`'s lambda does — add a VVR branch that calls `print_nasa(oss, offset, record, nullptr, 0UL)` before the generic fallback.

- [ ] **Step 7: `clang-format` and commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
clang-format -i include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp
git add include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp tests/resources/rvariable_cdfirsdump_data_reference.txt
git commit -m "feat(debug): add --data hex dump for VVR/CVVR payload bytes"
```

---

### Task 4: Summary table (`--level brief`) and the `--summary` trailer

**Files:**
- Modify: `include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp`
- Test: `tests/nasa_compat_repr/main.cpp` (extend)
- Fixtures (already added): `tests/resources/a_cdf_cdfirsdump_brief_reference.txt`, `tests/resources/rvariable_cdfirsdump_summary_reference.txt`

**Interfaces:**
- Produces: `struct summary_counts_t { std::size_t count = 0; std::size_t bytes = 0; };`
- Produces: `struct summary_stats { summary_counts_t CCR, CDR, GDR, ADR, AgrEDR, AzEDR, rVDR, zVDR, VXR, VVR, CVVR, CPR, SPR, UIR; std::size_t adr_global_count = 0; std::size_t adr_variable_count = 0; std::size_t used_bytes = 8; std::size_t wasted_bytes = 0; bool checksum_eligible = true; };`
- Produces: `summary_stats compute_summary(const std::string& path)`
- Produces: `void print_summary(std::ostream& os, const summary_stats& stats, bool show_adr_scope_split)`
- Produces: `void dump_brief(std::ostream& os, const std::string& path)` / `std::string dump_brief(const std::string& path)`
- Consumes: `dump()`'s existing per-record walk pattern (Task 1) for `dump()`'s new trailing `bool show_summary = false` parameter.

**Source-verified formatting rules** (from `src/tools/cdfirsdump.c`'s `DisplaySummary`/`DisplaySummary64`, lines ~2492-2900, plus the two real captures already saved):
- `width1` = decimal digit-width of `total_bytes` (`used_bytes + wasted_bytes`); used to right-pad every count column *and* the Total/Used/Unused byte columns.
- `width2` = decimal digit-width of the single largest per-category byte total; used to right-pad every bytes column.
- Row order is fixed: CCR, CDR, GDR, ADR, AgrEDR, AzEDR, rVDR, zVDR, VXR, VVR, CVVR, CPR, SPR, UIR.
- `wasted_bytes` = sum of UIR record sizes only; `used_bytes` = 8 (magic) + every other record's size. `total_bytes = used_bytes + wasted_bytes`.
- The ADR row gets a `(G:<n> V:<m>)` suffix (global/variable-scope ADR split) **only** when the dump level is MOST or FULL (`cdfirsdump.c:2576` — `MOST(level)` is `level >= MOST_`, true for both MOST and FULL, false only for BRIEF). In this project's own 2-level `{brief, full}` scheme (with `most` aliased to `full`), that means: shown for `--level full`'s summary trailer, never for `--level brief`.
- **The "`(+16 if with checksum)`" suffix on the `Total bytes` line is a real NASA quirk, not a deliberate per-file feature**: the underlying `checksum` variable starts `true` and is only ever set `false` while decoding an individual CDR's `Flags` field during a **full** per-record dump (`cdfirsdump.c:791-793`: `false` if version ≥ 3.2.0 and the CDR's checksum bit is clear, or if version < 3.2.0 at all). Brief mode never decodes CDR flags at all, so the variable simply keeps its default `true` — **`--level brief` always shows the suffix, regardless of the file**; `--level full`'s trailer reflects the real per-file value. Verified both ways: `a_cdf_cdfirsdump_brief_reference.txt` (brief, shows it) vs. `rvariable_cdfirsdump_summary_reference.txt` (full+summary, CDR has no checksum bit → doesn't show it) — same file family, different outcome, purely because of which code path ran.

- [ ] **Step 1: Write the failing numeric test for `compute_summary`**

Add to `tests/nasa_compat_repr/main.cpp`:

```cpp
SCENARIO("compute_summary accumulates real per-record-type counts/bytes", "[nasa_compat_repr]")
{
    GIVEN("the real rvariable.cdf fixture (1 CDR, 1 GDR, 1 rVDR, 1 VXR, 1 VVR, no ADRs)")
    {
        const std::string path = std::string(DATA_PATH) + "/rvariable.cdf";
        const auto stats = compute_summary(path);

        THEN("per-type counts and bytes match the file exactly")
        {
            REQUIRE(stats.CDR.count == 1);
            REQUIRE(stats.CDR.bytes == 312);
            REQUIRE(stats.GDR.count == 1);
            REQUIRE(stats.GDR.bytes == 84);
            REQUIRE(stats.rVDR.count == 1);
            REQUIRE(stats.rVDR.bytes == 344);
            REQUIRE(stats.VXR.count == 1);
            REQUIRE(stats.VXR.bytes == 140);
            REQUIRE(stats.VVR.count == 1);
            REQUIRE(stats.VVR.bytes == 8204);
            REQUIRE(stats.ADR.count == 0);
            REQUIRE(stats.UIR.count == 0);
        }
        THEN("used/wasted bytes match the file (no UIR freed space in this fixture)")
        {
            REQUIRE(stats.used_bytes == 9092);
            REQUIRE(stats.wasted_bytes == 0);
        }
        THEN("checksum_eligible reflects this fixture's real CDR flags (no checksum bit set)")
        {
            REQUIRE_FALSE(stats.checksum_eligible);
        }
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ninja -C build test-nasa_compat_repr`
Expected: compile error (`compute_summary`/`summary_stats` don't exist yet).

- [ ] **Step 3: Add `summary_counts_t`, `summary_stats`, and `compute_summary`**

Add near the top of the file, after `dump_options`:

```cpp
struct summary_counts_t
{
    std::size_t count = 0;
    std::size_t bytes = 0;
};

// Mirrors cdfirsdump.c's own flat per-record-type globals (CCRcount/CDRcount/...) plus
// the two derived facts DisplaySummary/DisplaySummary64 print alongside them (the
// global/variable ADR scope split, and the "checksum-eligible" quirk - see this task's
// own plan notes for why the latter is a full-dump-only side effect, not a real
// per-file feature).
struct summary_stats
{
    summary_counts_t CCR, CDR, GDR, ADR, AgrEDR, AzEDR, rVDR, zVDR, VXR, VVR, CVVR, CPR, SPR, UIR;
    std::size_t adr_global_count = 0;
    std::size_t adr_variable_count = 0;
    std::size_t used_bytes = 8; // the 8-byte magic preamble, always "used"
    std::size_t wasted_bytes = 0;
    bool checksum_eligible = true;
};

inline summary_stats compute_summary(const std::string& path)
{
    summary_stats stats;
    for_each_record(path,
        [&](std::size_t, const auto& record)
        {
            using record_t = std::decay_t<decltype(record)>;
            cdf::cdf_record_type type;
            std::size_t record_size;
            if constexpr (std::is_same_v<record_t, cdf::io::debug::undecoded_record_t>)
            {
                type = record.type;
                record_size = record.record_size;
            }
            else
            {
                type = record.header.record_type;
                record_size = static_cast<std::size_t>(record.header.record_size);
            }

            using enum cdf::cdf_record_type;
            summary_counts_t* bucket = nullptr;
            switch (type)
            {
                case CCR: bucket = &stats.CCR; break;
                case CDR: bucket = &stats.CDR; break;
                case GDR: bucket = &stats.GDR; break;
                case ADR: bucket = &stats.ADR; break;
                case AgrEDR: bucket = &stats.AgrEDR; break;
                case AzEDR: bucket = &stats.AzEDR; break;
                case rVDR: bucket = &stats.rVDR; break;
                case zVDR: bucket = &stats.zVDR; break;
                case VXR: bucket = &stats.VXR; break;
                case VVR: bucket = &stats.VVR; break;
                case CVVR: bucket = &stats.CVVR; break;
                case CPR: bucket = &stats.CPR; break;
                case SPR: bucket = &stats.SPR; break;
                case UIR: bucket = &stats.UIR; break;
                default: return;
            }
            bucket->count++;
            bucket->bytes += record_size;

            if (type == UIR)
                stats.wasted_bytes += record_size;
            else
                stats.used_bytes += record_size;

            if constexpr (requires { record.scope; })
            {
                if (static_cast<int32_t>(record.scope) == 1)
                    stats.adr_global_count++;
                else
                    stats.adr_variable_count++;
            }
            if constexpr (requires { record.Flags; record.Version; record.Release; record.Increment; })
            {
                const bool at_least_3_2_0 = std::tie(record.Version, record.Release, record.Increment)
                    >= std::tuple { 3, 2, 0 };
                stats.checksum_eligible
                    = at_least_3_2_0 && details::bit_set(record.Flags, 2 /* CDR_CHECKSUM_BIT */);
            }
        });
    return stats;
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr "[nasa_compat_repr]" -c "compute_summary*"`
Expected: PASS, all 6 assertions.

- [ ] **Step 5: `clang-format` and commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
clang-format -i include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp
git add include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp
git commit -m "feat(debug): add compute_summary, a per-record-type count/byte accumulator"
```

- [ ] **Step 6: Write the failing formatting test for `print_summary`/`dump_brief`**

Add to `tests/nasa_compat_repr/main.cpp`:

```cpp
SCENARIO("dump_brief() reproduces a real -brief capture byte-for-byte, including the "
         "always-on checksum note",
    "[nasa_compat_repr]")
{
    GIVEN("a real captured -brief run of a_cdf.cdf")
    {
        const std::string cdf_path = std::string(DATA_PATH) + "/a_cdf.cdf";
        const std::string reference_path
            = std::string(DATA_PATH) + "/a_cdf_cdfirsdump_brief_reference.txt";
        std::ifstream f { reference_path, std::ios::binary };
        REQUIRE(f.is_open());
        std::string reference { std::istreambuf_iterator<char> { f },
            std::istreambuf_iterator<char> {} };

        THEN("dump_brief() matches it exactly (a_cdf.cdf's real CDR has no checksum bit, "
             "but brief mode shows the note anyway)")
        {
            REQUIRE(dump_brief(cdf_path) == reference);
        }
    }
}

SCENARIO("dump() with show_summary=true appends the real full-level summary trailer, "
         "including the ADR G:/V: split and the correctly-computed checksum note",
    "[nasa_compat_repr]")
{
    GIVEN("a real captured -full (default -summary) run of rvariable.cdf")
    {
        const std::string cdf_path = std::string(DATA_PATH) + "/rvariable.cdf";
        const std::string reference_path
            = std::string(DATA_PATH) + "/rvariable_cdfirsdump_summary_reference.txt";
        std::ifstream f { reference_path, std::ios::binary };
        REQUIRE(f.is_open());
        std::string reference { std::istreambuf_iterator<char> { f },
            std::istreambuf_iterator<char> {} };

        THEN("dump() with show_summary=true matches it exactly (no checksum note here: "
             "this fixture's real CDR flags lack the checksum bit)")
        {
            REQUIRE(dump(cdf_path, dump_options {}, /*show_summary=*/true) == reference);
        }
    }
}
```

- [ ] **Step 7: Run test to verify it fails**

Run: `ninja -C build test-nasa_compat_repr`
Expected: compile error (`dump_brief` doesn't exist; `dump()` doesn't take a `show_summary` argument yet).

- [ ] **Step 8: Add `print_summary`**

```cpp
namespace details
{
    inline int decimal_width(std::size_t v) { return static_cast<int>(fmt::format("{}", v).size()); }
}

// Reproduces cdfirsdump.c's DisplaySummary/DisplaySummary64 exactly (see this task's plan
// notes for the width1/width2/checksum-note/ADR-scope-split rules, all source-verified).
// show_adr_scope_split also gates the checksum-note's truthfulness, matching the real
// tool's own accidental coupling (both stem from "brief mode never decodes CDR flags").
inline void print_summary(std::ostream& os, const summary_stats& s, bool show_adr_scope_split)
{
    const std::size_t total_bytes = s.used_bytes + s.wasted_bytes;
    const std::size_t ir_count = s.CCR.count + s.CDR.count + s.GDR.count + s.ADR.count
        + s.AgrEDR.count + s.AzEDR.count + s.rVDR.count + s.zVDR.count + s.VXR.count + s.VVR.count
        + s.CVVR.count + s.CPR.count + s.SPR.count + s.UIR.count;
    const std::size_t largest_bytes = std::max({ s.CCR.bytes, s.CDR.bytes, s.GDR.bytes, s.ADR.bytes,
        s.AgrEDR.bytes, s.AzEDR.bytes, s.rVDR.bytes, s.zVDR.bytes, s.VXR.bytes, s.VVR.bytes,
        s.CVVR.bytes, s.CPR.bytes, s.SPR.bytes, s.UIR.bytes });
    const int width1 = details::decimal_width(total_bytes);
    const int width2 = details::decimal_width(largest_bytes);
    const bool checksum_note = show_adr_scope_split ? s.checksum_eligible : true;

    auto pct = [&](std::size_t bytes)
    { return 100.0 * static_cast<double>(bytes) / static_cast<double>(total_bytes); };

    os << "\n\nSummary...";
    if (checksum_note)
        os << fmt::format("\n\n  Total bytes: {:>{}} (+16 if with checksum)\n", total_bytes, width1);
    else
        os << fmt::format("\n\n  Total bytes: {:>{}}\n", total_bytes, width1);
    os << fmt::format("   Used bytes: {:>{}}, {:7.3f}%\n", s.used_bytes, width1, pct(s.used_bytes));
    os << fmt::format(
        " Unused bytes: {:>{}}, {:7.3f}%\n\n", s.wasted_bytes, width1, pct(s.wasted_bytes));
    os << fmt::format("     IR count: {:>{}}\n\n", ir_count, width1);

    auto row = [&](const char* label, const summary_counts_t& c)
    {
        os << fmt::format(
            "{} count: {:>{}}, {:>{}} bytes, {:6.3f}%\n", label, c.count, width1, c.bytes, width2,
            pct(c.bytes));
    };
    row("    CCR", s.CCR);
    row("    CDR", s.CDR);
    row("    GDR", s.GDR);
    if (show_adr_scope_split)
        os << fmt::format("    ADR count: {:>{}}, {:>{}} bytes, {:6.3f}% (G:{} V:{})\n", s.ADR.count,
            width1, s.ADR.bytes, width2, pct(s.ADR.bytes), s.adr_global_count, s.adr_variable_count);
    else
        row("    ADR", s.ADR);
    row(" AgrEDR", s.AgrEDR);
    row("  AzEDR", s.AzEDR);
    row("   rVDR", s.rVDR);
    row("   zVDR", s.zVDR);
    row("    VXR", s.VXR);
    row("    VVR", s.VVR);
    row("   CVVR", s.CVVR);
    row("    CPR", s.CPR);
    row("    SPR", s.SPR);
    os << fmt::format("    UIR count: {:>{}}, {:>{}} bytes, {:6.3f}%\n\n\n", s.UIR.count, width1,
        s.UIR.bytes, width2, pct(s.UIR.bytes));
}
```

- [ ] **Step 9: Add `dump_brief` and extend `dump()` with `show_summary`**

```cpp
// A brief-level dump is banner + summary only, no per-record content at all - a real,
// verified quirk of the "Scanning records..." banner text is that it gets three blank
// lines before "Summary..." here (vs. one blank line before the magic numbers at full
// level) - see the real capture this is tested against.
inline void dump_brief(std::ostream& os, const std::string& path)
{
    os << "\nScanning records...\n\n\n";
    print_summary(os, compute_summary(path), /*show_adr_scope_split=*/false);
}

inline std::string dump_brief(const std::string& path)
{
    std::ostringstream oss;
    dump_brief(oss, path);
    return oss.str();
}
```

Modify `dump(std::ostream&, const std::string&, const dump_options&)` to accept a trailing `bool show_summary = false` and append the trailer when set:

```cpp
inline void dump(std::ostream& os, const std::string& path, const dump_options& opts = {},
    bool show_summary = false)
{
    auto buffer = cdf::io::buffers::make_shared_file_adapter(path);
    print_nasa_magic_preamble(os, buffer);
    cdf_encoding encoding = cdf_encoding::network;
    constexpr std::size_t header_size = sizeof(std::int32_t) * 2;
    for_each_record(buffer,
        [&](std::size_t offset, const auto& record)
        {
            using record_t = std::decay_t<decltype(record)>;
            if constexpr (requires { record.Encoding; })
                encoding = record.Encoding;
            if constexpr (std::is_same_v<record_t, cdf::io::cdf_VVR_t<typename record_t::cdf_version_t>>)
            {
                if (opts.show_data)
                {
                    no_init_vector<char> raw(record.data_size());
                    buffer.read(raw.data(), offset + header_size, raw.size());
                    print_nasa(os, offset, record, raw.data(), raw.size(), opts);
                }
                else
                    print_nasa(os, offset, record, nullptr, 0UL, opts);
            }
            else if constexpr (requires { print_nasa(os, offset, record, encoding, opts); })
                print_nasa(os, offset, record, encoding, opts);
            else
                print_nasa(os, offset, record, opts);
        });
    if (show_summary)
        print_summary(os, compute_summary(path), /*show_adr_scope_split=*/true);
}

inline std::string dump(
    const std::string& path, const dump_options& opts = {}, bool show_summary = false)
{
    std::ostringstream oss;
    dump(oss, path, opts, show_summary);
    return oss.str();
}
```

(This absorbs Task 3's VVR-branch edit to `dump()` — if Task 3 is done first, this step is only the `show_summary` addition; the block above shows the fully merged result for clarity.)

- [ ] **Step 10: Run tests to verify they pass**

Run: `ninja -C build test-nasa_compat_repr && ./build/tests/nasa_compat_repr/test-nasa_compat_repr`
Expected: all pass, including both new `SCENARIO`s from Step 6.

- [ ] **Step 11: `clang-format` and commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
clang-format -i include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp
git add include/cdfpp/cdf-io/debug/nasa_compat_repr.hpp tests/nasa_compat_repr/main.cpp tests/resources/a_cdf_cdfirsdump_brief_reference.txt tests/resources/rvariable_cdfirsdump_summary_reference.txt
git commit -m "feat(debug): add dump_brief and a show_summary trailer for dump()"
```

---

### Task 5: Python bindings

**Files:**
- Modify: `pycdfpp/debug.hpp`
- Modify: `pycdfpp/debug.py`
- Test: `tests/python_debug/test.py` (extend, already registered)

**Interfaces:**
- Consumes: `cdf::io::debug::nasa::dump`, `dump_from_offset`, `dump_brief`, `dump_options` (Tasks 1-4).
- Produces: `pycdfpp.debug.nasa_compat_dump(path, radix=10, show_data=False, summary=False) -> str`
- Produces: `pycdfpp.debug.nasa_compat_dump_from_offset(path, offset, radix=10, show_data=False) -> str`
- Produces: `pycdfpp.debug.nasa_compat_dump_brief(path) -> str`

- [ ] **Step 1: Write the failing Python test**

Add to `tests/python_debug/test.py`, inside `class NasaCompatDumpTest`:

```python
    def test_radix_16_matches_a_real_hex_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_hex_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump(path, radix=16), reference)

    def test_dump_from_offset_matches_a_real_offset_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_offset_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump_from_offset(path, 404), reference)

    def test_dump_brief_matches_a_real_brief_capture(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with open(os.path.join(_resources, 'a_cdf_cdfirsdump_brief_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump_brief(path), reference)

    def test_summary_true_matches_a_real_full_plus_summary_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_summary_reference.txt')) as f:
            reference = f.read()
        self.assertEqual(debug.nasa_compat_dump(path, summary=True), reference)
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && ninja && python3 -m pytest ../tests/python_debug/test.py -v -k "radix_16 or dump_from_offset or dump_brief or summary_true"` (adjust `PYTHONPATH` to the build dir if not already active — see `tests/meson.build`'s `PYTHONPATH=<build_root>` convention; or simply `ninja test -C build` and read the `python_debug` log).
Expected: `AttributeError: module 'pycdfpp._pycdfpp' has no attribute 'nasa_compat_dump_from_offset'` (or similar).

- [ ] **Step 3: Extend the `nasa_compat_dump` binding and add the two new ones**

In `pycdfpp/debug.hpp`, replace the existing `nasa_compat_dump` binding and add two more, right after it:

```cpp
    mod.def(
        "nasa_compat_dump",
        [](const std::string& path, int radix, bool show_data, bool summary)
        {
            cdf::io::debug::nasa::dump_options opts { radix, show_data };
            return cdf::io::debug::nasa::dump(path, opts, summary);
        },
        py::arg("path"), py::arg("radix") = 10, py::arg("show_data") = false,
        py::arg("summary") = false,
        R"pbdoc(
            Dump a CDF file's records in NASA's own cdfirsdump (-full -nopage) text
            format, byte-for-byte (verified against real captures of that tool's own
            output - see tests/nasa_compat_repr). Built on the same physical-order walk
            as for_each_record, not load()'s reconstructed view.

            Parameters
            ----------
            path : str
                Path to the CDF file.
            radix : int, default 10
                10 (decimal) or 16 (hex, "0x" + 16 uppercase digits) for record offsets.
            show_data : bool, default False
                Hex-dump VVR/CVVR payload bytes (cdfirsdump's -data).
            summary : bool, default False
                Append the closing record-type summary table (cdfirsdump's default
                -summary behavior; this binding defaults to False to match this
                project's own existing default, not NASA's).

            Returns
            -------
            str
                The full dump text, ready to print or write to a file.
        )pbdoc");

    mod.def(
        "nasa_compat_dump_from_offset",
        [](const std::string& path, std::size_t offset, int radix, bool show_data)
        {
            cdf::io::debug::nasa::dump_options opts { radix, show_data };
            return cdf::io::debug::nasa::dump_from_offset(path, offset, opts);
        },
        py::arg("path"), py::arg("offset"), py::arg("radix") = 10, py::arg("show_data") = false,
        R"pbdoc(
            Same as nasa_compat_dump, but starts the walk at a given byte offset instead
            of the file start - no magic-number preamble is printed (matching a real
            cdfirsdump -offset capture).

            Parameters
            ----------
            path : str
                Path to the CDF file.
            offset : int
                Byte offset to start the walk at.
            radix : int, default 10
                10 (decimal) or 16 (hex) for record offsets.
            show_data : bool, default False
                Hex-dump VVR/CVVR payload bytes.

            Returns
            -------
            str
                The dump text from that offset onward.
        )pbdoc");

    mod.def(
        "nasa_compat_dump_brief", [](const std::string& path)
        { return cdf::io::debug::nasa::dump_brief(path); }, py::arg("path"),
        R"pbdoc(
            Dump only the record-type summary table (cdfirsdump's -brief, the real
            tool's own default level), byte-for-byte, including its "(+16 if with
            checksum)" quirk which is always shown at brief level regardless of the
            file (see nasa_compat_repr.hpp's own notes on why).

            Parameters
            ----------
            path : str
                Path to the CDF file.

            Returns
            -------
            str
                The banner + summary table text.
        )pbdoc");
```

- [ ] **Step 4: Export the new names from `pycdfpp/debug.py`**

```python
for_each_record = _pycdfpp.debug_for_each_record
nasa_compat_dump = _pycdfpp.nasa_compat_dump
nasa_compat_dump_from_offset = _pycdfpp.nasa_compat_dump_from_offset
nasa_compat_dump_brief = _pycdfpp.nasa_compat_dump_brief

__all__ = ['for_each_record', 'nasa_compat_dump', 'nasa_compat_dump_from_offset',
           'nasa_compat_dump_brief']
```

- [ ] **Step 5: Rebuild and run test to verify it passes**

Run: `ninja -C build && ninja test -C build --print-errorlogs -R python_debug`
Expected: `python_debug` test PASS, all new cases included (plus the pre-existing `test_matches_a_real_cdfirsdump_capture_byte_for_byte` still passing unmodified — confirms the `nasa_compat_dump(path)` no-kwargs call still defaults to today's exact behavior).

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
git add pycdfpp/debug.hpp pycdfpp/debug.py tests/python_debug/test.py
git commit -m "feat(python): bind radix/offset/brief/summary dump options in pycdfpp.debug"
```

---

### Task 6: `cdfirsdump` CLI + CI wiring

**Files:**
- Create: `pycdfpp/cli_irsdump.py`
- Modify: `pyproject.toml`
- Create: `tests/python_cli_irsdump/test.py`
- Modify: `tests/meson.build` (register the new test)
- Modify: `.github/workflows/tests-with-sanitizers.yml` (register the new test)

**Interfaces:**
- Consumes: `pycdfpp.debug.nasa_compat_dump`, `nasa_compat_dump_from_offset`, `nasa_compat_dump_brief` (Task 5), `pycdfpp.__version__` (already exists, `pycdfpp/__init__.py:33`).
- Produces: `cdfirsdump` console script (`pycdfpp.cli_irsdump:main`), a cyclopts `app` object, importable for testing.

- [ ] **Step 1: Write the failing CLI test**

Create `tests/python_cli_irsdump/test.py`:

```python
#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import tempfile
import unittest

from cyclopts import App

from pycdfpp import __version__
from pycdfpp.cli_irsdump import app

os.environ['TZ'] = 'UTC'

_here = os.path.dirname(os.path.abspath(__file__))
_resources = os.path.join(_here, '..', 'resources')


def _run(*args):
    # cyclopts' App.__call__ defaults to sys.exit(0) on a *successful* run too (not just
    # on error) - result_action='return_value' returns main()'s return value instead,
    # verified against the installed cyclopts 4.22.1 this session (a bare app(tokens)
    # call raises SystemExit(0) here even when nothing went wrong).
    return app(list(args), result_action='return_value')


class CdfirsdumpCliTest(unittest.TestCase):
    def test_is_a_cyclopts_app_named_cdfirsdump(self):
        self.assertIsInstance(app, App)
        self.assertEqual(app.name[0], 'cdfirsdump')

    def test_default_level_is_brief_and_matches_a_real_capture(self):
        path = os.path.join(_resources, 'a_cdf.cdf')
        with open(os.path.join(_resources, 'a_cdf_cdfirsdump_brief_reference.txt')) as f:
            reference = f.read()
        # A bare path (no --output handle held open by the test itself) avoids
        # NamedTemporaryFile's Windows-only "second open() on an already-open handle"
        # lock, which this project's tests-windows.yml CI would otherwise hit.
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_level_full_with_radix_16_matches_a_real_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_hex_reference.txt')) as f:
            reference = f.read()
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--level', 'full', '--no-summary', '--radix', '16', '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_offset_matches_a_real_capture(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with open(os.path.join(_resources, 'rvariable_cdfirsdump_offset_reference.txt')) as f:
            reference = f.read()
        with tempfile.TemporaryDirectory() as tmpdir:
            out_path = os.path.join(tmpdir, 'out.txt')
            _run(path, '--level', 'full', '--no-summary', '--offset', '404', '--output', out_path)
            with open(out_path) as f:
                self.assertEqual(f.read(), reference)

    def test_most_aliases_to_full(self):
        path = os.path.join(_resources, 'rvariable.cdf')
        with tempfile.TemporaryDirectory() as tmpdir:
            most_path = os.path.join(tmpdir, 'most.txt')
            full_path = os.path.join(tmpdir, 'full.txt')
            _run(path, '--level', 'most', '--no-summary', '--output', most_path)
            _run(path, '--level', 'full', '--no-summary', '--output', full_path)
            with open(most_path) as f_most, open(full_path) as f_full:
                self.assertEqual(f_most.read(), f_full.read())

    def test_about_prints_the_pycdfpp_version(self):
        import io
        import contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            _run(os.path.join(_resources, 'a_cdf.cdf'), '--about')
        self.assertIn(__version__, buf.getvalue())


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `PYTHONPATH=build python3 -m pytest tests/python_cli_irsdump/test.py -v`
Expected: `ModuleNotFoundError: No module named 'pycdfpp.cli_irsdump'`.

- [ ] **Step 3: Write `pycdfpp/cli_irsdump.py`**

```python
"""
pycdfpp.cli_irsdump
--------------------
The ``cdfirsdump`` console script: a modern, transparent replacement for NASA's own
cdfirsdump tool, byte-for-byte compatible with its real output but using double-dash
flags (not NASA's single-dash syntax) - conceptual parity, not a literal drop-in for
shell scripts calling the real tool. See docs/superpowers/specs/2026-07-25-cdfirsdump-
cli-design.md for the full flag-scope rationale.
"""
import cyclopts

from . import __version__
from .debug import nasa_compat_dump, nasa_compat_dump_brief, nasa_compat_dump_from_offset

app = cyclopts.App(
    name='cdfirsdump',
    help="Dump a CDF file's Internal Records (IRs), byte-for-byte compatible with "
         "NASA's own cdfirsdump tool.",
)


@app.default
def main(path: str, *, level: str = 'brief', summary: bool = True, data: bool = False,
          output: str = None, offset: int = None, radix: int = 10, about: bool = False):
    """Dump a CDF file's Internal Records (IRs) in NASA cdfirsdump's text format.

    Parameters
    ----------
    path: Path to the CDF file.
    level: "brief" (summary table only, the default - matches the real tool's own
        default) or "full" (per-record dump). "most" is accepted as an alias for
        "full" - NASA's real MOST-level per-field gating isn't independently
        implemented here.
    summary: Whether to print the closing summary table. Only meaningful at
        level="full" (level="brief" is nothing *but* the summary table).
    data: Hex-dump VVR/CVVR payload bytes. Only applies at level="full".
    output: Write to this file instead of stdout.
    offset: Start the dump at this byte offset instead of the file start. Ignored
        when level="brief" (brief always summarizes the whole file).
    radix: 10 (decimal, default) or 16 (hex) for record offsets.
    about: Print pycdfpp's version and exit.
    """
    if about:
        print(f'pycdfpp {__version__}')
        return

    resolved_level = 'full' if level == 'most' else level

    if resolved_level == 'brief':
        if summary:
            text = nasa_compat_dump_brief(path)
        else:
            # NASA's own "Scanning records..." banner text (see nasa_compat_repr.hpp's
            # dump_brief) - brief level with no summary has nothing else to show.
            text = '\nScanning records...\n\n\n'
    elif offset is not None:
        text = nasa_compat_dump_from_offset(path, offset, radix=radix, show_data=data)
    else:
        text = nasa_compat_dump(path, radix=radix, show_data=data, summary=summary)

    if output:
        with open(output, 'w') as f:
            f.write(text)
    else:
        print(text, end='')


if __name__ == '__main__':
    app()
```

- [ ] **Step 4: Add the console script entry to `pyproject.toml`**

```toml
[project.scripts]
cdfdump = "pycdfpp.cli:main"
cdfirsdump = "pycdfpp.cli_irsdump:main"
```

- [ ] **Step 5: Register the new test file in `tests/meson.build`**

Change the `py_test` `foreach` list:

```meson
foreach py_test:['python_loading', 'python_saving', 'python_skeletons',
            'python_variable_set_values', 'full_corpus', 'python_chrono',
            'python_windows_crash', 'python_structural_introspection', 'python_debug',
            'python_cli_irsdump']
```

- [ ] **Step 6: Register the new test file in `tests-with-sanitizers.yml`**

In `.github/workflows/tests-with-sanitizers.yml`, find this exact 3-line block (currently around line 81-83):

```
            python_loading python_saving python_skeletons python_variable_set_values \
            python_chrono python_windows_crash python_structural_introspection \
            python_debug python_wrapper_cpp
```

Change the last line to add the new test name:

```
            python_debug python_cli_irsdump python_wrapper_cpp
```

- [ ] **Step 7: Rebuild and run test to verify it passes**

Run: `ninja -C build && PYTHONPATH=build python3 -m pytest tests/python_cli_irsdump/test.py -v`
Expected: all 6 tests PASS.

- [ ] **Step 8: Run the full test suite (minus `full_corpus`) to confirm no regressions anywhere in the feature**

Run: `ninja test -C build --print-errorlogs $(meson test -C build --list | grep -v full_corpus | sed 's/^/-R ^/;s/$/$/' | tr '\n' ' ')`

If that composed command is awkward in your shell, simpler equivalent: `ninja test -C build --print-errorlogs --suite '' 2>&1 | tee /tmp/test-output.txt; grep -c "^PASS\|Ok:" /tmp/test-output.txt` and separately confirm `full_corpus` was the only failure/skip (per `feedback-skip-full-corpus` project convention, it's expected to fail offline — don't let it block this task).

Expected: every test passes except (optionally) `full_corpus`, which needs network access and is not part of this feature's verification surface.

- [ ] **Step 9: Commit**

```bash
cd /var/home/jeandet/Documents/prog/CDFpp
git add pycdfpp/cli_irsdump.py pyproject.toml tests/python_cli_irsdump/test.py tests/meson.build .github/workflows/tests-with-sanitizers.yml
git commit -m "feat(cli): add the cdfirsdump console script (Phase A)"
```

---

## After all 6 tasks

Run `meson wrap update` (project hygiene convention before opening a PR — see `feedback` memory on this), rebuild, rerun the full suite once more, then follow `superpowers:finishing-a-development-branch` for how to integrate `feature/debug-record-stream-cdfdump` (still not pushed as of this plan — that's a decision for the user, not automatic). Delete `HANDOFF-cdfirsdump-cli.md` and `HANDOFF-pad-values.md` at the repo root once their content is fully superseded by this plan and the shipped code (both are explicitly scratch/handover files per their own headers).
