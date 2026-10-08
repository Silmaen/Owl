"""Parsing of the ruff and mypy outputs by the `CodeStyle` python sub-check."""

from ci.utils.python_lint import Finding, parse_mypy, parse_pytest, parse_ruff, parse_ruff_format


def test_ruff_diagnostics_are_read_and_the_summary_ignored() -> None:
    output = "ci/a.py:3:5: F401 [*] `os` imported but unused\nci/b.py:10:1: E501 Line too long\nFound 2 errors.\n"

    assert parse_ruff(output) == [
        Finding("ruff", "ci/a.py", 3, 5, "F401 [*] `os` imported but unused"),
        Finding("ruff", "ci/b.py", 10, 1, "E501 Line too long"),
    ]


def test_every_file_to_reformat_is_a_finding() -> None:
    output = "Would reformat: ci/a.py\nWould reformat: ci_action.py\n2 files would be reformatted\n"

    assert [f.path for f in parse_ruff_format(output)] == ["ci/a.py", "ci_action.py"]


def test_only_mypy_errors_are_findings() -> None:
    output = (
        'ci/a.py:7: error: Name "x" is not defined  [name-defined]\n'
        "ci/a.py:7: note: See https://mypy.readthedocs.io\n"
        "Success: no issues found in 1 source file\n"
    )

    assert parse_mypy(output) == [Finding("mypy", "ci/a.py", 7, 1, 'Name "x" is not defined  [name-defined]')]


def test_failed_pytest_cases_are_findings() -> None:
    output = (
        "..F.\n"
        "FAILED ci/tests/test_a.py::test_one - assert 1 == 2\n"
        "FAILED ci/tests/test_b.py::test_two\n"
        "1 failed, 3 passed in 0.1s\n"
    )

    assert parse_pytest(output) == [
        Finding("pytest", "ci/tests/test_a.py", 1, 1, "test_one failed: assert 1 == 2"),
        Finding("pytest", "ci/tests/test_b.py", 1, 1, "test_two failed"),
    ]
