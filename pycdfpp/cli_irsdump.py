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
