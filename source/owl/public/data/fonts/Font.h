/**
 * @file Font.h
 * @author Silmaen
 * @date 10/17/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/utils/Utf8.h"
#include "data/fonts/FontSettings.h"

#include "math/box.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace owl::renderer::gpu {
class Texture2D;
}// namespace owl::renderer::gpu

namespace owl::data::fonts {

struct MsdfData;

/**
 * @brief
 *  Class describing a font: an MSDF glyph atlas over a charset of Unicode code points.
 *
 * The charset is `FontSettings::defaultRanges()` plus the extra ranges of the settings. The atlas is cached on disk
 * (`getAtlasCacheDirectory()`), keyed by the font file content, the charset and the atlas parameters: a change of any
 * of them regenerates it.
 */
class OWL_API Font final {
public:
	/**
	 * @brief
	 *  Load a font from a file.
	 * @param[in] iPath Path of the font file (stored by the font, hence taken by value).
	 * @param[in] iIsDefault True for the default font.
	 * @param[in] iSettings Import settings (extra code-point ranges), stored too.
	 */
	explicit Font(std::filesystem::path iPath, bool iIsDefault = false, FontSettings iSettings = {});

	/**
	 * @brief
	 *  Default destructor.
	 */
	~Font();

	/**
	 * @brief
	 *  Default copy constructor.
	 */
	Font(const Font&) = default;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	Font(Font&&) = default;

	/**
	 * @brief
	 *  Default copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const Font&) -> Font& = default;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(Font&&) -> Font& = default;

	/**
	 * @brief
	 *  Get the atlas texture corresponding to the Font.
	 * @return The atlas texture.
	 */
	[[nodiscard]] auto getAtlasTexture() const -> shared<renderer::gpu::Texture2D> { return m_atlasTexture; }

	/**
	 * @brief
	 *  Metrics of the glyphs.
	 */
	struct GlyphMetrics {
		/// The quad for 3D space.
		math::box2f quad;
		/// The quad for texture space.
		math::box2f uv;
	};

	/**
	 * @brief
	 *  Get the glyph metrics for the given code point.
	 *
	 * A code point missing from the atlas falls back to `?`, a tab to a space, a line feed gives an empty box.
	 * @param[in] iCodePoint The code point to look up.
	 * @return Quad and UV boxes for the glyph in the font atlas.
	 */
	[[nodiscard]] auto getGlyphBox(char32_t iCodePoint) const -> GlyphMetrics;

	/**
	 * @brief
	 *  Check whether the atlas holds a glyph for a code point (no fallback).
	 * @param[in] iCodePoint The code point.
	 * @return True when the glyph is in the atlas.
	 */
	[[nodiscard]] auto hasGlyph(char32_t iCodePoint) const -> bool;

	/**
	 * @brief
	 *  Number of glyphs in the atlas.
	 * @return The glyph count.
	 */
	[[nodiscard]] auto getGlyphCount() const -> size_t;

	/**
	 * @brief
	 *  Get the distance between two baselines.
	 *
	 * The font unit is the ascender-to-descender height: `getAscender() - getDescender()` is 1.
	 * @return The line height, in font units.
	 */
	[[nodiscard]] auto getScaledLineHeight() const -> float;

	/**
	 * @brief
	 *  Height of the ascender above the baseline.
	 * @return The ascender, in font units (positive).
	 */
	[[nodiscard]] auto getAscender() const -> float;

	/**
	 * @brief
	 *  Height of the descender relative to the baseline.
	 * @return The descender, in font units (negative or zero).
	 */
	[[nodiscard]] auto getDescender() const -> float;

	/**
	 * @brief
	 *  Horizontal advance from a code point to the next one, kerning included.
	 * @param[in] iCodePoint The current code point.
	 * @param[in] iNextCodePoint The next code point.
	 * @return The advance, in the same unit as the glyph quads.
	 */
	[[nodiscard]] auto getAdvance(char32_t iCodePoint, char32_t iNextCodePoint) const -> float;

