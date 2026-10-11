/**
 * @file Theme.h
 * @author Silmaen
 * @date 10/08/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "math/vectors.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::gui {
/**
 * @brief
 *  Available built-in theme presets.
 */
enum struct ThemePreset : uint8_t {
	OwlDark,///< Default dark theme, Owl amber accent.
	OwlLight,///< Light theme, darker amber accent.
};

/**
 * @brief
 *  GUI theme as a small set of semantic tokens; `UiLayer::setTheme` derives every ImGui colour and state from them.
 *
 * Surfaces go from the application ground (`surface0`) to the pressed state (`surface4`): every hover and press is
 * one step up. A single accent marks selection, focus and the main action; `play` is kept for the play mode so it
 * never reads as a selection. The defaults are Owl Dark.
 */
struct OWL_API Theme {
	/// Application ground: gutters between docked panels, inactive tabs, title bars.
	math::vec4 surface0{0.067f, 0.071f, 0.082f, 1.f};
	/// Panel body.
	math::vec4 surface1{0.098f, 0.106f, 0.122f, 1.f};
	/// Headers, toolbars, ribbon and popups.
	math::vec4 surface2{0.125f, 0.137f, 0.157f, 1.f};
	/// Hovered controls.
	math::vec4 surface3{0.161f, 0.176f, 0.2f, 1.f};
	/// Pressed controls.
	math::vec4 surface4{0.2f, 0.22f, 0.251f, 1.f};
	/// Background of input fields.
	math::vec4 well{0.051f, 0.055f, 0.067f, 1.f};
	/// Borders.
	math::vec4 line{0.165f, 0.18f, 0.208f, 1.f};
	/// Separators inside a panel.
	math::vec4 lineSoft{0.122f, 0.133f, 0.153f, 1.f};
	/// Main text.
	math::vec4 text{0.855f, 0.867f, 0.886f, 1.f};
	/// Secondary text: labels, captions.
	math::vec4 textMuted{0.604f, 0.631f, 0.671f, 1.f};
	/// Disabled text and placeholders.
	math::vec4 textDisabled{0.373f, 0.4f, 0.439f, 1.f};
	/// Selection, focus and main action.
	math::vec4 accent{0.918f, 0.647f, 0.235f, 1.f};
	/// Text drawn on the accent.
	math::vec4 onAccent{0.102f, 0.071f, 0.02f, 1.f};
	/// Play mode.
	math::vec4 play{0.239f, 0.545f, 1.f, 1.f};
	/// Success.
	math::vec4 success{0.298f, 0.718f, 0.51f, 1.f};
	/// Warning.
	math::vec4 warning{0.937f, 0.541f, 0.235f, 1.f};
	/// Error.
	math::vec4 error{0.898f, 0.325f, 0.294f, 1.f};
	/// Information and links.
	math::vec4 info{0.353f, 0.635f, 0.941f, 1.f};
	/// X axis.
	math::vec4 axisX{0.898f, 0.325f, 0.294f, 1.f};
	/// Y axis.
	math::vec4 axisY{0.365f, 0.733f, 0.388f, 1.f};
	/// Z axis.
	math::vec4 axisZ{0.298f, 0.553f, 0.941f, 1.f};

	/// Rounding of windows, popups and child regions, in pixels.
	float windowRounding{5.f};
	/// Rounding of frames (fields, buttons), in pixels.
	float frameRounding{3.f};
	/// Rounding of tabs, in pixels.
	float tabRounding{4.f};
	/// Indentation of tree levels, in pixels.
	float indentSpacing{14.f};

	/**
	 * @brief
	 *  A colour with another opacity.
	 * @param[in] iColor The colour.
	 * @param[in] iAlpha The opacity.
	 * @return The colour with `iAlpha` as alpha.
	 */
	[[nodiscard]] static constexpr auto withAlpha(const math::vec4& iColor, const float iAlpha) -> math::vec4 {
		return {iColor.x(), iColor.y(), iColor.z(), iAlpha};
	}

	/**
	 * @brief
	 *  Linear mix of two colours.
	 * @param[in] iFrom The colour at 0.
	 * @param[in] iTo The colour at 1.
	 * @param[in] iRatio The share of `iTo`.
	 * @return The mixed colour.
	 */
	[[nodiscard]] static constexpr auto mix(const math::vec4& iFrom, const math::vec4& iTo, const float iRatio)
			-> math::vec4 {
		return iFrom + (iTo - iFrom) * iRatio;
	}

	/**
	 * @brief
	 *  Background of a selected row or item: the accent, faint.
	 * @return The selection colour.
	 */
	[[nodiscard]] constexpr auto getSelection() const -> math::vec4 { return withAlpha(accent, 0.18f); }

	/**
	 * @brief
	 *  Load from a yaml config file; missing keys keep their current value.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief
	 *  Save To a yaml file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;

	/**
	 * @brief
	 *  Create a theme from a built-in preset.
	 * @param[in] iPreset The preset to use.
	 * @return The generated theme.
	 */
	static auto fromPreset(ThemePreset iPreset) -> Theme;

	/**
	 * @brief
	 *  Get the list of available presets and their names.
	 * @return The presets, Owl Dark first.
	 */
	static auto getPresetNames() -> std::vector<std::pair<ThemePreset, std::string>>;

	/**
	 * @brief
	 *  Preset from its name, as stored in the editor settings.
	 * @param[in] iName The name; names of retired presets ("Dark", "Nord"...) fall back to their closest preset.
	 * @return The preset, Owl Dark for an unknown name.
	 */
	static auto presetFromName(std::string_view iName) -> ThemePreset;
};

}// namespace owl::gui
