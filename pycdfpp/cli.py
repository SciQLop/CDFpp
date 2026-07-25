"""
pycdfpp.cli
-----------
The ``cdfdump`` console script: dumps a CDF file's on-disk records in physical
order, using :func:`pycdfpp.debug.for_each_record`.
"""

import cyclopts
from rich.console import Console
from rich.tree import Tree

from .debug import for_each_record, nasa_compat_dump

app = cyclopts.App(
    name='cdfdump',
    help="Dump a CDF file's on-disk records in physical order "
         "(not the reconstructed variable/attribute view pycdfpp.load() gives you).",
)


def _format_value(value) -> str:
    if isinstance(value, bytes):
        return f'<{len(value)} bytes>'
    if isinstance(value, str):
        return f'"{value}"'
    return str(value)


def build_tree(path: str) -> Tree:
    """Build a rich Tree of a CDF file's on-disk records, one node per record.

    Parameters
    ----------
    path : str
        Path to the CDF file.

    Returns
    -------
    rich.tree.Tree
        One child per record (in physical file order), each with one grandchild
        leaf per field.
    """
    tree = Tree(path)
    for offset, type_name, fields in for_each_record(path):
        record_node = tree.add(f'[bold cyan]@{offset}[/bold cyan] [bold]{type_name}[/bold]')
        for name, value in fields.items():
            record_node.add(f'[green]{name}[/green]: {_format_value(value)}')
    return tree


@app.default
def main(path: str, *, irsdump: bool = False):
    """Dump a CDF file's on-disk records in physical order.

    Parameters
    ----------
    path: Path to the CDF file.
    irsdump: Print NASA's cdfirsdump (-full -nopage -nosummary) text format instead
        of the default rich tree - byte-for-byte compatible with the real tool,
        useful for diffing against it or feeding other cdfirsdump-aware tooling.
    """
    if irsdump:
        print(nasa_compat_dump(path), end='')
    else:
        Console().print(build_tree(path))


if __name__ == '__main__':
    app()
