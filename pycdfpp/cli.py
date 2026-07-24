"""
pycdfpp.cli
-----------
The ``cdfdump`` console script: dumps a CDF file's on-disk records in physical
order, using :func:`pycdfpp.debug.for_each_record`.
"""

import argparse

from .debug import for_each_record


def _format_value(value) -> str:
    if isinstance(value, bytes):
        return f'<{len(value)} bytes>'
    if isinstance(value, str):
        return f'"{value}"'
    return str(value)


def dump(path: str) -> str:
    """Format a CDF file's on-disk records, one line per field.

    Parameters
    ----------
    path : str
        Path to the CDF file.

    Returns
    -------
    str
        Multi-line text: one ``@offset type_name`` header per record, followed by
        one indented ``field_name: value`` line per field.
    """
    lines = []
    for offset, type_name, fields in for_each_record(path):
        lines.append(f'@{offset} {type_name}')
        for name, value in fields.items():
            lines.append(f'  {name}: {_format_value(value)}')
    return '\n'.join(lines)


def main(argv=None) -> None:
    parser = argparse.ArgumentParser(
        prog='cdfdump',
        description="Dump a CDF file's on-disk records in physical order "
                    "(not the reconstructed variable/attribute view pycdfpp.load() gives you).",
    )
    parser.add_argument('path', help='Path to the CDF file')
    args = parser.parse_args(argv)
    print(dump(args.path))


if __name__ == '__main__':
    main()
