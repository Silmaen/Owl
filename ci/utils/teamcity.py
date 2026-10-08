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
        value = (
            value.replace("|", "||")
            .replace("'", "|'")
            .replace("\n", "|n")
            .replace("\r", "|r")
            .replace("[", "|[")
            .replace("]", "|]")
        )
        print(f"##teamcity[setParameter name='{name}' value='{value}']")
    else:
        log.warning(f"Not in TeamCity, setting environment variable: {name}={value}")
        os.environ[name] = value


def report_statistic(key: str, value: float) -> None:
    """
    Publish a build statistic (a metric TeamCity charts and failure conditions can compare between builds).

    :param key: The statistic key; the `CodeCoverage*` keys feed TeamCity's own coverage metrics.
    :param value: The value.
    """
    import os

    if "TEAMCITY_VERSION" in os.environ:
        print(f"##teamcity[buildStatisticValue key='{key}' value='{value:.4f}']")
    else:
        log.info(f"Statistic {key} = {value:.4f}")
