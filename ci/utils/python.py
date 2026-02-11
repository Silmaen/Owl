"""
Utility functions for Python environment management in CI workflows.
"""
from ci import log, root


def is_poetry_managed() -> bool:
    """
    Check if the current Python environment is managed by Poetry.

    Returns:
    bool: True if Poetry is managing the environment, False otherwise.
    """
    if (root / "pyproject.toml").exists():
        log.info("Poetry environment detected.")
        return True
    log.info("Poetry environment not detected.")
    return False


def get_venv_activate_cmd() -> str:
    """
    Get the command to activate the virtual environment based on the operating system.

    Returns:
    str: The command to activate the virtual environment.
    """
    from ci.utils.run import run_command_capture_output

    if not is_poetry_managed():
        log.info("No poetry, no venv management.")
        return ""

    code, command = run_command_capture_output("poetry env activate")
    if code != 0:
        log.error("Failed to get poetry virtual environment activation command.")
        return ""

    log.info(f"Virtual environment activation command: {command}")
    return command


def install_python_requirements() -> int:
    """
    Get the list of Python dependencies from the Poetry-managed environment.

    Returns:
    list[str]: A list of Python dependencies.
    """
    from ci.utils.run import run_command

    if not is_poetry_managed():
        log.info("No poetry, no dependencies to list.")
        return 0

    code = run_command("poetry sync --no-root")
    if code != 0:
        log.error("Failed to sync poetry dependencies.")
        return code

    return 0
