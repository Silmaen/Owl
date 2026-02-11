"""
Utility function for running application commands.
"""

from logging import INFO, WARNING, ERROR

from ci import log

# enum for mode of log level determination
MODE_BY_CONTENT = 0
MODE_BY_COLOR = 1
MODE_FOR_NINJA = 2

# variables to keep track of current and next log levels for color-based detection
current_level = INFO
next_level = INFO

# list of regex patterns to exclude from ninja error detection
ninja_error_exclusions = [
    r"^\[(\d+)/(\d+)\]",  # Ninja build progress [n/m]
    r"^\[\d+%\]",  # Ninja percentage progress [XX%]
    r"^\s*Building ",  # Building target lines
    r"^\s*Linking ",  # Linking target lines
    r"^CPack:.*",
    r"^Scanning dependencies of target .*",
    r"^ Importing .*",
    r"^ninja: no work to do.",
]


def _determine_log_level(line: str, mode: int = MODE_BY_CONTENT) -> int:
    """
    Determine the appropriate log level based on the line content.

    :param line: The log line to analyze.
    :return: The logging level (DEBUG, INFO, WARNING, ERROR).
    """
    import re

    global current_level, next_level
    current_level = next_level
    if mode == MODE_BY_COLOR:
        if "\x1b[31m" in line:  # Red
            current_level = ERROR
            next_level = ERROR
        elif "\x1b[33m" in line:  # Yellow
            current_level = WARNING
            next_level = WARNING
        if "\x1b[0m" in line:  # Green
            next_level = INFO
        return current_level
    elif mode == MODE_FOR_NINJA:
        for pattern in ninja_error_exclusions:
            if re.search(pattern, line):
                return INFO
        else:
            for pattern in ninja_error_exclusions:
                if re.search(pattern, line):
                    return INFO
            return ERROR
    else:
        # old content-based detection (may trigger false positives)
        line_lower = line.lower()
        if re.search(r"\b0\s+(tests?|errors?)\s+(failed|error)", line_lower):
            return INFO
        if re.search(r"\b(error|failed|fatal|exception)\b", line_lower):
            return ERROR
        elif re.search(r"\b(warning|warn|deprecated)\b", line_lower):
            return WARNING
        else:
            return INFO


def _strip_ansi_codes(text: str) -> str:
    """
    Remove ANSI escape codes from the given text.

    :param text: The text containing ANSI codes.
    :return: The cleaned text without ANSI codes.
    """
    import re

    ansi_escape = re.compile(r"\x1B[@-_][0-?]*[ -/]*[@-~]")
    return ansi_escape.sub("", text)


def run_command(command: list[str] | str,
                detection_mode: int = MODE_BY_CONTENT) -> int:
    """
    Runs a potentially long command as a subprocess and logs its output in real-time.

    :param command: The command to run as a list of strings.
    :param detection_mode: Log Level detection mode.
    :return: The exit code of the command.
    """
    import subprocess
    from os import environ

    if isinstance(command, str):
        command = command.split()
    log.info(f"Running command: {' '.join(command)}")
    try:
        env = environ.copy()
        if detection_mode == MODE_BY_COLOR:
            env["CLICOLOR_FORCE"] = "1"
        env["PYTHONUNBUFFERED"] = "1"
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            env=env,
        )
        import select
        from sys import platform

        # Process Outputs in real time
        if platform != "win32":
            import fcntl
            import os as os_module
            fcntl.fcntl(process.stdout, fcntl.F_SETFL, os_module.O_NONBLOCK)
            fcntl.fcntl(process.stderr, fcntl.F_SETFL, os_module.O_NONBLOCK)
            while process.poll() is None:
                reads = [process.stdout, process.stderr]
                ret = select.select(reads, [], [])
                for stream in ret[0]:
                    try:
                        line = stream.readline()
                        if not line:
                            continue
                        line = line.rstrip("\n")
                        if line:
                            is_stderr = stream == process.stderr
                            level = _determine_log_level(line, detection_mode)
                            if is_stderr:
                                level = max(level, WARNING)
                            if detection_mode == MODE_BY_COLOR:
                                line = _strip_ansi_codes(line)
                            log.log(level, line)
                    except (BlockingIOError, IOError):
                        continue  # Capture any remaining output after process ends
            for line in process.stdout:
                line = line.rstrip("\n")
                if line:
                    level = _determine_log_level(line, detection_mode)
                    if detection_mode == MODE_BY_COLOR:
                        line = _strip_ansi_codes(line)
                    log.log(level, line)
            for line in process.stderr:
                line = line.rstrip("\n")
                if line:
                    level = max(_determine_log_level(line, detection_mode), WARNING)
                    if detection_mode == MODE_BY_COLOR:
                        line = _strip_ansi_codes(line)
                    log.log(level, line)
        else:
            for line in process.stdout:
                line = line.rstrip("\n")
                if line:
                    level = _determine_log_level(line, detection_mode)
                    if detection_mode == MODE_BY_COLOR:
                        line = _strip_ansi_codes(line)
                    log.log(level, line)
            for line in process.stderr:
                line = line.rstrip("\n")
                if line:
                    level = max(_determine_log_level(line, detection_mode), WARNING)
                    if detection_mode == MODE_BY_COLOR:
                        line = _strip_ansi_codes(line)
                    log.log(level, line)

        process.wait()
        return process.returncode
    except FileNotFoundError:
        log.error(
            f"Command not found: {command[0]}. Make sure it's installed and in PATH."
        )
        return 1
    except Exception as e:
        log.error(f"Error running command '{' '.join(command)}': {e}")
        return 1


def run_command_capture_output(command: list[str] | str) -> tuple[int, str]:
    """
    Runs a command as a subprocess and captures its output.

    :param command: The command to run as a list of strings.
    :return: A tuple containing the exit code, stdout, and stderr.
    """
    import subprocess

    if isinstance(command, str):
        command = command.split()
    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )

        stdout, stderr = process.communicate()
        return process.returncode, stdout
    except FileNotFoundError:
        log.error(
            f"Command not found: {command[0]}. Make sure it's installed and in PATH."
        )
        return 1, ""
    except Exception as e:
        log.error(f"Error running command '{' '.join(command)}': {e}")
        return 1, ""
