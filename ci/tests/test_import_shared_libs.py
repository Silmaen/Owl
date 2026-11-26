"""`importSharedLibs` reports every declared dependency when `ldd` fails."""

import importlib.util
import io
import subprocess
from pathlib import Path

import pytest

from ci import root

_spec = importlib.util.spec_from_file_location("importSharedLibs", root / "cmake" / "importSharedLibs.py")
assert _spec is not None and _spec.loader is not None
isl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(isl)


class FakeRun:
    """Minimal stand-in for a :func:`subprocess.run` result."""

    def __init__(self, returncode: int, stdout: bytes) -> None:
        self.returncode = returncode
        self.stdout = stdout


_READELF = (
    b" 0x0000000000000001 (NEEDED) Shared library: [libfoo.so.1]\n"
    b" 0x0000000000000001 (NEEDED) Shared library: [libbar.so]\n"
)


def test_declared_dependencies_parses_readelf(monkeypatch: pytest.MonkeyPatch) -> None:
    def fake_run(_cmd: list[str], **_: object) -> FakeRun:
        return FakeRun(0, _READELF)

    monkeypatch.setattr(subprocess, "run", fake_run)
    assert isl.declared_dependencies(Path("binary")) == ["libfoo.so.1", "libbar.so"]


def test_report_failed_ldd_lists_every_needed_library(monkeypatch: pytest.MonkeyPatch) -> None:
    def fake_run(_cmd: list[str], **_: object) -> FakeRun:
        return FakeRun(0, _READELF)

    monkeypatch.setattr(subprocess, "run", fake_run)
    # The script binds `stderr` at import (`from sys import stderr`), so capture through the module's name.
    buffer = io.StringIO()
    monkeypatch.setattr(isl, "stderr", buffer)
    isl.report_failed_ldd(Path("binary"), 1, "", [])
    err = buffer.getvalue()
    assert "ldd exited with 1" in err
    assert "ldd produced no output." in err
    # Every declared dependency is reported, flagged as not packaged (empty roots).
    assert "libfoo.so.1" in err and "libbar.so" in err
    assert "not beside the binary nor in a package root" in err
