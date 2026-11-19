#!/usr/bin/env python3
"""
Package the Android game runner and an exported game folder into a signed APK.

The runner library and every shared library it needs (resolved with readelf from the build, the Conan packages and the
NDK) go to `lib/arm64-v8a/`; the game folder (`OwlNest --export --target android`) goes to `assets/` with the file list
`owl_assets.txt` that the engine extracts at the first launch (platform/AndroidApp.h). The APK is linked by aapt2,
aligned by zipalign and signed by apksigner, with the debug key unless a keystore is given.
"""

from __future__ import annotations

import hashlib
import subprocess
import zipfile
from argparse import ArgumentParser
from pathlib import Path
from sys import stderr

ABI = "arm64-v8a"
ASSET_LIST = "owl_assets.txt"
# Libraries of the system image: never packed.
SYSTEM_LIBS = {
    "libc.so",
    "libm.so",
    "libdl.so",
    "liblog.so",
    "libandroid.so",
    "libvulkan.so",
    "libOpenSLES.so",
    "libaaudio.so",
    "libEGL.so",
    "libGLESv2.so",
    "libGLESv3.so",
    "libmediandk.so",
    "libnativewindow.so",
    "libz.so",
}

MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="{package}"
    android:versionCode="{version_code}"
    android:versionName="{version_name}">
    <uses-sdk android:minSdkVersion="{min_sdk}" android:targetSdkVersion="{target_sdk}" />
    <uses-feature android:name="android.hardware.vulkan.version" android:version="0x403000" android:required="true" />
    <uses-permission android:name="android.permission.INTERNET" />
    <application android:label="{label}" android:hasCode="false" android:extractNativeLibs="true"
        android:debuggable="{debuggable}">
        <activity android:name="android.app.NativeActivity" android:exported="true"
            android:configChanges="orientation|screenSize|screenLayout|keyboardHidden|keyboard|navigation|uiMode|density"
            android:screenOrientation="sensorLandscape"
            android:theme="@android:style/Theme.NoTitleBar.Fullscreen">
            <meta-data android:name="android.app.lib_name" android:value="{lib_name}" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
