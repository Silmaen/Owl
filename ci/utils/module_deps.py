"""
Direction of the dependencies between the engine modules, for the `CodeStyle` gate.

A public header of `source/owl/public/<module>/` may include its own module and the modules of a lower layer only:
the layer stack below is the rule (`.claude/rules/module-layout.md` documents it). Two modules of one layer do not
include each other. Implementation files (`source/owl/private/`) may reach up: the rule keeps the public API acyclic.
"""

from __future__ import annotations

import re
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

LAYERS: tuple[tuple[str, ...], ...] = (
    ("core",),
    ("math", "debug", "platform", "script"),
    ("input",),
    ("event",),
    ("data",),
    ("renderer",),
    ("io", "window"),
    ("sound",),
    ("scene",),
    ("physics",),
    ("app",),
    ("gui",),
)
"""Engine modules from the bottom layer to the top one."""

LAYER_OF: dict[str, int] = {module: index for index, layer in enumerate(LAYERS) for module in layer}

_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([A-Za-z_]+)/')


@dataclass(frozen=True)
class UpwardInclude:
    """A public header including a module it must not depend on."""

    path: Path
    line: int
    module: str
    target: str


def upward_includes(public_root: Path, files: Iterable[Path]) -> list[UpwardInclude]:
    """
    Find the includes of public headers that point to the same or a higher layer.

    :param public_root: `source/owl/public`.
    :param files: The public headers to check.
    :return: One entry per offending include; unknown modules are reported too (add them to `LAYERS`).
    """
    found: list[UpwardInclude] = []
    for path in files:
        parts = path.relative_to(public_root).parts
        if len(parts) < 2:
            continue  # the umbrella header (`owl.h`) sits above every module
        module = parts[0]
        try:
            lines = path.read_text(errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, start=1):
            match = _INCLUDE_RE.match(line)
            if match is None:
                continue
            target = match.group(1)
            if target == module or not (public_root / target).is_dir():
                continue
            if module not in LAYER_OF or target not in LAYER_OF or LAYER_OF[target] >= LAYER_OF[module]:
                found.append(UpwardInclude(path, number, module, target))
    return found
