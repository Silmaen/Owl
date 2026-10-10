"""Filtering of the updates found by the `DependencyReport` action, on ConanCenter and upstream."""

import json
from pathlib import Path
from unittest import mock

from ci.actions import dependency_report
from ci.actions.dependency_report import (
    DependencyReport,
    OutdatedDependency,
    is_false_positive,
    is_update,
    newest_update,
    parse_conan_list,
    parse_outdated,
    parse_tags,
    tag_version,
    upstream_sources,
)


def _entry(current: str, latest: str) -> dict:
    name = current.split("/")[0]
    return {"current_versions": [current], "version_ranges": [], "latest_remote": {"ref": f"{name}/{latest}"}}


def test_snapshots_and_dates_older_than_a_semantic_version_are_ignored() -> None:
    assert is_false_positive("1.10", "cci.20231114")
    assert is_false_positive("2.1.0", "20191104")
    assert is_false_positive("1.92.9b-docking", "cci.20230105+1.89.2.docking")
    assert is_false_positive("4.4.3", "4.4.3")


def test_real_updates_are_kept() -> None:
    assert not is_false_positive("2.13.2", "2.14.3")
    assert not is_false_positive("cci.20240531", "cci.20250101")
    assert is_update("1.2.10", "1.2.16.1")
    assert is_update("3.4", "3.5.1")
    assert is_update("1.45", "1.49")


def test_versions_compare_by_number_not_by_text() -> None:
    assert is_update("1.9", "1.10")
    assert not is_update("1.10", "1.9")
    assert not is_update("1.10", "1.10.0")
    assert is_update("1.92.9", "1.92.9b")


def test_flavours_and_pre_releases() -> None:
    assert not is_update("1.92.9b-docking", "1.92.10")
    assert is_update("1.92.9b-docking", "1.92.10-docking")
    assert not is_update("0.8.2", "0.8.4-rc")
    assert is_update("2.0.0-rc13", "2.0.0")
    assert is_update("2.0.0-rc13", "2.0.0-rc14")
    assert is_update("2.0.0rc13", "2.0.0rc14")
    assert not is_update("2.0.0-rc13", "2.0.0-rc10")


def test_known_false_positives_are_ignored() -> None:
    assert not is_update("1.10", "1.83", "imguizmo")
    assert is_update("1.10", "1.83")


def test_the_newest_update_hides_behind_a_snapshot() -> None:
    assert newest_update("plutovg", "1.3.2", ["1.3.1", "1.3.3", "cci.20230205", "1.3.2"]) == "1.3.3"
    assert newest_update("plutovg", "1.3.3", ["1.3.3", "cci.20230205"]) is None


def test_the_conan_json_becomes_the_sorted_list_of_real_updates() -> None:
    data = {
        "imguizmo": _entry("imguizmo/1.10", "cci.20231114"),
        "freetype": _entry("freetype/2.13.2", "2.14.3"),
        "brotli": _entry("brotli/1.1.0", "1.2.0"),
        "plutovg": _entry("plutovg/1.3.2", "cci.20230205"),
        "local": {"current_versions": ["local/1.0"], "latest_remote": None},
    }
    versions = {"plutovg": ["1.3.2", "1.3.3", "cci.20230205"], "imguizmo": ["1.10", "cci.20231114"]}

    assert parse_outdated(data, versions) == [
        OutdatedDependency(name="brotli", current="1.1.0", latest="1.2.0"),
        OutdatedDependency(name="freetype", current="2.13.2", latest="2.14.3"),
        OutdatedDependency(name="plutovg", current="1.3.2", latest="1.3.3"),
    ]
    assert [d.name for d in parse_outdated(data)] == ["brotli", "freetype"]


def test_the_conan_list_json_gives_every_version() -> None:
    data: dict = {"conancenter": {"flac/1.4.2": {}, "flac/1.5.0#abc": {}}}

    assert parse_conan_list(data) == ["1.4.2", "1.5.0"]
    assert parse_conan_list({"conancenter": {"error": "not found"}}) == []


def test_tags_carry_versions() -> None:
    assert tag_version("v1.4.1") == "1.4.1"
    assert tag_version("1.49") == "1.49"
    assert tag_version("glfw-3.5.1", "glfw") == "3.5.1"
    assert tag_version("cpp14") is None
    assert tag_version("vulkan-sdk-1.4.350.0", "slang") is None
    ls_remote = "a" * 40 + "\trefs/tags/v3.5.1\n" + "b" * 40 + "\trefs/tags/latest\n"
    assert parse_tags(ls_remote) == ["3.5.1"]


