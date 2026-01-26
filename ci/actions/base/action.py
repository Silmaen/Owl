from abc import ABC, abstractmethod
from typing import Optional

from ci.utils.preset import PresetConfig


class BaseAction(ABC):
    """Abstract base class for all CI actions."""

    @abstractmethod
    def run(self, preset: PresetConfig, extra_args: Optional[list[str]] = None) -> int:
        """
        Execute the action.
        :param preset: The preset to use for the action.
        :param extra_args: Optional list of extra arguments passed after '--'.
        :return: Exit code indicating success or failure.
        """
        pass

    def __call__(self, preset: PresetConfig, extra_args: Optional[list[str]] = None) -> int:
        """Allow the action to be called directly."""
        return self.run(preset, extra_args)

    @staticmethod
    def parse_extra_args(extra_args: Optional[list[str]]) -> dict[str, str]:
        """
        Parse extra arguments into a dictionary.
        Supports formats: --key=value and --flag (stored as key: "true").
        :param extra_args: List of extra arguments.
        :return: Dictionary of parsed arguments.
        """
        result = {}
        if not extra_args:
            return result
        for arg in extra_args:
            if arg.startswith("--"):
                arg = arg[2:]
                if "=" in arg:
                    key, value = arg.split("=", 1)
                    result[key] = value
                else:
                    result[arg] = "true"
        return result
