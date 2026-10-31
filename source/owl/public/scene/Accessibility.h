/**
 * @file Accessibility.h
 * @author Silmaen
 * @date 31/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "renderer/ColorVision.h"

namespace owl::scene {

class SettingsManager;

/**
 * @brief
 *  The accessibility settings of the player, read from the `SettingsManager` of the game.
 */
struct OWL_API Accessibility {
	/// Colour vision deficiency the final frame corrects (`color_blind_mode`, `toName` of the deficiency).
	renderer::ColorDeficiency colorBlindMode = renderer::ColorDeficiency::None;
	/// Strength of the correction, 0 to 1 (`color_blind_strength`).
	float colorBlindStrength = 1.f;
	/// No camera motion blur, a still film grain, wipes as fades (`reduced_motion`, also read by scripts).
	bool reducedMotion = false;
	/// Subtitles of the sounds and of `ui.show_subtitle` drawn (`subtitles`).
	bool subtitles = true;
	/// Size of the subtitles, 0.5 to 3, on top of the HUD scale (`subtitle_scale`).
	float subtitleScale = 1.f;
	/// Opacity of the band behind the subtitles, 0 to 1 (`subtitle_background`).
	float subtitleBackground = 0.6f;

	/// Settings key of the colour vision deficiency.
	static constexpr auto KeyColorBlindMode = "color_blind_mode";
	/// Settings key of the strength of the correction.
	static constexpr auto KeyColorBlindStrength = "color_blind_strength";
	/// Settings key of the reduced motion.
	static constexpr auto KeyReducedMotion = "reduced_motion";
	/// Settings key of the subtitles.
	static constexpr auto KeySubtitles = "subtitles";
	/// Settings key of the size of the subtitles.
	static constexpr auto KeySubtitleScale = "subtitle_scale";
	/// Settings key of the opacity of the band behind the subtitles.
	static constexpr auto KeySubtitleBackground = "subtitle_background";

	/**
	 * @brief
	 *  Read the settings, clamped; a missing or unreadable key keeps its default.
	 * @param[in] iSettings The settings.
	 * @return The accessibility settings.
	 */
	[[nodiscard]] static auto read(const SettingsManager& iSettings) -> Accessibility;

	/**
	 * @brief
	 *  Register the default of every accessibility key, so `settings.get` and `settings.reset` know them.
	 * @param[in,out] ioSettings The settings.
	 */
	static void setDefaults(SettingsManager& ioSettings);

	/**
	 * @brief
	 *  The colour vision filter the final frame of a game applies.
	 * @return The correction filter of the setting.
	 */
	[[nodiscard]] auto getColorVisionFilter() const -> renderer::ColorVisionFilter {
		return {.deficiency = colorBlindMode,
				.mode = renderer::ColorVisionMode::Correct,
				.strength = colorBlindStrength};
	}
};

}// namespace owl::scene
