"""
Cmake utility functions for CI scripts.
"""
from pathlib import Path
from typing import Dict, Any, List, Optional

from ci import root

preset_file = root / "CMakePresets.json"


def _load_preset_file(i_file_path: Path) -> dict[str, Any]:
    """Load a CMake preset JSON file."""
    import json

    with open(i_file_path, "r") as file:
        return json.load(file)


def _resolve_includes(
        i_preset_file: Path, i_data: Dict[str, Any], io_visited: set
) -> List[Dict[str, Any]]:
    """Recursively resolve included preset files."""
    all_presets = [i_data]

    if "include" in i_data:
        base_dir = i_preset_file.parent
        for includePath in i_data["include"]:
            include_file = base_dir / includePath

            # Avoid circular includes
            if include_file in io_visited:
                continue
            io_visited.add(include_file)

            if include_file.exists():
                included_data = _load_preset_file(include_file)
                all_presets.extend(
                    _resolve_includes(include_file, included_data, io_visited)
                )
    return all_presets


def _find_preset_by_name(
        i_name: str,
        i_all_presets: List[Dict[str, Any]],
        i_preset_type: str = "configurePresets",
) -> Optional[Dict[str, Any]]:
    """Find a preset configuration by name across all loaded files."""
    for presetFile in i_all_presets:
        if i_preset_type in presetFile:
            for preset in presetFile[i_preset_type]:
                if preset.get("name") == i_name:
                    return preset
    return None


def _deep_merge(i_base: Dict[str, Any], i_override: Dict[str, Any]) -> Dict[str, Any]:
    """Deep merge two dictionaries. Override values take precedence."""
    result = i_base.copy()

    for key, value in i_override.items():
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = _deep_merge(result[key], value)
        else:
            result[key] = value

    return result


def _merge_preset_with_inheritance(
        i_preset_name: str,
        i_all_presets: List[Dict[str, Any]],
        i_preset_type: str = "configurePresets",
) -> Dict[str, Any]:
    """Recursively merge a preset with all its inherited presets."""
    preset = _find_preset_by_name(i_preset_name, i_all_presets, i_preset_type)
    if not preset:
        return {}

    merged_config = {}

    # Resolve inheritance chain
    if "inherits" in preset:
        inherits = preset["inherits"]
        if isinstance(inherits, str):
            inherits = [inherits]

        for parentName in inherits:
            parent_config = _merge_preset_with_inheritance(
                parentName, i_all_presets, i_preset_type
            )
            merged_config = _deep_merge(merged_config, parent_config)

    # Merge current preset (overrides parents)
    merged_config = _deep_merge(merged_config, preset)

    # Remove 'inherits' from final output
    merged_config.pop("inherits", None)
    # Remove 'hidden' from final output
    merged_config.pop("hidden", None)

    return merged_config


def get_preset_config(i_preset_name: str) -> Dict[str, Any]:
    """Get complete configuration for a preset including all inherited values."""
    global preset_file
    data = _load_preset_file(preset_file)
    visited = {preset_file}
    all_presets = _resolve_includes(preset_file, data, visited)

    return _merge_preset_with_inheritance(i_preset_name, all_presets)


def list_cmake_presets() -> list[str]:
    """
    Get all available presets from the configuration file.

    :return: A list of all preset names.
    """
    global preset_file
    names = []
    preset_types = [
        "configurePresets",
        "buildPresets",
        "testPresets",
        "packagePresets",
        "workflowPresets",
    ]
    data = _load_preset_file(preset_file)
    visited = {preset_file}
    all_presets = _resolve_includes(preset_file, data, visited)
    for preset_name in all_presets:
        for preset_type in preset_types:
            if preset_type in preset_name:
                for preset in preset_name[preset_type]:
                    name = preset.get("name")
                    if name and not preset.get("hidden", False):
                        names.append(name)
    return ["help"] + sorted(set(names))


def cmake_preset_exists(i_preset: str) -> bool:
    """
    Check if a given CMake preset exists.
    :param i_preset: The preset to check.
    :return: True if the preset exists, False otherwise.
    """
    return i_preset in list_cmake_presets()
