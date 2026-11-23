"""Runner archives of a release, `runners.json` and the Owl Nest packages completed with them."""

import json
import tarfile
import zipfile
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest

from ci.actions.assemble_nest import AssembleNest
from ci.actions.package_runner import PackageRunner
from ci.actions.publish_package import archive_flavor, find_archives
from ci.utils import runners
from ci.utils.runners import TARGETS, archive_name, make_manifest, runner_files, sha256_of, target_of, write_archive

VERSION = "0.8.0"
HASH = "abcdef1"


def touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def test_every_target_has_one_archive_name() -> None:
    for target in TARGETS:
        name = archive_name(VERSION, HASH, target)
        assert target_of(Path(name), VERSION, HASH) == target
    assert archive_name(VERSION, HASH, "windows-x64") == "OwlGameRunner-0.8.0-abcdef1-windows-x64.tar.gz"
    assert target_of(Path("OwlGameRunner-0.8.0-1234567-web.tar.gz"), VERSION, HASH) is None


def test_a_desktop_runner_takes_its_libraries_but_not_the_shader_compiler(tmp_path: Path) -> None:
    bin_dir = tmp_path / "bin"
    for name in ["OwlGameRunner", "libOwlEngine.so", "libyaml.so.0.8", "libslang.so", "OwlNest", "readme.txt"]:
        touch(bin_dir / name)
    files = [name for _, name in runner_files(bin_dir, "linux-x64")]
    assert files == ["OwlGameRunner", "libOwlEngine.so", "libyaml.so.0.8"]
    assert runner_files(bin_dir, "windows-x64") == []


def test_the_web_runner_needs_its_three_files(tmp_path: Path) -> None:
    bin_dir = tmp_path / "bin"
    touch(bin_dir / "OwlGameRunner.html")
    touch(bin_dir / "OwlGameRunner.js")
    assert runner_files(bin_dir, "web") == []
    touch(bin_dir / "OwlGameRunner.wasm")
    assert [name for _, name in runner_files(bin_dir, "web")] == list(runners.WEB_RUNNER_FILES)


def test_the_android_runner_is_the_stripped_libraries_of_the_apk(tmp_path: Path) -> None:
    touch(tmp_path / "android-runner" / "lib" / "arm64-v8a" / "libOwlGameRunner.so")
    touch(tmp_path / "android-runner" / "lib" / "arm64-v8a" / "libc++_shared.so")
    names = [name for _, name in runner_files(tmp_path / "bin", "android-arm64")]
    assert names == ["lib/arm64-v8a/libOwlGameRunner.so", "lib/arm64-v8a/libc++_shared.so"]


def test_package_runner_writes_the_archive_of_the_preset_target(tmp_path: Path) -> None:
    touch(tmp_path / "bin" / "OwlGameRunner")
    touch(tmp_path / "bin" / "libOwlEngine.so")
    preset: Any = SimpleNamespace(cmake_preset="linux-clang-release", runner_target="linux-x64")
    preset.get_build_dir = lambda: tmp_path
    assert PackageRunner().run(preset, [f"--hash={HASH}"]) == 0
    archive = tmp_path / archive_name(runners_version(), HASH, "linux-x64")
    with tarfile.open(archive) as content:
        assert sorted(content.getnames()) == ["OwlGameRunner", "libOwlEngine.so"]


def test_package_runner_refuses_a_preset_without_runner(masked_caplog: pytest.LogCaptureFixture) -> None:
    preset: Any = SimpleNamespace(cmake_preset="linux-gcc-debug", runner_target=None)
    assert PackageRunner().run(preset, []) == 1
    assert "no runner target" in masked_caplog.text


def test_the_manifest_gives_size_and_checksum_by_target(tmp_path: Path) -> None:
    web = touch(tmp_path / archive_name(VERSION, HASH, "web"), b"web")
    linux = touch(tmp_path / archive_name(VERSION, HASH, "linux-x64"), b"linux!")
    touch(tmp_path / "OwlNest-0.8.0-abcdef1-linux-x64.tar.gz")
    manifest = make_manifest([web, linux, tmp_path / "OwlNest-0.8.0-abcdef1-linux-x64.tar.gz"], VERSION, HASH)
    assert list(manifest["runners"]) == ["linux-x64", "web"]
    assert manifest["runners"]["linux-x64"] == {"file": linux.name, "size": 6, "sha256": sha256_of(linux)}
    assert (manifest["version"], manifest["hash"]) == (VERSION, HASH)


