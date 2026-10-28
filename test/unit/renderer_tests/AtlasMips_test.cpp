/**
 * @file AtlasMips_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/AtlasMips.h>
#include <renderer/TextureDecoder.h>
#include <renderer/gpu/Texture.h>
#include <renderer/gpu/null/Texture.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

class AtlasMipsTest : public testing::Test {
protected:
	static void SetUpTestSuite() { core::Log::init(core::Log::Level::Off); }
};

constexpr std::array<std::array<uint8_t, 4>, 4> g_cellColours{
		{{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {255, 255, 0, 255}}};

// An atlas of 2x2 cells of the given size, each cell filled with its own colour.
auto makeAtlas(const math::vec2ui& iCell) -> std::vector<uint8_t> {
	const math::vec2ui size{iCell.x() * 2, iCell.y() * 2};
	std::vector<uint8_t> pixels(static_cast<size_t>(size.surface()) * 4);
	for (uint32_t y = 0; y < size.y(); ++y) {
		for (uint32_t x = 0; x < size.x(); ++x) {
			const auto& colour = g_cellColours[(y / iCell.y()) * 2 + x / iCell.x()];
			for (size_t channel = 0; channel < 4; ++channel)
				pixels[(static_cast<size_t>(y) * size.x() + x) * 4 + channel] = colour[channel];
		}
	}
	return pixels;
}

}// namespace

TEST_F(AtlasMipsTest, LevelCountStopsAtTheOddCellSide) {
	EXPECT_EQ(cellMipLevelCount({16, 16}), 5u);
	EXPECT_EQ(cellMipLevelCount({128, 64}), 7u);
	EXPECT_EQ(cellMipLevelCount({18, 18}), 2u);
	EXPECT_EQ(cellMipLevelCount({17, 16}), 1u);
	EXPECT_EQ(cellMipLevelCount({0, 16}), 1u);
}

TEST_F(AtlasMipsTest, EveryLevelKeepsEachCellToItself) {
	const math::vec2ui cell{8, 8};
	const auto atlas = makeAtlas(cell);
	const auto levels = buildCellMips(atlas, {16, 16}, cell);
	ASSERT_EQ(levels.levels.size(), 4u);
	EXPECT_EQ(levels.format, gpu::ImageFormat::Rgba8);
	for (size_t level = 0; level < levels.levels.size(); ++level) {
		const uint32_t side = 16u >> level;
		const uint32_t cellSide = 8u >> level;
		ASSERT_EQ(levels.levels[level].size(), static_cast<size_t>(side) * side * 4) << "level " << level;
		for (uint32_t y = 0; y < side; ++y) {
			for (uint32_t x = 0; x < side; ++x) {
				const auto& colour = g_cellColours[(y / cellSide) * 2 + x / cellSide];
				for (size_t channel = 0; channel < 4; ++channel)
					ASSERT_EQ(levels.levels[level][(static_cast<size_t>(y) * side + x) * 4 + channel], colour[channel])
							<< "level " << level << " texel " << x << "," << y;
			}
		}
	}
}

TEST_F(AtlasMipsTest, TransparentTexelsDoNotDarkenTheColour) {
	// One cell of 2x2: a single opaque white texel and three transparent black ones.
	const std::vector<uint8_t> pixels{255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const auto levels = buildCellMips(pixels, {2, 2}, {2, 2});
	ASSERT_EQ(levels.levels.size(), 2u);
	const auto& texel = levels.levels[1];
	EXPECT_EQ(texel[0], 255);
	EXPECT_EQ(texel[1], 255);
	EXPECT_EQ(texel[2], 255);
	EXPECT_EQ(texel[3], 64);
}

TEST_F(AtlasMipsTest, ShortBufferGivesNoLevel) {
	const std::vector<uint8_t> pixels(8, 0);
	EXPECT_TRUE(buildCellMips(pixels, {4, 4}, {2, 2}).levels.empty());
}

TEST_F(AtlasMipsTest, TextureTurnsIntoAPixelArtAtlas) {
	const auto file = std::filesystem::temp_directory_path() / "owl_atlas_mips.png";
	ASSERT_TRUE(writeImagePng(file, {32, 32}, makeAtlas({16, 16})));
	gpu::null::Texture2D texture{file};
	ASSERT_TRUE(texture.isLoaded());
	EXPECT_TRUE(texture.setPixelArtCells({16, 16}));
	EXPECT_EQ(texture.getFilterMode(), gpu::FilterMode::PixelArt);
	EXPECT_EQ(texture.getSpecification().atlasCell, (math::vec2ui{16, 16}));
	EXPECT_EQ(texture.getSpecification().levelCount, 5u);
	EXPECT_FALSE(texture.setPixelArtCells({0, 0}));
	std::filesystem::remove(file);
}

TEST_F(AtlasMipsTest, TextureWithoutSourceCannotBuildMips) {
	gpu::null::Texture2D texture{gpu::Texture::Specification{.size = {16, 16}}};
	EXPECT_FALSE(texture.setPixelArtCells({8, 8}));
	EXPECT_EQ(texture.getFilterMode(), gpu::FilterMode::PixelArt);
}
