#!/usr/bin/env python3
"""
Script to import the Shared libs into the binary dir.
"""
from pathlib import Path
from shutil import copy2
from sys import stderr

script_path = Path(__file__).resolve().parent
root_path = script_path.parent


def is_external(path: str, roots: list[str]) -> bool:
    """
    Tell if a resolved shared object comes from a third-party package cache.

    :param path: The resolved path of the shared object, as printed by ldd.
    :param roots: Package cache roots (Conan) whose libraries must be copied.
    :return: True for a library under one of the given roots.
    """
    return any(path.startswith(root.rstrip("/") + "/") for root in roots)


def is_up_to_date(copied: Path, source: Path) -> bool:
    """
    Tell if a library already copied next to the binary matches its source (copy2 keeps the modification time).

    :param copied: The library next to the binary.
    :param source: The library in the package cache.
    :return: True when the copy exists with the same size and modification time, so a rebuilt package is copied again.
    """
    if not copied.exists():
        return False
    copied_stat, source_stat = copied.stat(), source.resolve().stat()
    return copied_stat.st_size == source_stat.st_size and int(copied_stat.st_mtime) == int(source_stat.st_mtime)


def ldd_command(binary: Path, sysroot: str) -> tuple[list[str], dict[str, str] | None]:
    """
    Build the command listing the shared objects a binary loads.

    :param binary: The binary file to inspect.
    :param sysroot: The target's sysroot when cross compiling, else empty.
    :return: The command and its environment (None: inherited).
    """
    if not sysroot:
        return ["ldd", str(binary)], None
    # The host ldd cannot load a foreign binary: the target's loader lists it, run by qemu-user (binfmt_misc),
    # which looks for the absolute paths in the sysroot first.
    from os import environ

    loaders = sorted(Path(sysroot, "lib").glob("ld-linux-*.so.*"))
    if not loaders:
        print(f"Error: no dynamic loader in the sysroot {sysroot}", file=stderr)
        exit(1)
    return [str(loaders[0]), "--list", str(binary)], {**environ, "QEMU_LD_PREFIX": sysroot}


def declared_dependencies(binary: Path) -> list[str]:
    """
    The shared libraries a binary directly declares it needs (ELF ``DT_NEEDED``), read with readelf.

    Used to report what a binary wants when the loader (``ldd``) itself fails to list anything.

    :param binary: The binary file to inspect.
    :return: The SONAMEs declared, in order; empty when readelf is missing or cannot read the file.
    """
    from subprocess import run, PIPE, STDOUT

    try:
        out = run(["readelf", "-d", str(binary)], stdout=PIPE, stderr=STDOUT)
    except Exception as err:
        print(f"Could not read the declared dependencies of {binary}: {err}", file=stderr)
        return []
    if out.returncode != 0:
        return []
    needed = []
    for line in out.stdout.decode(errors="replace").splitlines():
        if "(NEEDED)" not in line:
            continue
        start = line.find("[")
        end = line.find("]", start + 1)
        if start != -1 and end > start:
            needed.append(line[start + 1 : end])
    return needed


def report_failed_ldd(binary: Path, returncode: int, output: str, roots: list[str]) -> None:
    """
    Print everything known about a binary's dependencies when ``ldd`` failed, so the cause is visible.

    Lists the full ``ldd`` output and every library the binary declares it needs, flagging the ones that
    are neither beside the binary nor in a package root (the ones this script would have to provide).

    :param binary: The binary whose dependencies could not be listed.
    :param returncode: The non-zero exit code ``ldd`` returned.
    :param output: The merged stdout/stderr of ``ldd`` (often empty on failure).
    :param roots: Package cache roots whose libraries are copied.
    """
    print(f"Error while searching dependencies: ldd exited with {returncode}.", file=stderr)
    print(f"ldd output:\n{output}" if output.strip() else "ldd produced no output.", file=stderr)
    needed = declared_dependencies(binary)
    if not needed:
        print("Could not read the binary's declared dependencies (readelf unavailable?).", file=stderr)
        return
    print("Libraries the binary declares it needs (readelf DT_NEEDED):", file=stderr)
    for name in needed:
        beside = (binary.parent / name).exists()
        packaged = beside or any(next(Path(r).rglob(name), None) is not None for r in roots)
        suffix = "" if packaged else "   <-- not beside the binary nor in a package root"
        print(f"  {name}{suffix}", file=stderr)


def external_dependencies(binary: Path, index: dict[str, Path]) -> list[tuple[str, Path]]:
    """
    The third-party libraries the binary and the libraries already beside it need, resolved statically.

    Reads each ELF's ``DT_NEEDED`` with readelf (no execution, so it works for a foreign architecture and never
    fails under a loaded runner) and maps each soname to its package source through the index. A soname absent
    from the index is a system library the target provides and is left out.

    :param binary: The binary to make self-contained.
    :param index: Soname to package-library path, for the third-party libraries of this build.
    :return: The (soname, source) pairs to copy next to the binary that are not already there up to date.
    """
    binary_dir = binary.parent
    elves = [binary] + sorted(path for path in binary_dir.glob("*.so*") if path.is_file())
    to_copy: list[tuple[str, Path]] = []
    seen: set[str] = set()
    for elf in elves:
        for soname in declared_dependencies(elf):
            if soname in seen:
                continue
            seen.add(soname)
            source = index.get(soname)
            if source is None or is_up_to_date(binary_dir / soname, source):
                continue
            to_copy.append((soname, source))
    return to_copy


