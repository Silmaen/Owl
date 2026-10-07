"""Fuzzer discovery and command line of the `Fuzz` action."""

from pathlib import Path

from ci.actions.fuzz import find_fuzzers, fuzzer_command


def _touch(path: Path) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("")
    return path


def test_only_fuzzer_executables_are_found(tmp_path: Path) -> None:
    _touch(tmp_path / "owl_pack_reader_fuzzer")
    _touch(tmp_path / "owl_scene_fuzzer")
    _touch(tmp_path / "owl_scene_tests_unit_test")
    (tmp_path / "owl_dir_fuzzer").mkdir()

    assert [p.name for p in find_fuzzers(tmp_path)] == ["owl_pack_reader_fuzzer", "owl_scene_fuzzer"]


def test_a_fuzzer_can_be_selected_by_name(tmp_path: Path) -> None:
    _touch(tmp_path / "owl_pack_reader_fuzzer")
    _touch(tmp_path / "owl_scene_fuzzer")

    assert [p.name for p in find_fuzzers(tmp_path, "owl_scene_fuzzer")] == ["owl_scene_fuzzer"]
    assert find_fuzzers(tmp_path, "owl_missing_fuzzer") == []


def test_the_seed_corpus_is_added_only_when_it_exists(tmp_path: Path) -> None:
    binary = tmp_path / "bin" / "owl_pack_reader_fuzzer"
    work = tmp_path / "work"
    seed = tmp_path / "seed"

    without = fuzzer_command(binary, work, seed, 60)
    seed.mkdir()
    with_seed = fuzzer_command(binary, work, seed, 60)

    assert without == [str(binary), "-max_total_time=60", f"-artifact_prefix={work / 'artifacts'}/",
                       "-print_final_stats=1", str(work / "corpus")]
    assert with_seed == [*without, str(seed)]
