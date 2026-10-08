"""Dead NOLINT detection of the CodeStyle `nolint` sub-check."""

from pathlib import Path

from ci.utils.nolint import dead_suppressions, is_active

ACTIVE = {"bugprone-narrowing-conversions", "readability-magic-numbers", "google-explicit-constructor"}


def test_active_names_globs_and_analyzer_checks_are_live() -> None:
    assert is_active("google-explicit-constructor", ACTIVE)
    assert is_active("*-magic-numbers", ACTIVE)
    assert is_active("clang-analyzer-core.NullDereference", ACTIVE)
    assert not is_active("hicpp-no-assembler", ACTIVE)
    assert not is_active("cppcoreguidelines-*", ACTIVE)


def test_only_dead_names_are_reported(tmp_path: Path) -> None:
    source = tmp_path / "a.cpp"
    source.write_text(
        "int a = 1;// NOLINT(readability-magic-numbers)\n"
        "// NOLINTNEXTLINE(hicpp-no-assembler,google-explicit-constructor) reason\n"
        'asm volatile("");\n'
        "int b = 2;// NOLINT\n"
    )

    dead = dead_suppressions([source], ACTIVE)

    assert [(d.line, d.check) for d in dead] == [(2, "hicpp-no-assembler")]
