"""Module layering of the public headers (CodeStyle `module-deps`)."""

from pathlib import Path

from ci import root
from ci.utils.module_deps import LAYER_OF, third_party_includes, upward_includes


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


def test_only_third_party_headers_the_package_provides_are_allowed(tmp_path: Path) -> None:
    for module in ("core", "scene", "gui"):
        (tmp_path / module).mkdir()
    scene = _header(tmp_path, "scene", "#include <entt/entt.hpp>\n#include <core/Core.h>\n#include <yaml-cpp/yaml.h>\n")
    core = _header(tmp_path, "core", "#include <vector>\n#include <imgui.h>\n")
    gui = _header(tmp_path, "gui", "#include <imgui.h>\n")

    found = third_party_includes(tmp_path, [scene, core, gui])

    assert [(f.module, f.line, f.target) for f in found] == [("scene", 3, "yaml-cpp/yaml.h"), ("core", 2, "imgui.h")]
