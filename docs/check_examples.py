"""Run every Python example of the documentation, page by page.

Blocks of a page run in order, in one shared namespace, like notebook cells, inside a
fresh temporary directory. Some examples download public files, so this needs network
access.

    python docs/check_examples.py [page.rst ...]
"""
import os
import re
import subprocess
import sys
import textwrap
import tempfile
import traceback
import warnings
from pathlib import Path

DOCS = Path(__file__).parent
DIRECTIVE = re.compile(r"^(\s*)\.\. code-block:: python\s*$")


def python_blocks(text: str):
    """Yield (line number, code) for each ``.. code-block:: python`` of an rst text."""
    lines = text.splitlines()
    for number, line in enumerate(lines):
        match = DIRECTIVE.match(line)
        if not match:
            continue
        directive_indent = len(match.group(1))
        body = []
        for body_line in lines[number + 1:]:
            if body_line.strip() and len(body_line) - len(body_line.lstrip()) <= directive_indent:
                break
            body.append(body_line)
        yield number + 1, textwrap.dedent("\n".join(_skip_options(body)))


def _skip_options(body: list[str]) -> list[str]:
    """Drop the directive's ``:option:`` lines, which come before the first blank line."""
    first_blank = next((i for i, l in enumerate(body) if not l.strip()), len(body))
    return [l for i, l in enumerate(body) if i >= first_blank or not l.strip().startswith(":")]


def run_page(page: Path) -> list[str]:
    failures = []
    namespace = {"__name__": "__main__"}
    with tempfile.TemporaryDirectory() as workdir:
        previous = os.getcwd()
        os.chdir(workdir)
        try:
            for line, code in python_blocks(page.read_text()):
                try:
                    with warnings.catch_warnings():
                        warnings.simplefilter("ignore")
                        exec(compile(code, f"{page.name}:{line}", "exec"), namespace)
                except Exception:
                    failures.append(f"{page.name}:{line}\n{traceback.format_exc(limit=2)}")
        finally:
            os.chdir(previous)
    return failures


def check_one_page(page: str) -> int:
    import matplotlib
    matplotlib.use("Agg")
    failures = run_page(Path(page).resolve())
    for failure in failures:
        print(f"FAIL {failure}")
    return len(failures)


def main(pages: list[str]) -> int:
    # Each page runs in its own process: an example that crashes the interpreter only
    # fails its own page.
    paths = [Path(p).resolve() for p in pages] or sorted(DOCS.glob("*.rst"))
    failed = 0
    for page in paths:
        result = subprocess.run([sys.executable, __file__, "--page", str(page)])
        if result.returncode:
            failed += 1
            crash = f" (crashed, exit code {result.returncode})" if result.returncode < 0 else ""
            print(f"FAIL {page.name}{crash}")
    print(f"{failed} failing page(s) out of {len(paths)}")
    return 1 if failed else 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["--page"]:
        sys.exit(min(check_one_page(sys.argv[2]), 1))
    sys.exit(main(sys.argv[1:]))
