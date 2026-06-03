/**
 * @file FontCoverage_test.cpp
 * @author Silmaen
 * @date 14/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/Application.h>
#include <data/fonts/Font.h>
#include <data/fonts/FontLibrary.h>

using namespace owl::data::fonts;
using namespace owl;

TEST(FontCoverage, nonExistentFontFile) {
	core::Log::init(core::Log::Level::Off);
	// Constructing a Font with a nonexistent path should not crash.
	Font font(std::filesystem::path("/nonexistent/path/to/font.ttf"));
	EXPECT_EQ(font.getAtlasTexture(), nullptr);
	EXPECT_TRUE(font.getName().empty());
	EXPECT_FALSE(font.isDefault());
	core::Log::invalidate();
}

TEST(FontCoverage, glyphBoxNewline) {
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "fontTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);

	// Newline should return zeroed GlyphMetrics.
	const auto [quad, uv] = myFont->getGlyphBox('\n');
	EXPECT_NEAR(quad.min().x(), 0.0f, 0.001f);
	EXPECT_NEAR(quad.min().y(), 0.0f, 0.001f);
	EXPECT_NEAR(quad.max().x(), 0.0f, 0.001f);
	EXPECT_NEAR(quad.max().y(), 0.0f, 0.001f);

	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(FontCoverage, glyphBoxTab) {
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "fontTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);

	// Tab should use the space glyph geometry.
	const auto tabMetrics = myFont->getGlyphBox('\t');
	const auto spaceMetrics = myFont->getGlyphBox(' ');
	// Tab and space should produce the same quad (tab redirects to space glyph).
	EXPECT_NEAR(tabMetrics.quad.min().x(), spaceMetrics.quad.min().x(), 0.001f);
	EXPECT_NEAR(tabMetrics.quad.min().y(), spaceMetrics.quad.min().y(), 0.001f);
	EXPECT_NEAR(tabMetrics.quad.max().x(), spaceMetrics.quad.max().x(), 0.001f);
	EXPECT_NEAR(tabMetrics.quad.max().y(), spaceMetrics.quad.max().y(), 0.001f);

	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(FontCoverage, glyphBoxStandardChars) {
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "fontTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);

	// Test some standard ASCII chars to exercise the main getGlyphBox path.
	for (const char c: {'A', 'Z', '0', '9', '!', '~'}) {
		const auto metrics = myFont->getGlyphBox(c);
		// These chars should have non-trivial quads.
		EXPECT_GT(metrics.quad.max().x() - metrics.quad.min().x(), 0.0f);
	}

	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}

TEST(FontCoverage, getAdvanceMultipleChars) {
	core::Log::init(core::Log::Level::Off);
	auto app = mkShared<app::Application>(app::AppParams{.args = nullptr,
														 .frameLogFrequency = 0,
														 .name = "fontTest",
														 .assetsPattern = "",
														 .icon = "",
														 .width = 0,
														 .height = 0,
														 .argCount = 0,
														 .renderer = renderer::gpu::RenderAPI::Type::Null,
														 .hasGui = false,
														 .useDebugging = false,
														 .isDummy = true});
	auto& fontLibrary = app->getFontLibrary();
	const auto myFont = fontLibrary.getDefaultFont();
	ASSERT_NE(myFont, nullptr);

	// Advance should be positive for standard character pairs.
	EXPECT_GT(myFont->getAdvance('H', 'e'), 0.0f);
	EXPECT_GT(myFont->getAdvance('l', 'o'), 0.0f);
	EXPECT_GT(myFont->getAdvance(' ', 'W'), 0.0f);

	app::Application::invalidate();
	app.reset();
	core::Log::invalidate();
}