def test_local_recipe_sources_point_to_their_repositories() -> None:
    conandata = """sources:
  "1.4.1":
    nfd:
      url: "https://github.com/btzy/nativefiledialog-extended/archive/refs/tags/v1.4.1.tar.gz"
    wayland-protocols:
      url: "https://gitlab.freedesktop.org/wayland/wayland-protocols/-/releases/1.45/downloads/wp-1.45.tar.xz"
  "3.4":
    url: "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip"
  "cci.1":
    url: "https://github.com/goossens/ImGuiColorTextEdit/archive/9670c32ec0fde0f048da5dae19decf8cc310ca38.tar.gz"
  "3.100":
    url: https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
"""
    assert upstream_sources(conandata) == [
        ("nativefiledialog-extended", "https://github.com/btzy/nativefiledialog-extended.git", "1.4.1"),
        ("wayland-protocols", "https://gitlab.freedesktop.org/wayland/wayland-protocols.git", "1.45"),
        ("glfw", "https://github.com/glfw/glfw.git", "3.4"),
    ]


def test_the_action_never_fails(tmp_path: Path) -> None:
    output = tmp_path / "report.json"
    with (
        mock.patch.object(dependency_report, "run_command", return_value=1),
        mock.patch.object(dependency_report, "report_statistic") as statistic,
    ):
        assert DependencyReport().run(mock.MagicMock(), [f"--output={output}"]) == 0
    statistic.assert_not_called()


def _fake_conan(output: Path, outdated: dict, versions: dict[str, list[str]]):
    def run(command: list[str], *_args, **_kwargs) -> int:
        out_file = next((arg.split("=", 1)[1] for arg in command if arg.startswith("--out-file=")), None)
        if command[1:3] == ["graph", "outdated"]:
            assert out_file == str(output)
            Path(out_file).write_text(json.dumps(outdated))
        elif command[1] == "list" and out_file:
            name = command[2].split("/")[0]
            refs: dict = {f"{name}/{version}": {} for version in versions.get(name, [])}
            Path(out_file).write_text(json.dumps({"conancenter": refs}))
        return 0

    return run


def test_the_action_resolves_with_the_local_recipes_and_publishes_the_count(tmp_path: Path) -> None:
    output = tmp_path / "report.json"
    outdated = {
        "libalsa": _entry("libalsa/1.2.10", "1.2.16.1"),
        "plutovg": _entry("plutovg/1.3.2", "cci.20230205"),
    }
    fake = _fake_conan(output, outdated, {"plutovg": ["1.3.3", "cci.20230205"]})
    with (
        mock.patch.object(dependency_report, "run_command", side_effect=fake) as run,
        mock.patch.object(dependency_report, "report_statistic") as statistic,
    ):
        assert DependencyReport().run(mock.MagicMock(), [f"--output={output}", "--no-upstream"]) == 0
    graph = next(call.args[0] for call in run.call_args_list if call.args[0][1:3] == ["graph", "outdated"])
    assert graph[graph.index("-r") + 1] == "owl-local"
    assert "conancenter" in graph
    statistic.assert_called_once_with("OutdatedDependencies", 2)
    assert [entry["latest"] for entry in json.loads(output.read_text())] == ["1.2.16.1", "1.3.3"]


def test_the_action_checks_the_local_recipes_upstream(tmp_path: Path) -> None:
    recipes = tmp_path / "conan" / "recipes" / "glfw" / "all"
    recipes.mkdir(parents=True)
    (recipes / "conandata.yml").write_text(
        'sources:\n  "3.4":\n    url: "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip"\n'
    )
    output = tmp_path / "report.json"
    ls_remote = "a" * 40 + "\trefs/tags/3.4\n" + "b" * 40 + "\trefs/tags/3.5.1\n"
    with (
        mock.patch.object(dependency_report, "root", tmp_path),
        mock.patch.object(dependency_report, "run_command", side_effect=_fake_conan(output, {}, {})),
        mock.patch.object(dependency_report, "run_command_capture_output", return_value=(0, ls_remote)),
        mock.patch.object(dependency_report, "report_statistic") as statistic,
    ):
        assert DependencyReport().run(mock.MagicMock(), [f"--output={output}"]) == 0
    statistic.assert_called_once_with("OutdatedDependencies", 1)
    assert json.loads(output.read_text()) == [
        {"name": "glfw", "current": "3.4", "latest": "3.5.1", "source": "upstream"}
    ]
