"""The `vendor.silmaen.run_tests` flag of a preset wins over its `OWL_TESTING` cache variable."""

from ci.utils.preset import PresetConfig


def test_tidy_preset_builds_tests_without_running_them():
    preset = PresetConfig("linux-clang-tidy")
    assert preset.is_valid
    assert preset.raw_config is not None
    assert preset.raw_config["cacheVariables"]["OWL_TESTING"] == "ON"
    assert preset.run_tests is False
    assert preset.run_coverage is False


def test_preset_without_flag_follows_owl_testing():
    preset = PresetConfig("linux-clang-release")
    assert preset.run_tests is True
