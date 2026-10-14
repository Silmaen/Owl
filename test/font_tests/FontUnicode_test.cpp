/**
 * @file FontUnicode_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/fonts/Font.h>
#include <data/fonts/FontLibrary.h>
#include <data/fonts/FontSettings.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

using namespace owl;
using namespace owl::data::fonts;

namespace {

class FontUnicode : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		m_dir = std::filesystem::temp_directory_path() / "owl_font_unicode_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		m_font = m_dir / "OwlUnicodeTest.ttf";
		std::filesystem::copy_file(
				test::getRootPath() / "engine_assets" / "fonts" / "opensans" / "OpenSans-Regular.ttf", m_font);
		clearCache();
	}

	void TearDown() override {
		clearCache();
		std::filesystem::remove_all(m_dir);
		app::Application::invalidate();
		m_app.reset();
		core::Log::invalidate();
	}

	[[nodiscard]] static auto cachedAtlasCount() -> size_t {
		size_t count = 0;
		std::error_code error;
		for (const auto& entry: std::filesystem::directory_iterator(Font::getAtlasCacheDirectory(), error))
			if (entry.path().filename().string().starts_with("OwlUnicodeTest-"))
				++count;
		return count;
	}

	static void clearCache() {
		std::error_code error;
		for (const auto& entry: std::filesystem::directory_iterator(Font::getAtlasCacheDirectory(), error))
			if (entry.path().filename().string().starts_with("OwlUnicodeTest-"))
				std::filesystem::remove(entry.path(), error);
	}

	shared<app::Application> m_app;
	std::filesystem::path m_dir;
	std::filesystem::path m_font;
};

}// namespace

TEST_F(FontUnicode, defaultCharsetCoversLatinExtendedAndPunctuation) {
	const auto font = m_app->getFontLibrary().getDefaultFont();
	ASSERT_NE(font, nullptr);
	// œ Œ Š ž (Latin Extended-A), ’ “ ” … (General Punctuation), € (U+20AC).
	for (const char32_t code: {U'œ', U'Œ', U'Š', U'ž', U'’', U'“', U'”', U'…', U'€', U'é', U'A'})
		EXPECT_TRUE(font->hasGlyph(code)) << static_cast<uint32_t>(code);
	// Cyrillic is outside the default charset.
	EXPECT_FALSE(font->hasGlyph(U'Ж'));
}

TEST_F(FontUnicode, glyphBeyondLatin1HasItsOwnBox) {
	const auto font = m_app->getFontLibrary().getDefaultFont();
	ASSERT_NE(font, nullptr);
	const auto euro = font->getGlyphBox(U'€');
	const auto question = font->getGlyphBox(U'?');
	EXPECT_NE(euro.uv.min(), question.uv.min());
	EXPECT_GT(euro.quad.max().x(), euro.quad.min().x());
	EXPECT_GT(font->getAdvance(U'œ', U'u'), 0.f);
	// A missing code point falls back to '?'.
	const auto missing = font->getGlyphBox(U'Ж');
	EXPECT_EQ(missing.uv.min(), question.uv.min());
	EXPECT_GT(font->getAdvance(U'Ж', U'Ж'), 0.f);
}

TEST_F(FontUnicode, measureTextDecodesUtf8) {
	const auto font = m_app->getFontLibrary().getDefaultFont();
	ASSERT_NE(font, nullptr);
	const auto one = font->measureText("€", 0.f, 0.f);
	const auto two = font->measureText("€€", 0.f, 0.f);
	EXPECT_GT(two.diagonal().x(), one.diagonal().x() * 1.5f);
	size_t glyphs = 0;
	font->forEachGlyph("a\xC3\xA9\r\n\xFF"
					   "b",
					   0.f, 0.f, [&glyphs](const Font::GlyphMetrics&) -> void { ++glyphs; });
	EXPECT_EQ(glyphs, 4u);
}

TEST_F(FontUnicode, extraRangesAddGlyphs) {
	const Font font(m_font, false, FontSettings{.extraRanges = {{.first = 0x0400, .last = 0x04FF}}});
	EXPECT_TRUE(font.hasGlyph(U'Ж'));
	EXPECT_TRUE(font.hasGlyph(U'€'));
	const Font plain(m_font);
	EXPECT_GT(font.getGlyphCount(), plain.getGlyphCount());
}

TEST_F(FontUnicode, atlasIsCachedAndInvalidatedByRanges) {
	{
		const Font first(m_font);
		EXPECT_FALSE(first.isAtlasFromCache());
		EXPECT_EQ(cachedAtlasCount(), 1u);
	}
	{
		const Font second(m_font);
		EXPECT_TRUE(second.isAtlasFromCache());
		EXPECT_NE(second.getAtlasTexture(), nullptr);
	}
	{
		const Font greek(m_font, false, FontSettings{.extraRanges = {{.first = 0x0370, .last = 0x03FF}}});
		EXPECT_FALSE(greek.isAtlasFromCache());
		EXPECT_TRUE(greek.hasGlyph(U'Ω'));
		// The atlas of the previous charset is dropped.
		EXPECT_EQ(cachedAtlasCount(), 1u);
	}
	{
		const Font plain(m_font);
		EXPECT_FALSE(plain.isAtlasFromCache());
	}
}

TEST_F(FontUnicode, reloadRebuildsInPlace) {
	auto font = mkShared<Font>(m_font);
	const auto holder = font;
	EXPECT_FALSE(holder->hasGlyph(U'Ж'));
	font->reload(FontSettings{.extraRanges = {{.first = 0x0410, .last = 0x044F}}});
	EXPECT_TRUE(holder->hasGlyph(U'Ж'));
	EXPECT_EQ(holder->getSettings().extraRanges.size(), 1u);
	EXPECT_EQ(holder->getPath(), m_font);
}

TEST_F(FontUnicode, libraryReadsSidecarAndReloads) {
	auto& library = m_app->getFontLibrary();
	EXPECT_FALSE(library.reloadFont("NotAFont", {}));
	const auto font = library.getDefaultFont();
	ASSERT_NE(font, nullptr);
	const auto previous = font->getSettings();
	EXPECT_TRUE(library.reloadFont(font->getName(), FontSettings{.extraRanges = {{.first = 0x0416, .last = 0x0416}}}));
	EXPECT_TRUE(font->hasGlyph(U'Ж'));
	EXPECT_TRUE(library.reloadFont(font->getName(), previous));
	EXPECT_FALSE(font->hasGlyph(U'Ж'));
}

TEST(FontSettings, defaultRangesAndCharset) {
	const auto defaults = FontSettings::defaultRanges();
	ASSERT_FALSE(defaults.empty());
	EXPECT_EQ(defaults.front().first, U' ');
	const FontSettings settings{
			.extraRanges = {{.first = 0x0500, .last = 0x0400}, {.first = 0x10FFFF, .last = 0x200000}}};
	const auto ranges = settings.getCharsetRanges();
	ASSERT_EQ(ranges.size(), defaults.size() + 2);
	// Swapped bounds are ordered, values above U+10FFFF clamped.
	EXPECT_EQ(ranges[defaults.size()], (CodePointRange{.first = 0x0400, .last = 0x0500}));
	EXPECT_EQ(ranges.back(), (CodePointRange{.first = 0x10FFFF, .last = 0x10FFFF}));
	EXPECT_TRUE(ranges.back().contains(0x10FFFF));
	EXPECT_FALSE(ranges.back().contains(0x10FFFE));
}

TEST(FontSettings, yamlRoundTrip) {
	core::Log::init(core::Log::Level::Off);
	const FontSettings settings{.extraRanges = {{.first = 0x0400, .last = 0x04FF}, {.first = 0x2190, .last = 0x21FF}}};
	FontSettings parsed;
	ASSERT_TRUE(parsed.deserializeFromString(settings.serializeToString()));
	EXPECT_EQ(parsed, settings);
	EXPECT_FALSE(parsed.deserializeFromString("not: [valid"));
	EXPECT_FALSE(parsed.deserializeFromString("Other: 1"));
	EXPECT_EQ(parsed, settings);
	ASSERT_TRUE(parsed.deserializeFromString("FontSettings: {}\nextraRanges: [[1, 2], [3], [x, 4]]"));
	EXPECT_EQ(parsed.extraRanges.size(), 1u);
	core::Log::invalidate();
}

TEST(FontSettings, sidecarFile) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_font_settings_test";
	std::filesystem::remove_all(dir);
	std::filesystem::create_directories(dir);
	const auto fontPath = dir / "MyFont.ttf";
	EXPECT_EQ(FontSettings::sidecarPath(fontPath), dir / "MyFont.owlfont");
	EXPECT_TRUE(FontSettings::loadForFont(fontPath).extraRanges.empty());
	const FontSettings settings{.extraRanges = {{.first = 0x0370, .last = 0x03FF}}};
	ASSERT_TRUE(settings.saveToFile(FontSettings::sidecarPath(fontPath)));
	EXPECT_EQ(FontSettings::loadForFont(fontPath), settings);
	FontSettings missing;
	EXPECT_FALSE(missing.loadFromFile(dir / "missing.owlfont"));
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}
