"""Source roots of the CodeStyle gate: all of them exist, and a stale one fails instead of scanning nothing."""

from pathlib import Path

import pytest

from ci import root
from ci.actions.code_style import API_DOC_ROOTS, SOURCE_ROOTS, _iter_sources


def test_every_declared_root_exists() -> None:
    assert all(r.is_dir() for r in SOURCE_ROOTS)
    assert root / "test" in SOURCE_ROOTS
    assert root / "test" not in API_DOC_ROOTS


def test_a_missing_root_is_an_error(tmp_path: Path) -> None:
    with pytest.raises(FileNotFoundError):
        _iter_sources([tmp_path / "gone"], (".cpp",))
