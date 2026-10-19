"""
Optional Authenticode signing of the Windows executables before CPack archives them.

Off unless a certificate is configured, so an unsigned tree packages as before. Two ways to provide one, both through
the environment of the agent (never the command line, see `.claude/rules/python-ci.md`):

* ``OWL_SIGN_THUMBPRINT``: SHA-1 thumbprint of a certificate already installed in the agent's Windows certificate
  store (OV/EV certificate, or its hardware token); ``signtool`` signs with it, no secret is handled here.
* ``OWL_SIGN_CERTIFICATE``: path to a PKCS#12 (``.pfx``) file, with its password in ``OWL_SIGN_PASSWORD``;
  ``osslsigncode`` signs with it (Windows through MSYS2, or Linux), the password handed over in a private file.

``OWL_SIGN_TIMESTAMP_URL`` overrides the RFC 3161 timestamp server (a timestamped signature stays valid after the
certificate expires). See `doc/pages/building.md#code-signing`.
"""

from __future__ import annotations

import os
import shutil
import tempfile
from pathlib import Path

from ci import log
from ci.utils.run import MODE_BY_CONTENT, run_command
from ci.utils.secrets import get_secret

THUMBPRINT_ENV = "OWL_SIGN_THUMBPRINT"
"""Thumbprint of a certificate of the agent's certificate store (signtool)."""
CERTIFICATE_ENV = "OWL_SIGN_CERTIFICATE"
"""Path to a PKCS#12 certificate file (osslsigncode)."""
PASSWORD_ENV = "OWL_SIGN_PASSWORD"
"""Password of the PKCS#12 file: a secret."""
TIMESTAMP_ENV = "OWL_SIGN_TIMESTAMP_URL"
"""RFC 3161 timestamp server."""
DEFAULT_TIMESTAMP_URL = "http://timestamp.digicert.com"
"""Default timestamp server (the timestamp protocol signs its answer, plain HTTP is the norm)."""

SIGNED_BINARIES: tuple[str, ...] = ("OwlNest.exe", "OwlRunner.exe", "OwlEngine.dll")
"""Owl's own binaries in the build's bin/; third-party DLLs keep their vendor's signature, or none."""


def binaries_to_sign(bin_dir: Path) -> list[Path]:
    """
    List the Owl binaries of a build tree that can be signed.

    :param bin_dir: The build's bin/ folder.
    :return: The Windows binaries found there (none in a Linux tree).
    """
    return [bin_dir / name for name in SIGNED_BINARIES if (bin_dir / name).is_file()]


def sign_with_signtool(files: list[Path], thumbprint: str, timestamp: str) -> int:
    """
    Sign with a certificate of the Windows certificate store.

    :param files: The binaries to sign in place.
    :param thumbprint: SHA-1 thumbprint of the certificate.
    :param timestamp: The timestamp server.
    :return: The exit code of signtool, 1 when it is missing.
    """
    tool = shutil.which("signtool")
    if tool is None:
        log.error(f"Code signing: {THUMBPRINT_ENV} is set but signtool (Windows SDK) is not on PATH.")
        return 1
    command = [tool, "sign", "/sha1", thumbprint, "/fd", "sha256", "/tr", timestamp, "/td", "sha256"]
    return run_command(command + [str(f) for f in files], detection_mode=MODE_BY_CONTENT)


def sign_with_osslsigncode(files: list[Path], certificate: Path, password: str, timestamp: str) -> int:
    """
    Sign with a PKCS#12 file, the password in a private temporary file (never in argv).

    :param files: The binaries to sign in place.
    :param certificate: The .pfx file.
    :param password: Its password (may be empty).
    :param timestamp: The timestamp server.
    :return: 0 when every file is signed.
    """
    tool = shutil.which("osslsigncode")
    if tool is None:
        log.error(f"Code signing: {CERTIFICATE_ENV} is set but osslsigncode is not on PATH.")
        return 1
    if not certificate.is_file():
        log.error(f"Code signing: certificate file {certificate} not found.")
        return 1
    with tempfile.TemporaryDirectory() as private:
        password_file = Path(private) / "pass"
        password_file.touch(mode=0o600)
        password_file.write_text(password)
        for binary in files:
            signed = binary.with_name(binary.name + ".signed")
            command = [tool, "sign", "-pkcs12", str(certificate), "-readpass", str(password_file), "-h", "sha256"]
            command += ["-ts", timestamp, "-in", str(binary), "-out", str(signed)]
            result = run_command(command, detection_mode=MODE_BY_CONTENT)
            if result != 0:
                signed.unlink(missing_ok=True)
                log.error(f"Code signing: osslsigncode failed on {binary.name}.")
                return result
            signed.replace(binary)
    return 0


def sign_binaries(bin_dir: Path) -> int:
    """
    Sign the Owl binaries of a build tree when a certificate is configured, else do nothing.

    :param bin_dir: The build's bin/ folder.
    :return: 0 when signed or when signing is off, non-zero on a signing failure.
    """
    thumbprint = os.environ.get(THUMBPRINT_ENV, "").strip()
    certificate = os.environ.get(CERTIFICATE_ENV, "").strip()
    if not thumbprint and not certificate:
        log.info("Code signing: no certificate configured, the binaries stay unsigned.")
        return 0
    files = binaries_to_sign(bin_dir)
    if not files:
        log.info("Code signing: no Windows binary in this tree, nothing to sign.")
        return 0
    timestamp = os.environ.get(TIMESTAMP_ENV, "").strip() or DEFAULT_TIMESTAMP_URL
    log.info(f"Code signing: {', '.join(f.name for f in files)}.")
    if thumbprint:
        return sign_with_signtool(files, thumbprint, timestamp)
    return sign_with_osslsigncode(files, Path(certificate), get_secret(PASSWORD_ENV), timestamp)
