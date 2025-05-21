#!/usr/bin/env python3
"""
Script to check and manage remote dependencies using depmanager.
"""
from pathlib import Path
from sys import stderr


class HostInfo:
    """
    Host system information.
    """
    def __init__(self):
        self.os = "unknown"
        self.arch = "unknown"
        self.release = ""
        self.libc = ""
        self.__analyse()

    def is_valid(self) -> bool:
        """
        Check if the host info is valid.

        :return: True if valid, False otherwise.
        """
        if self.os in [None, "", "unknown"]:
            return False
        if self.arch in [None, "", "unknown"]:
            return False
        return True

    def __analyse(self):
        """
        Analyse the host system to fill in OS, arch, release, and libc.
        :return: None
        """
        import platform

        if Path("/etc/os-release").exists():
            rel = {}
            with open(Path("/etc/os-release"), "r") as fp:
                lines = fp.readlines()
                for line in lines:
                    line = line.replace('"', "")
                    it = line.split("=", 1)
                    rel[it[0]] = it[1]
            self.release = f"{rel['NAME']} {rel['VERSION']}"
            self.libc = " ".join(platform.libc_ver())
        self.os = platform.system()
        self.arch = platform.machine()
        if self.arch == "AMD64":
            self.arch = "x86_64"


hi = HostInfo()


def has_depmanager():
    """
    Check if depmanager is available.

    :return: True if depmanager is available, exits otherwise.
    """
    import subprocess

    try:
        ret = subprocess.run("depmanager info version", shell=True).returncode
        if ret != 0:
            print(f"Error running depmanager ({ret}).", file=stderr)
            exit(1)
    except Exception as err:
        print(f"Error Exception while running depmanager, {err}.", file=stderr)
        exit(1)
    return True


def check_remote(remote: dict):
    """
    Check and add a remote if needed.

    :param remote: Dictionary with remote info.
    :return: None
    """
    if "url" not in remote:
        return
    if remote["url"] in ["", None]:
        return
    from depmanager.api.remotes import RemotesManager

    remotes = RemotesManager(verbosity=4)
    rem = remotes.get_safe_remote(name=remote["name"])
    if rem is None:
        url = remote["url"]
        kind = "ftp"
        if "://" in url:
            kind, url = url.split("://", 1)
            url = url.replace("/", "")
        remotes.add_remote(
            name=remote["name"],
            url=url,
            kind=kind,
            default=True,
            login=remote["login"],
            passwd=remote["passwd"],
        )


def main():
    """
    Main entry point for dependency check.

    :return: None
    """
    from argparse import ArgumentParser as ap

    arg_p = ap()
    arg_p.add_argument("--remote-url", "-r", type=str, help="Remote url to use")
    arg_p.add_argument("--remote-login", "-l", type=str, help="Remote login to use")
    arg_p.add_argument("--remote-passwd", "-p", type=str, help="Remote password to use")
    args = arg_p.parse_args()
    has_depmanager()
    remote = {
        "name": "remote",
        "url": args.remote_url,
        "login": args.remote_login,
        "passwd": args.remote_passwd,
    }
    check_remote(remote)


if __name__ == "__main__":
    main()