	/**
	 * @brief
	 *  Lay out a UTF-8 string on one baseline per line and call a function for each visible glyph.
	 *
	 * Control characters other than line feed and carriage return draw as `?`; a carriage return is skipped.
	 * @tparam Fn Callable taking `const GlyphMetrics&` (quad translated to its pen position).
	 * @param[in] iText The UTF-8 text, invalid sequences drawn as U+FFFD (or its fallback).
	 * @param[in] iKerning Extra advance added between two glyphs.
	 * @param[in] iLineSpacing Extra space added between two lines.
	 * @param[in] iOnGlyph The function called per glyph.
	 */
	template<typename Fn>
	void forEachGlyph(const std::string_view iText, const float iKerning, const float iLineSpacing,
					  Fn&& iOnGlyph) const {
		math::vec2 cursor{0.f, 0.f};
		size_t offset = 0;
		char32_t next = 0;
		bool hasNext = !iText.empty();
		if (hasNext)
			next = core::utils::decodeNextCodePoint(iText, offset);
		while (hasNext) {
			const char32_t current = next < 0x20 && next != U'\r' && next != U'\n' ? U'?' : next;
			hasNext = offset < iText.size();
			if (hasNext)
				next = core::utils::decodeNextCodePoint(iText, offset);
			if (current == U'\r')
				continue;
			if (current == U'\n') {
				cursor.x() = 0.f;
				cursor.y() -= getScaledLineHeight() + iLineSpacing;
				continue;
			}
			auto metrics = getGlyphBox(current);
			metrics.quad.translate(cursor);
			iOnGlyph(std::as_const(metrics));
			if (hasNext)
				cursor.x() += getAdvance(current, next < 0x20 ? U'?' : next) + iKerning;
		}
	}

	/**
	 * @brief
	 *  Bounding box of a laid out UTF-8 string (see `forEachGlyph`).
	 * @param[in] iText The UTF-8 text.
	 * @param[in] iKerning Extra advance added between two glyphs.
	 * @param[in] iLineSpacing Extra space added between two lines.
	 * @return The union of the glyph quads.
	 */
	[[nodiscard]] auto measureText(std::string_view iText, float iKerning, float iLineSpacing) const -> math::box2f;

	/**
	 * @brief
	 *  Rebuild the font from its file with new settings, in place: every holder of the font sees the new atlas.
	 * @param[in] iSettings The new settings.
	 */
	void reload(const FontSettings& iSettings);

	/**
	 * @brief
	 *  Get the import settings the font was built with.
	 * @return The settings.
	 */
	[[nodiscard]] auto getSettings() const -> const FontSettings& { return m_settings; }

	/**
	 * @brief
	 *  Get the path of the font file.
	 * @return The path.
	 */
	[[nodiscard]] auto getPath() const -> const std::filesystem::path& { return m_path; }

	/**
	 * @brief
	 *  Check whether the atlas was read from the disk cache or the pack instead of being generated.
	 * @return True on a cache hit.
	 */
	[[nodiscard]] auto isAtlasFromCache() const -> bool { return m_atlasFromCache; }

	/**
	 * @brief
	 *  Folder of the cached atlases: `cache/font` in the working directory of the application, or in the temporary
	 *  directory without an application.
	 * @return The folder path.
	 */
	[[nodiscard]] static auto getAtlasCacheDirectory() -> std::filesystem::path;

	/**
	 * @brief
	 *  Atlas cache file of this font, written unless a pack is open; the export bakes it into the pack.
	 * @return The file path, empty when the font failed to load.
	 */
	[[nodiscard]] auto getAtlasCacheFile() const -> const std::filesystem::path& { return m_atlasCacheFile; }

	/**
	 * @brief
	 *  Pack entry of a baked atlas: `cache/font/` and the name of the cache file.
	 * @param[in] iCacheFile The atlas cache file (`getAtlasCacheFile`).
	 * @return The entry path, relative to the pack root.
	 */
	[[nodiscard]] static auto getPackedAtlasPath(const std::filesystem::path& iCacheFile) -> std::filesystem::path;

	/**
	 * @brief
	 *  get the font's name.
	 * @return The font name.
	 */
	[[nodiscard]] auto getName() const -> const std::string& { return m_name; }

	/**
	 * @brief
	 *  Look if it is the default font.
	 * @return True if this is the default font.
	 */
	[[nodiscard]] auto isDefault() const -> bool { return m_default; }

private:
	/// pointer to the texture.
	shared<renderer::gpu::Texture2D> m_atlasTexture;
	/// The internal atlas data.
	shared<MsdfData> m_data;
	/// The name of the font.
	std::string m_name;
	/// Path of the font file.
	std::filesystem::path m_path;
	/// Import settings the atlas was built with.
	FontSettings m_settings;
	/// If this font is the default one.
	bool m_default = false;
	/// True when the atlas came from the disk cache or the pack.
	bool m_atlasFromCache = false;
	/// Atlas cache file, named after the stem of the font and its atlas key.
	std::filesystem::path m_atlasCacheFile;
};

}// namespace owl::data::fonts
