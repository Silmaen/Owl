"""Module layering of the public headers (CodeStyle `module-deps`)."""

from pathlib import Path

from ci import root
from ci.utils.module_deps import LAYER_OF, upward_includes


def _header(public: Path, module: str, text: str) -> Path:
    path = public / module / "A.h"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def test_every_engine_module_has_a_layer() -> None:
    modules = {p.name for p in (root / "source" / "owl" / "public").iterdir() if p.is_dir()}
    assert modules <= set(LAYER_OF)


def test_only_includes_of_the_same_or_a_higher_layer_are_reported(tmp_path: Path) -> None:
    for module in ("core", "renderer", "scene"):
        (tmp_path / module).mkdir()
    down = _header(tmp_path, "scene", '#include "renderer/Camera.h"\n#include "core/Core.h"\n#include <vector>\n')
    up = _header(tmp_path, "renderer", '#include "core/Core.h"\n#include "scene/Scene.h"\n')

    umbrella = tmp_path / "owl.h"
    umbrella.write_text('#include "scene/Scene.h"\n')

    found = upward_includes(tmp_path, [down, up, umbrella])

    assert [(f.module, f.line, f.target) for f in found] == [("renderer", 2, "scene")]
