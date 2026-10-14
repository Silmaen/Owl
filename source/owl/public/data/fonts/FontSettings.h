/**
 * @file FontSettings.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/FormatVersion.h"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::data::fonts {

/**
 * @brief
 *  Inclusive range of Unicode code points.
 */
struct OWL_API CodePointRange {
	/// First code point of the range.
	char32_t first = 0;
	/// Last code point of the range, included.
	char32_t last = 0;

	/**
	 * @brief
	 *  Check whether a code point lies in the range.
	 * @param[in] iCodePoint The code point.
	 * @return True when `first <= iCodePoint <= last`.
	 */
	[[nodiscard]] constexpr auto contains(const char32_t iCodePoint) const noexcept -> bool {
		return iCodePoint >= first && iCodePoint <= last;
	}

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other range.
	 * @return True when both bounds are equal.
	 */
	constexpr auto operator==(const CodePointRange& iOther) const noexcept -> bool = default;
};

/**
 * @brief
 *  Import settings of a font asset, stored next to the font file in a `.owlfont` sidecar.
 *
 * Every font loads the default Latin charset (`defaultRanges()`); the settings add code-point ranges on top
 * (Cyrillic, Greek, arrows...). A change of the ranges regenerates the glyph atlas.
 */
struct OWL_API FontSettings {
	/// Code-point ranges loaded on top of the default charset.
	std::vector<CodePointRange> extraRanges;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other settings.
	 * @return True when the settings are equal.
	 */
	auto operator==(const FontSettings& iOther) const -> bool = default;

	/**
	 * @brief
	 *  The charset every font loads: Basic Latin, Latin-1 Supplement, Latin Extended-A, General Punctuation and €.
	 * @return The default code-point ranges.
	 */
	[[nodiscard]] static auto defaultRanges() noexcept -> std::span<const CodePointRange>;

	/**
	 * @brief
	 *  Default ranges followed by the extra ones, each extra range clamped to the valid Unicode span.
	 * @return The ranges to load in the atlas.
	 */
	[[nodiscard]] auto getCharsetRanges() const -> std::vector<CodePointRange>;

	/**
	 * @brief
	 *  File extension of the sidecar.
	 * @return `.owlfont`.
	 */
	[[nodiscard]] static auto fileExtension() noexcept -> const char* { return ".owlfont"; }

	/**
	 * @brief
	 *  Path of the sidecar of a font file (same folder and stem, `.owlfont` extension).
	 * @param[in] iFontPath Path of the `.ttf` / `.otf` file.
	 * @return The sidecar path.
	 */
	[[nodiscard]] static auto sidecarPath(const std::filesystem::path& iFontPath) -> std::filesystem::path;

	/**
	 * @brief
	 *  Format descriptor of the sidecar document.
	 * @return The format.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Serialize the settings to YAML.
	 * @return The YAML document.
	 */
	[[nodiscard]] auto serializeToString() const -> std::string;

	/**
	 * @brief
	 *  Read settings from YAML.
	 * @param[in] iYaml The YAML document.
	 * @return True on success; the settings are left untouched on failure.
	 */
	[[nodiscard]] auto deserializeFromString(std::string_view iYaml) -> bool;

	/**
	 * @brief
	 *  Write the settings to a sidecar file.
	 * @param[in] iPath Path of the `.owlfont` file.
	 * @return True on success.
	 */
	[[nodiscard]] auto saveToFile(const std::filesystem::path& iPath) const -> bool;

	/**
	 * @brief
	 *  Read the settings from a sidecar file.
	 * @param[in] iPath Path of the `.owlfont` file.
	 * @return True on success.
	 */
	[[nodiscard]] auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Settings of a font file: its sidecar when there is one, the defaults otherwise.
	 * @param[in] iFontPath Path of the `.ttf` / `.otf` file.
	 * @return The settings.
	 */
	[[nodiscard]] static auto loadForFont(const std::filesystem::path& iFontPath) -> FontSettings;
};

}// namespace owl::data::fonts
