"""
Module providing utility functions for interacting with TeamCity CI/CD system.
"""

from ci import log


def set_teamcity_parameter(name: str, value: str) -> None:
    """
    Sets a TeamCity build parameter by printing the appropriate service message.

    :param name: The name of the parameter to set.
    :param value: The value to assign to the parameter.
    """
    import os

    if "TEAMCITY_VERSION" in os.environ:
        log.debug(f"Setting TeamCity parameter: {name}={value}")
        # escape special characters according to TeamCity documentation
        value = (value.replace("|", "||")
                 .replace("'", "|'")
                 .replace("\n", "|n")
                 .replace("\r", "|r")
                 .replace("[", "|[")
                 .replace("]", "|]"))
        print(f"##teamcity[setParameter name='{name}' value='{value}']")
    else:
        log.warning(f"Not in TeamCity, setting environment variable: {name}={value}")
        os.environ[name] = value
