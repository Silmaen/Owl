/**
 * @file TextLayout_test.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/fonts/Font.h>
#include <data/fonts/FontLibrary.h>
#include <data/fonts/TextLayout.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace owl;
using namespace owl::data::fonts;

namespace {

class TextLayoutTest : public testing::Test {
protected:
	static void SetUpTestSuite() {
		core::Log::init(core::Log::Level::Off);
		s_app = mkShared<app::Application>(app::AppParams{.renderer = renderer::gpu::RenderAPI::Type::Null,
														  .hasGui = false,
														  .useDebugging = false,
														  .isDummy = true});
		s_font = s_app->getFontLibrary().getDefaultFont();
	}

	static void TearDownTestSuite() {
		s_font.reset();
		app::Application::invalidate();
		s_app.reset();
		core::Log::invalidate();
	}

	void SetUp() override { ASSERT_NE(s_font, nullptr); }

	[[nodiscard]] static auto font() -> const Font& { return *s_font; }

	[[nodiscard]] static auto glyphs(const std::string_view iText, const TextLayout& iLayout)
			-> std::vector<Font::GlyphMetrics> {
		std::vector<Font::GlyphMetrics> result;
		forEachLaidOutGlyph(font(), iText, iLayout,
							[&result](const Font::GlyphMetrics& iGlyph) -> void { result.push_back(iGlyph); });
		return result;
	}

	[[nodiscard]] static auto lines(const std::string_view iText, const float iWrapWidth) -> std::vector<std::string> {
		std::vector<std::string> result;
		LineBreaker breaker(font(), iText, 0.f, iWrapWidth);
		while (const auto line = breaker.next())
			result.emplace_back(iText.substr(line->begin, line->end - line->begin));
		return result;
	}

	[[nodiscard]] static auto width(const std::string_view iText) -> float {
		return measureTextLayout(font(), iText, {}).diagonal().x();
	}

	static shared<app::Application> s_app;
	static shared<Font> s_font;
};

shared<app::Application> TextLayoutTest::s_app;
shared<Font> TextLayoutTest::s_font;

constexpr float g_eps = 1e-4f;

}// namespace

TEST_F(TextLayoutTest, glyphsKeepTheFontAspect) {
	const auto reference = font().getGlyphBox(U'H').quad.diagonal();
	for (const TextLayout& layout: {TextLayout{.fontSize = 1.f}, TextLayout{.fontSize = 3.f, .boxSize = {300.f, 10.f}},
									TextLayout{.fontSize = 2.f, .boxSize = {5.f, 80.f}, .wrap = true}}) {
		const auto laid = glyphs("H", layout);
		ASSERT_EQ(laid.size(), 1u);
		const auto size = laid.front().quad.diagonal();
		EXPECT_NEAR(size.x(), reference.x() * layout.fontSize, g_eps);
		EXPECT_NEAR(size.y(), reference.y() * layout.fontSize, g_eps);
	}
}

TEST_F(TextLayoutTest, glyphSizeDoesNotDependOnTheOtherGlyphs) {
	const auto alone = glyphs("a", {.fontSize = 2.f});
	const auto mixed = glyphs("aAgy", {.fontSize = 2.f});
	ASSERT_EQ(alone.size(), 1u);
	ASSERT_EQ(mixed.size(), 4u);
	EXPECT_NEAR(alone.front().quad.diagonal().y(), mixed.front().quad.diagonal().y(), g_eps);
}

TEST_F(TextLayoutTest, spacesDrawNothing) { EXPECT_EQ(glyphs("a b  c", {}).size(), 3u); }

TEST_F(TextLayoutTest, horizontalAlignmentInABox) {
	const TextLayout base{.boxSize = {20.f, 0.f}};
	auto layout = base;
	layout.horizontal = HorizontalAlignment::Left;
	EXPECT_NEAR(measureTextLayout(font(), "ab", layout).min().x(), -10.f, g_eps);
	layout.horizontal = HorizontalAlignment::Right;
	EXPECT_NEAR(measureTextLayout(font(), "ab", layout).max().x(), 10.f, g_eps);
	layout.horizontal = HorizontalAlignment::Center;
	const auto centred = measureTextLayout(font(), "ab", layout);
	EXPECT_NEAR(centred.min().x() + centred.max().x(), 0.f, g_eps);
}

TEST_F(TextLayoutTest, horizontalAlignmentWithoutABox) {
	EXPECT_NEAR(measureTextLayout(font(), "ab", {.horizontal = HorizontalAlignment::Left}).min().x(), 0.f, g_eps);
	EXPECT_NEAR(measureTextLayout(font(), "ab", {.horizontal = HorizontalAlignment::Right}).max().x(), 0.f, g_eps);
}

TEST_F(TextLayoutTest, linesAreAlignedOneByOne) {
	const auto laid = glyphs("i\nmmmm", {.boxSize = {40.f, 0.f}, .horizontal = HorizontalAlignment::Right});
	ASSERT_EQ(laid.size(), 5u);
	const float iRight = laid.front().quad.max().x();
	const float mRight = laid.back().quad.max().x();
	EXPECT_LT(iRight, 20.f + g_eps);
	EXPECT_LT(mRight, 20.f + g_eps);
	EXPECT_GT(iRight, 19.f);
	EXPECT_GT(mRight, 19.f);
}

TEST_F(TextLayoutTest, verticalAlignmentInABox) {
	TextLayout layout{.fontSize = 2.f, .boxSize = {0.f, 10.f}};
	layout.vertical = VerticalAlignment::Top;
	EXPECT_NEAR(measureTextLayout(font(), "Ag\nAg", layout).max().y(), 5.f, g_eps);
	layout.vertical = VerticalAlignment::Bottom;
	EXPECT_NEAR(measureTextLayout(font(), "Ag\nAg", layout).min().y(), -5.f, g_eps);
	layout.vertical = VerticalAlignment::Middle;
	const auto middle = measureTextLayout(font(), "Ag\nAg", layout);
	EXPECT_NEAR(middle.min().y() + middle.max().y(), 0.f, g_eps);
	layout.vertical = VerticalAlignment::Baseline;
	EXPECT_NEAR(computeTextLayout(font(), "Ag\nAg", layout).firstBaseline, 0.f, g_eps);
}

TEST_F(TextLayoutTest, verticalAlignmentWithoutABox) {
	EXPECT_NEAR(measureTextLayout(font(), "Ag", {.vertical = VerticalAlignment::Top}).max().y(), 0.f, g_eps);
	EXPECT_NEAR(measureTextLayout(font(), "Ag", {.vertical = VerticalAlignment::Bottom}).min().y(), 0.f, g_eps);
}

TEST_F(TextLayoutTest, multiLineMeasure) {
	const TextLayout layout{.fontSize = 2.f, .lineSpacing = 0.5f};
	const auto metrics = computeTextLayout(font(), "one\ntwo\nthree", layout);
	EXPECT_EQ(metrics.lineCount, 3u);
	const float expected = 2.f * (font().getAscender() - font().getDescender() +
								  2.f * (font().getScaledLineHeight() + layout.lineSpacing));
	EXPECT_NEAR(metrics.height, expected, g_eps);
	EXPECT_NEAR(metrics.width, 2.f * width("three"), g_eps);
	EXPECT_NEAR(measureTextLayout(font(), "one\ntwo\nthree", layout).diagonal().y(), expected, g_eps);
}

TEST_F(TextLayoutTest, lineFeedsAndEmptyText) {
	EXPECT_EQ(computeTextLayout(font(), "", {}).lineCount, 0u);
	EXPECT_TRUE(glyphs("", {}).empty());
	EXPECT_EQ(lines("a\nb\n", 0.f), (std::vector<std::string>{"a", "b", ""}));
	EXPECT_EQ(lines("a\r\nb", 0.f), (std::vector<std::string>{"a\r", "b"}));
}

TEST_F(TextLayoutTest, wrapBreaksOnSpaces) {
	const float wrapWidth = width("aaa bbb") * 1.01f;
	EXPECT_EQ(lines("aaa bbb ccc", wrapWidth), (std::vector<std::string>{"aaa bbb", "ccc"}));
	EXPECT_EQ(lines("aaa bbb   aaa bbb", wrapWidth), (std::vector<std::string>{"aaa bbb", "aaa bbb"}));
	EXPECT_EQ(lines("aaa bbb ccc", 0.f), (std::vector<std::string>{"aaa bbb ccc"}));
}

TEST_F(TextLayoutTest, wrapHardBreaksOverlongWords) {
	const float wrapWidth = width("abcde") * 1.01f;
	const auto broken = lines("abcdefghij", wrapWidth);
	ASSERT_GE(broken.size(), 2u);
	std::string joined;
	for (const auto& line: broken) {
		EXPECT_LE(width(line), wrapWidth + g_eps) << line;
		joined += line;
	}
	EXPECT_EQ(joined, "abcdefghij");
	EXPECT_EQ(lines("W", 0.01f), (std::vector<std::string>{"W"}));
}

TEST_F(TextLayoutTest, wrapIsUtf8Aware) {
	const std::string text = "\xc3\xa9t\xc3\xa9 \xc3\xa9t\xc3\xa9";
	EXPECT_EQ(lines(text, width("\xc3\xa9t\xc3\xa9") * 1.05f),
			  (std::vector<std::string>{"\xc3\xa9t\xc3\xa9", "\xc3\xa9t\xc3\xa9"}));
	const auto broken = lines("\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", width("\xc3\xa9\xc3\xa9") * 1.05f);
	ASSERT_EQ(broken.size(), 2u);
	EXPECT_EQ(broken.front(), "\xc3\xa9\xc3\xa9");
}

TEST_F(TextLayoutTest, wrapUsesTheBoxWidth) {
	const TextLayout layout{.fontSize = 2.f, .boxSize = {2.f * width("aaa bbb") * 1.01f, 0.f}, .wrap = true};
	const auto metrics = computeTextLayout(font(), "aaa bbb ccc", layout);
	EXPECT_EQ(metrics.lineCount, 2u);
	EXPECT_LE(metrics.width, layout.boxSize.x());
}

TEST_F(TextLayoutTest, shrinkFitsTheBox) {
	const TextLayout layout{.fontSize = 1.f, .boxSize = {2.f, 1.f}, .fit = TextFit::Shrink};
	const auto metrics = computeTextLayout(font(), "Hello world", layout);
	EXPECT_LT(metrics.fontSize, 1.f);
	EXPECT_NEAR(metrics.width, 2.f, 1e-3f);
	EXPECT_EQ(computeTextLayout(font(), "Hi", {.fontSize = 0.5f, .boxSize = {20.f, 20.f}, .fit = TextFit::Shrink})
					  .fontSize,
			  0.5f);
}

TEST_F(TextLayoutTest, shrinkStopsAtTheMinimum) {
	const TextLayout layout{.fontSize = 1.f, .boxSize = {0.1f, 0.1f}, .fit = TextFit::Shrink, .minFontSize = 0.5f};
	EXPECT_NEAR(computeTextLayout(font(), "Hello world", layout).fontSize, 0.5f, g_eps);
}

TEST_F(TextLayoutTest, shrinkWithWrapFitsBothAxes) {
	const TextLayout layout{.fontSize = 2.f, .boxSize = {10.f, 3.f}, .wrap = true, .fit = TextFit::Shrink};
	const auto metrics = computeTextLayout(font(), "The quick brown fox jumps over the lazy dog", layout);
	EXPECT_LT(metrics.fontSize, 2.f);
	EXPECT_GT(metrics.lineCount, 1u);
	EXPECT_LE(metrics.width, 10.f * 1.001f);
	EXPECT_LE(metrics.height, 3.f * 1.001f);
}

TEST_F(TextLayoutTest, overflowDrawsEverything) {
	const auto unbounded = glyphs("Hello world", {});
	const auto overflow = glyphs("Hello world", {.boxSize = {1.f, 0.5f}});
	EXPECT_EQ(overflow.size(), unbounded.size());
}

TEST_F(TextLayoutTest, clipCutsGlyphsAtTheBox) {
	const TextLayout layout{.boxSize = {2.f, 0.5f}, .fit = TextFit::Clip};
	const auto clipped = glyphs("Hello world", layout);
	EXPECT_LT(clipped.size(), glyphs("Hello world", {}).size());
	ASSERT_FALSE(clipped.empty());
	for (const auto& [quad, uv]: clipped) {
		EXPECT_GE(quad.min().x(), -1.f - g_eps);
		EXPECT_LE(quad.max().x(), 1.f + g_eps);
		EXPECT_GE(quad.min().y(), -0.25f - g_eps);
		EXPECT_LE(quad.max().y(), 0.25f + g_eps);
		EXPECT_LE(uv.min().x(), uv.max().x());
	}
}

TEST_F(TextLayoutTest, clipGlyphScalesTheTextureCoordinates) {
	Font::GlyphMetrics glyph{.quad = {{0.f, 0.f}, {2.f, 2.f}}, .uv = {{0.f, 0.f}, {0.2f, 0.4f}}};
	ASSERT_TRUE(clipGlyph(glyph, {{1.f, -5.f}, {5.f, 1.f}}));
	EXPECT_NEAR(glyph.quad.min().x(), 1.f, g_eps);
	EXPECT_NEAR(glyph.quad.max().y(), 1.f, g_eps);
	EXPECT_NEAR(glyph.uv.min().x(), 0.1f, g_eps);
	EXPECT_NEAR(glyph.uv.max().x(), 0.2f, g_eps);
	EXPECT_NEAR(glyph.uv.max().y(), 0.2f, g_eps);
	EXPECT_FALSE(clipGlyph(glyph, {{3.f, 3.f}, {4.f, 4.f}}));
}
