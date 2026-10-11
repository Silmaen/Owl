/**
 * @file IconBank_test.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/IconBank.h>
#include <gui/IconFont.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::gui;

namespace {

auto writeSvg(const std::filesystem::path& iFile) -> std::filesystem::path {
	std::ofstream out(iFile);
	out << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">)"
		<< R"(<rect x="3" y="3" width="18" height="18" rx="3" fill="white" fill-opacity=".2" stroke="white"/></svg>)";
	return iFile;
}

// Code point of a single UTF-8 encoded character.
auto decode(const char* iUtf8) -> uint32_t {
	const auto* bytes = reinterpret_cast<const unsigned char*>(iUtf8);
	if (std::strlen(iUtf8) != 3 || (bytes[0] & 0xF0u) != 0xE0u)
		return 0;
	return ((bytes[0] & 0x0Fu) << 12u) | ((bytes[1] & 0x3Fu) << 6u) | (bytes[2] & 0x3Fu);
}

}// namespace

TEST(IconBank, BuildsOneTierPerDisplaySize) {
	core::Log::init(core::Log::Level::Off);
	const auto dir = std::filesystem::temp_directory_path() / "owl_icon_bank_test";
	std::filesystem::create_directories(dir);
	IconBank bank;
	bank.build({{"square", writeSvg(dir / "square.svg")}, {"missing", dir / "missing.svg"}}, 64,
			   {.primary = {0.1f, 0.2f, 0.3f, 1.f}, .secondary = {1.f, 0.f, 0.f, 1.f}});
	EXPECT_EQ(bank.getTierSizes(), (std::vector<uint32_t>{16, 24, 32, 64}));
	EXPECT_TRUE(bank.hasIcon("square"));
	EXPECT_FALSE(bank.hasIcon("missing"));
	EXPECT_FALSE(bank.getIcon("unknown", 16.f).has_value());

	bank.build({{"square", dir / "square.svg"}}, 24);
	EXPECT_EQ(bank.getTierSizes(), (std::vector<uint32_t>{16, 24, 32}));

	bank.clear();
	EXPECT_TRUE(bank.getTierSizes().empty());
	EXPECT_FALSE(bank.hasIcon("square"));
	EXPECT_FALSE(bank.getIcon("square").has_value());
	EXPECT_EQ(bank.getAtlasTexture(), nullptr);
	std::filesystem::remove_all(dir);
	core::Log::invalidate();
}

TEST(IconFont, GlyphsAreLucidePrivateUseCodePoints) {
	for (const auto* glyph: {icon::save, icon::play, icon::trash2, icon::chevronDown, icon::triangleAlert,
							 icon::rotate3d, icon::folderOpen}) {
		const auto codePoint = decode(glyph);
		EXPECT_GE(codePoint, 0xE000u);
		EXPECT_LE(codePoint, 0xF8FFu);
	}
	EXPECT_STRNE(icon::play, icon::pause);
}