def test_runner_archives_are_published_with_their_target_as_flavor(tmp_path: Path) -> None:
    touch(tmp_path / archive_name(VERSION, HASH, "android-arm64"))
    touch(tmp_path / f"OwlNest-{VERSION}-{HASH}-windows-x64.zip")
    found = find_archives(tmp_path, VERSION, HASH)
    assert [base for base, _ in found] == ["OwlGameRunner", "OwlNest"]
    assert archive_flavor(found[0][1], "OwlGameRunner", VERSION, HASH) == "android-arm64"


def runners_version() -> str:
    from ci.utils.publish import get_project_version

    return get_project_version()


def make_release(folder: Path, version: str) -> None:
    for target in TARGETS:
        if target == "web":
            web = folder / "web" / "bin"
            for name in runners.WEB_RUNNER_FILES:
                touch(web / name, name.encode())
            write_archive(runner_files(web, "web"), folder / archive_name(version, HASH, target))
        else:
            touch(folder / "jobs" / target / archive_name(version, HASH, target), target.encode())
    linux = folder / "nest" / f"OwlNest-{version}-{HASH}-linux-glibc_2.43-x64"
    touch(linux / "bin" / "OwlNest")
    with tarfile.open(folder / f"{linux.name}.tar.gz", "w:gz") as archive:
        archive.add(linux, linux.name)
    with zipfile.ZipFile(folder / f"OwlNest-{version}-{HASH}-windows-x64.zip", "w") as archive:
        archive.writestr(f"OwlNest-{version}-{HASH}-windows-x64/bin/OwlNest.exe", b"exe")


def test_assemble_nest_adds_the_manifest_and_the_web_runner_to_every_nest(tmp_path: Path) -> None:
    version = runners_version()
    make_release(tmp_path / "in", version)
    preset: Any = SimpleNamespace(cmake_preset="linux-clang-release")
    args = [f"--input={tmp_path / 'in'}", f"--output={tmp_path / 'out'}", f"--hash={HASH}"]
    assert AssembleNest().run(preset, args) == 0

    manifest = json.loads((tmp_path / "out" / "runners.json").read_text())
    assert sorted(manifest["runners"]) == sorted(TARGETS)
    root = f"OwlNest-{version}-{HASH}-linux-glibc_2.43-x64"
    with tarfile.open(tmp_path / "out" / f"{root}.tar.gz") as archive:
        names = archive.getnames()
        assert f"{root}/bin/OwlNest" in names
        assert f"{root}/bin/web/OwlGameRunner.wasm" in names
        packed = archive.extractfile(f"{root}/bin/runners.json")
        assert packed is not None and json.loads(packed.read()) == manifest
    with zipfile.ZipFile(tmp_path / "out" / f"OwlNest-{version}-{HASH}-windows-x64.zip") as archive:
        assert archive.read(f"OwlNest-{version}-{HASH}-windows-x64/bin/web/OwlGameRunner.js") == b"OwlGameRunner.js"
        assert f"OwlNest-{version}-{HASH}-windows-x64/bin/runners.json" in archive.namelist()


def test_assemble_nest_needs_every_runner(tmp_path: Path, masked_caplog: pytest.LogCaptureFixture) -> None:
    version = runners_version()
    make_release(tmp_path / "in", version)
    (tmp_path / "in" / "jobs" / "linux-arm64" / archive_name(version, HASH, "linux-arm64")).unlink()
    preset: Any = SimpleNamespace(cmake_preset="linux-clang-release")
    args = [f"--input={tmp_path / 'in'}", f"--output={tmp_path / 'out'}", f"--hash={HASH}"]
    assert AssembleNest().run(preset, args) == 1
    assert "no linux-arm64 runner" in masked_caplog.text
