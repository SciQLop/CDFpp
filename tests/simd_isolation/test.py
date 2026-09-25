#!/usr/bin/env python3
"""Each per-arch SIMD library is built with that arch's flags (-mavx512bw, -mavx2, ...).
An inline function it emits is a weak symbol the linker may keep for the whole program,
so a CPU without that arch would run it and die with SIGILL. Only arch-specific symbols
may be defined there."""
import re
import subprocess
import sys
from pathlib import Path


def weak_functions(nm, library):
    """Weak data (V, u) holds no instructions, like the exception personality pointer
    DW.ref.__gxx_personality_v0: only weak code (W) matters."""
    listing = subprocess.run([nm, "-C", "--defined-only", library],
                             capture_output=True, text=True, check=True).stdout
    return {line.split(" ", 2)[2] for line in listing.splitlines()
            if re.match(r"^[0-9a-f]+ W ", line)}


def leaked_symbols(nm, library):
    arch = re.search(r"cdfpp_x86_vectorized_(\w+)\.", Path(library).name).group(1)
    return sorted(s for s in weak_functions(nm, library) if arch not in s)


def main(nm, libraries):
    leaks = {lib: leaked_symbols(nm, lib) for lib in libraries}
    for lib, symbols in leaks.items():
        for symbol in symbols:
            print(f"{Path(lib).name}: {symbol}")
    return 1 if any(leaks.values()) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2:]))
