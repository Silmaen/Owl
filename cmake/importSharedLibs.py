#!/usr/bin/env python3
"""
Script to import the Shared libs into the binary dir.
"""
from pathlib import Path
from shutil import copy2
from sys import stderr

script_path = Path(__file__).resolve().parent
root_path = script_path.parent


def list_missing_so(binary: Path, has_missing: bool):
    """
    List missing and external shared object dependencies for a given binary.

    :param binary: The binary file to inspect.
    :param has_missing: Whether to check for missing dependencies.
    :return: A tuple containing a list of missing shared objects and a list of external shared objects.
    """
    from subprocess import run, STDOUT, PIPE

    try:
        out = run(f"ldd {binary}", shell=True, stdout=PIPE, stderr=STDOUT)
        if out.returncode != 0:
            print(
                f"Error while searching dependencies: {out.returncode}\n{out.stdout.decode()}",
                file=stderr,
            )
            exit(1)
        lines = out.stdout.decode().replace("\t", "").splitlines(keepends=False)
        missing_so = []
        if has_missing:
            for line in lines:
                if "not found" not in line:
                    continue
                missing_so.append(line.split()[0])
        extern_so = []
        for line in lines:
            if ".edm" not in line:
                continue
            items = line.split()
            if (binary.parent / items[0]).exists():
                continue
            extern_so.append([items[0], items[2]])
        return missing_so, extern_so
    except Exception as err:
        print(f"Exception while searching dependencies: {err}", file=stderr)
        exit(1)


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
    args = parser.parse_args()

    binary_file = Path(args.binary).resolve()
    if not binary_file.exists():
        print(f"Error: file {binary_file} does not exists!", file=stderr)
        exit(1)
    binary_dir = binary_file.parent
    lib_list = args.lib_list
    has_missing = True
    if lib_list in ["", None]:
        has_missing = False

    missing, extern = list_missing_so(binary_file, has_missing)
    not_found = []
    if len(missing) > 0:
        available_so = list_available_so(lib_list)
    else:
        available_so = []
    go_on = True
    while go_on:
        go_on = False
        missing, extern = list_missing_so(binary_file, has_missing)
        if len(missing) + len(extern) == 0:
            break
        for miss in missing:
            if miss in not_found:
                continue
            found = False
            for av in available_so:
                if miss == av.name:
                    try:
                        copy2(av, binary_dir / miss)
                        print(f" Importing {miss} from {av}")
                    except Exception as err:
                        print(
                            f"ERROR while copy {av} to {binary_dir / miss} : {err}",
                            file=stderr,
                        )
                        exit(1)
                    go_on = True
                    found = True
                    break
            if not found:
                not_found.append(miss)
                print(f"Warning: dependency {miss}, not found.", file=stderr)
        for ext in extern:
            try:
                copy2(ext[1], binary_dir / ext[0])
                print(f" Importing {ext[0]} from {ext[1]}")
            except Exception as err:
                print(
                    f"ERROR while copy {ext[1]} to {binary_dir / ext[0]} : {err}",
                    file=stderr,
                )
                exit(1)
    missing, extern = list_missing_so(binary_file, has_missing)
    if len(missing) + len(extern) == 0:
        exit(0)
    else:
        for miss in missing:
            print(f"{miss} is still missing.")
        exit(1)


if __name__ == "__main__":
    main()