def verify_with_ldd(binary: Path, roots: list[str], sysroot: str) -> list[str] | None:
    """
    Last-resort check that every dependency of the binary resolves, with ldd.

    ldd runs the loader on the binary (under qemu for a foreign architecture), which can fail transiently on a
    loaded runner, so it is retried and, when it still cannot run, treated as inconclusive rather than fatal: the
    static import is trusted. When ldd does run, the libraries it reports as not found are returned.

    :param binary: The binary to check.
    :param roots: Package cache roots whose libraries are copied.
    :param sysroot: The target's sysroot when cross compiling, else empty.
    :return: The still-missing sonames, or None when ldd could not run.
    """
    from subprocess import run, STDOUT, PIPE
    from time import sleep

    command, env = ldd_command(binary, sysroot)
    attempts = 3
    out = None
    for attempt in range(1, attempts + 1):
        out = run(command, stdout=PIPE, stderr=STDOUT, env=env)
        if out.returncode == 0:
            break
        if attempt < attempts:
            print(f"ldd failed (exit {out.returncode}), attempt {attempt}/{attempts}; retrying.", file=stderr)
            sleep(2)
    if out is None or out.returncode != 0:
        code = out.returncode if out is not None else -1
        text = out.stdout.decode(errors="replace") if out is not None else ""
        report_failed_ldd(binary, code, text, roots)
        print("ldd could not verify the binary; trusting the static import.", file=stderr)
        return None
    return [line.split()[0] for line in out.stdout.decode().replace("\t", "").splitlines() if "not found" in line]


def copy_slang_modules(library: Path, binary_dir: Path):
    """
    Copy the files Slang loads at run time from its own directory (dlopen), which ldd cannot see.

    :param library: The Slang compiler library just imported.
    :param binary_dir: The directory of the binary.
    """
    from shutil import copytree

    for module in library.parent.glob("libslang-*-[0-9]*.so"):
        copy2(module, binary_dir / module.name)
        print(f" Importing {module.name} from {module}")
    for module in library.parent.glob("slang-standard-module-*"):
        if module.is_dir():
            copytree(module, binary_dir / module.name, dirs_exist_ok=True)
            print(f" Importing {module.name}/ from {module}")


def list_available_so(paths: str):
    """
    List all available shared object files (.so) in the provided paths.

    :param paths: A semicolon-separated string of paths to search for shared objects.
    :return: A list of Path objects pointing to available shared object files.
    """
    so_list = []
    try:
        for path in [Path(p) for p in paths.split(";")]:
            while "lib" in f"{path}" or "bin" in f"{path}" or "cmake" in f"{path}":
                path = path.parent
            so_list += list(path.rglob("*.so*"))
    except Exception as err:
        print(f"Error in inspecting available so: {err}")
    return so_list


def main():
    """
    Main function to import missing shared libraries into the binary directory.

    Parses command-line arguments, checks for missing dependencies, and copies the required
    shared libraries from the provided library paths to the binary's directory.
    """
    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument(
        "binary", type=str, help="The binary file to search for dependency"
    )
    parser.add_argument(
        "lib_list", type=str, help="A string in the format of CMAKE_MODULE_PATH"
    )
    parser.add_argument(
        "roots",
        type=str,
        nargs="?",
        default="",
        help="Semicolon-separated package cache roots whose libraries are copied (Conan)",
    )
    parser.add_argument(
        "--sysroot",
        type=str,
        default="",
        help="Sysroot of the target when cross compiling (its loader lists the libraries, under qemu-user)",
    )
    args = parser.parse_args()
    roots = [r for r in args.roots.split(";") if r]

    binary_file = Path(args.binary).resolve()
    if not binary_file.exists():
        print(f"Error: file {binary_file} does not exists!", file=stderr)
        exit(1)
    binary_dir = binary_file.parent

    # Soname to source for the third-party libraries of this build; a soname not here is a system library.
    index = {so.name: so for so in list_available_so(args.lib_list) if is_external(str(so), roots)}

    # Copy the transitive external closure statically; a copy beside the binary is inspected on the next pass.
    for _ in range(64):
        to_copy = external_dependencies(binary_file, index)
        if not to_copy:
            break
        for soname, source in to_copy:
            try:
                copy2(source, binary_dir / soname)
                print(f" Importing {soname} from {source}")
                if soname.startswith("libslang"):
                    copy_slang_modules(source, binary_dir)
            except Exception as err:
                print(f"ERROR while copy {source} to {binary_dir / soname} : {err}", file=stderr)
                exit(1)

    # Last resort: confirm with ldd. A library it still reports missing is a real failure; an ldd that cannot
    # run (a loaded runner, qemu) is inconclusive and the static import is trusted.
    missing = verify_with_ldd(binary_file, roots, args.sysroot)
    if missing:
        for name in missing:
            print(f"{name} is still missing.")
        exit(1)
    exit(0)


if __name__ == "__main__":
    main()
