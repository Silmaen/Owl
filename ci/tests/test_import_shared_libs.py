"""`importSharedLibs` resolves dependencies statically and verifies with ldd only as a last resort."""

import importlib.util
import io
import subprocess
import time
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
    buffer = io.StringIO()
    monkeypatch.setattr(isl, "stderr", buffer)
    isl.report_failed_ldd(Path("binary"), 1, "", [])
    err = buffer.getvalue()
    assert "ldd exited with 1" in err and "ldd produced no output." in err
    assert "libfoo.so.1" in err and "libbar.so" in err
    assert "not beside the binary nor in a package root" in err


def test_external_dependencies_selects_only_indexed(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    def fake_needed(_elf: Path) -> list[str]:
        return ["libfoo.so.1", "libc.so.6"]

    monkeypatch.setattr(isl, "declared_dependencies", fake_needed)
    source = Path("/pkg/lib/libfoo.so.1")
    to_copy = isl.external_dependencies(tmp_path / "app", {"libfoo.so.1": source})
    # The system library libc.so.6, absent from the index, is left out.
    assert to_copy == [("libfoo.so.1", source)]


def test_verify_with_ldd_retries_then_succeeds(monkeypatch: pytest.MonkeyPatch) -> None:
    results = [FakeRun(1, b""), FakeRun(1, b""), FakeRun(0, b"")]
    calls = {"n": 0}

    def fake_run(_cmd: list[str], **_: object) -> FakeRun:
        r = results[calls["n"]]
        calls["n"] += 1
        return r

    monkeypatch.setattr(subprocess, "run", fake_run)
    monkeypatch.setattr(time, "sleep", lambda _s: None)
    assert isl.verify_with_ldd(Path("binary"), [], "") == []
    assert calls["n"] == 3


def test_verify_with_ldd_is_inconclusive_after_retries(monkeypatch: pytest.MonkeyPatch) -> None:
    calls = {"n": 0}

    def fake_run(_cmd: list[str], **_: object) -> FakeRun:
        calls["n"] += 1
        return FakeRun(1, b"")

    monkeypatch.setattr(subprocess, "run", fake_run)
    monkeypatch.setattr(time, "sleep", lambda _s: None)
    monkeypatch.setattr(isl, "stderr", io.StringIO())
    # Inconclusive (ldd never ran): returns None instead of exiting, so the static import is trusted.
    assert isl.verify_with_ldd(Path("binary"), [], "") is None
    assert calls["n"] >= 3


def test_verify_with_ldd_reports_not_found(monkeypatch: pytest.MonkeyPatch) -> None:
    output = b"\tlibfoo.so.1 => not found\n\tlibc.so.6 => /lib/libc.so.6 (0x0)\n"

    def fake_run(_cmd: list[str], **_: object) -> FakeRun:
        return FakeRun(0, output)

    monkeypatch.setattr(subprocess, "run", fake_run)
    assert isl.verify_with_ldd(Path("binary"), [], "") == ["libfoo.so.1"]