"""


def needed_libraries(readelf: Path, library: Path) -> list[str]:
    """
    Shared libraries a library links against.

    :param readelf: The readelf of the NDK.
    :param library: The library to read.
    :return: The DT_NEEDED names.
    """
    out = subprocess.run([str(readelf), "-d", str(library)], capture_output=True, text=True, check=True).stdout
    names = []
    for line in out.splitlines():
        if "(NEEDED)" in line and "[" in line:
            names.append(line[line.index("[") + 1 : line.index("]")])
    return names


def collect_libraries(readelf: Path, runner: Path, search_dirs: list[Path]) -> list[Path]:
    """
    The runner and the libraries it needs, recursively, outside the system image.

    :param readelf: The readelf of the NDK.
    :param runner: The runner library.
    :param search_dirs: Directories searched for the libraries, in order.
    :return: The libraries to pack.
    """
    found: dict[str, Path] = {runner.name: runner}
    pending = [runner]
    while pending:
        for name in needed_libraries(readelf, pending.pop()):
            if name in found or name in SYSTEM_LIBS:
                continue
            candidate = next((d / name for d in search_dirs if (d / name).is_file()), None)
            if candidate is None:
                print(f"Error: shared library {name} not found in {[str(d) for d in search_dirs]}", file=stderr)
                raise SystemExit(1)
            found[name] = candidate.resolve()
            pending.append(found[name])
    return list(found.values())


def conan_library_dirs(conan_dir: Path) -> list[Path]:
    """
    Library folders of the packages of one Conan install, from the data files of its CMakeDeps generator.

    :param conan_dir: The output folder of the install.
    :return: The `lib` folders of the packages.
    """
    dirs = []
    for data in sorted(conan_dir.glob("*-data.cmake")):
        for line in data.read_text().splitlines():
            if "_PACKAGE_FOLDER_" in line and line.startswith("set("):
                folder = Path(line.split('"')[1])
                if (folder / "lib").is_dir():
                    dirs.append(folder / "lib")
    return dirs


def asset_list(game_dir: Path) -> tuple[list[Path], str]:
    """
    The files of the game folder and the list the engine extracts.

    :param game_dir: The exported game folder.
    :return: The relative paths, and the list text whose first line identifies the content.
    """
    files = sorted(p.relative_to(game_dir) for p in game_dir.rglob("*") if p.is_file())
    digest = hashlib.sha256()
    for relative in files:
        digest.update(relative.as_posix().encode())
        digest.update((game_dir / relative).read_bytes())
    lines = [digest.hexdigest()] + [p.as_posix() for p in files]
    return files, "\n".join(lines) + "\n"


def debug_keystore(path: Path) -> Path:
    """
    The debug keystore, created as the Android tools create it when missing.

    :param path: Where the keystore lives.
    :return: The keystore path.
    """
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "keytool", "-genkeypair", "-keystore", str(path), "-storepass", "android", "-keypass", "android",
                "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
                "-dname", "CN=Android Debug,O=Android,C=US",
            ],
            check=True,
            capture_output=True,
        )
    return path


def main() -> int:
    """
    Build the APK from the command-line arguments.

    :return: The exit code.
    """
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("runner", type=Path, help="The runner library (libOwlGameRunner.so)")
    parser.add_argument("game", type=Path, help="The game folder exported for Android")
    parser.add_argument("output", type=Path, help="The APK to write")
    parser.add_argument("--conan", type=Path, required=True, help="Output folder of the Conan install of the build")
    parser.add_argument("--ndk", type=Path, required=True, help="The Android NDK")
    parser.add_argument("--sdk", type=Path, required=True, help="The Android SDK")
    parser.add_argument("--build-tools", default="36.0.0", help="Version of the SDK build tools")
    parser.add_argument("--platform", default="android-36", help="SDK platform whose android.jar links the APK")
    parser.add_argument("--min-sdk", default="33", help="Lowest Android API level")
    parser.add_argument("--package", default="net.argawaen.owl.game", help="Android package name")
    parser.add_argument("--label", default="Owl Game", help="Name shown by the launcher")
    parser.add_argument("--version-name", default="0.0.0", help="Version shown to the user")
    parser.add_argument("--version-code", default="1", help="Integer version for the store")
    parser.add_argument("--keystore", type=Path, help="Release keystore (default: the debug key)")
    parser.add_argument("--key-alias", default="androiddebugkey", help="Alias of the key in the keystore")
    parser.add_argument("--store-pass", default="android", help="Keystore password (pass:, env: or file: forms work)")
    args = parser.parse_args()

    tools = args.sdk / "build-tools" / args.build_tools
    readelf = next((args.ndk / "toolchains" / "llvm" / "prebuilt").glob("*/bin/llvm-readelf"))
    strip = readelf.with_name("llvm-strip")
    libcxx_dir = next((args.ndk / "toolchains" / "llvm" / "prebuilt").glob("*/sysroot/usr/lib/aarch64-linux-android"))
    search_dirs = [args.runner.parent, *conan_library_dirs(args.conan), libcxx_dir]
    if not args.game.is_dir():
        print(f"Error: no exported game in {args.game}", file=stderr)
        return 1

    work = args.output.parent / f"{args.output.stem}.apk.d"
    work.mkdir(parents=True, exist_ok=True)
    manifest = work / "AndroidManifest.xml"
    manifest.write_text(
        MANIFEST.format(
            package=args.package,
            version_code=args.version_code,
            version_name=args.version_name,
            min_sdk=args.min_sdk,
            target_sdk=args.platform.removeprefix("android-"),
            label=args.label,
            debuggable="false" if args.keystore else "true",
            lib_name=args.runner.stem.removeprefix("lib"),
        )
    )
    unaligned = work / "unaligned.apk"
    subprocess.run(
        [str(tools / "aapt2"), "link", "-o", str(unaligned), "--manifest", str(manifest),
         "-I", str(args.sdk / "platforms" / args.platform / "android.jar")],
        check=True,
    )

    libraries = collect_libraries(readelf, args.runner, search_dirs)
    files, listing = asset_list(args.game)
    # The NDK compiles with -g in every configuration: the packed copies lose their debug sections, as Gradle does.
    stripped_dir = work / "lib"
    stripped_dir.mkdir(exist_ok=True)
    with zipfile.ZipFile(unaligned, "a", zipfile.ZIP_DEFLATED) as apk:
        for library in libraries:
            stripped = stripped_dir / library.name
            subprocess.run([str(strip), "--strip-unneeded", "-o", str(stripped), str(library)], check=True)
            apk.write(stripped, f"lib/{ABI}/{library.name}")
        for relative in files:
            apk.write(args.game / relative, f"assets/{relative.as_posix()}")
        apk.writestr(f"assets/{ASSET_LIST}", listing)

    aligned = work / "aligned.apk"
    subprocess.run([str(tools / "zipalign"), "-f", "-P", "16", "4", str(unaligned), str(aligned)], check=True)
    keystore = args.keystore or debug_keystore(Path.home() / ".android" / "debug.keystore")
    store_pass = args.store_pass if ":" in args.store_pass else f"pass:{args.store_pass}"
    subprocess.run(
        [str(tools / "apksigner"), "sign", "--ks", str(keystore), "--ks-key-alias", args.key_alias,
         "--ks-pass", store_pass, "--out", str(args.output), str(aligned)],
        check=True,
    )
    size = args.output.stat().st_size / (1024 * 1024)
    print(f"APK {args.output}: {len(libraries)} libraries, {len(files)} game files, {size:.1f} MiB")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
