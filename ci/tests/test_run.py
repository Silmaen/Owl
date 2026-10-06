"""Output pumping of `run_command`."""

import sys

from ci.utils.run import run_command


def test_large_stderr_neither_blocks_nor_loses_the_exit_code() -> None:
    # 2 MB on stderr before any stdout: far beyond a pipe buffer, deadlocks a sequential reader.
    script = "import sys\nfor _ in range(2000): sys.stderr.write('y' * 999 + '\\n')\nprint('done')\nsys.exit(3)"
    assert run_command([sys.executable, "-c", script]) == 3
