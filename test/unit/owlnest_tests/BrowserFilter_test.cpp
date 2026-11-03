/**
 * @file BrowserFilter_test.cpp
 * @author Silmaen
 * @date 02/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "panel/BrowserFilter.h"
#include "testHelper.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace owl::nest::panel;

TEST(BrowserFilter, TheAssetsOfOwlNestShowWithEveryFileOnly) {
	EXPECT_FALSE(isBrowsedRoot(AssetOrigin::Application, false));
	EXPECT_TRUE(isBrowsedRoot(AssetOrigin::Application, true));
	EXPECT_TRUE(isBrowsedRoot(AssetOrigin::Project, false));
	EXPECT_TRUE(isBrowsedRoot(AssetOrigin::Engine, false));
	EXPECT_TRUE(isBrowsedRoot(AssetOrigin::WorkingDirectory, false));
}

TEST(BrowserFilter, HidesTheShaderBuildProductsAndTheEngineLogos) {
	const std::filesystem::path work{"/game/bin/assets"};
	EXPECT_TRUE(isHiddenEntry(work / "shaders/renderer3D/spirv", work, AssetOrigin::WorkingDirectory));
	EXPECT_TRUE(isHiddenEntry(work / "shaders/sky.frag.spv", work, AssetOrigin::WorkingDirectory));
	EXPECT_TRUE(isHiddenEntry(work / "shaders/sky.frag.spv.hash", work, AssetOrigin::WorkingDirectory));
	EXPECT_FALSE(isHiddenEntry(work / "shaders/sky.slang", work, AssetOrigin::WorkingDirectory));
	const std::filesystem::path engine{"/owl/engine_assets"};
	EXPECT_TRUE(isHiddenEntry(engine / "logo", engine, AssetOrigin::Engine));
	EXPECT_TRUE(isHiddenEntry(engine / "logo/owl.png", engine, AssetOrigin::Engine));
	EXPECT_FALSE(isHiddenEntry(engine / "fonts", engine, AssetOrigin::Engine));
	// A project may hold a folder named logo of its own.
	const std::filesystem::path project{"/work/my_game"};
	EXPECT_FALSE(isHiddenEntry(project / "logo", project, AssetOrigin::Project));
}

TEST(BrowserFilter, PackagedFilesKnowTheirFilesAndTheFoldersAboveThem) {
	PackagedFiles packaged;
	packaged.assign(
			{"/work/game/scenes/level.owl", "/work/game/textures/../textures/wall.png", "/work/game/scenes/level.owl"});
	EXPECT_EQ(packaged.size(), 2u);
	EXPECT_TRUE(packaged.contains("/work/game/scenes/level.owl"));
	EXPECT_TRUE(packaged.contains("/work/game/textures/wall.png"));
	EXPECT_FALSE(packaged.contains("/work/game/textures/unused.png"));
	EXPECT_TRUE(packaged.holds("/work/game"));
	EXPECT_TRUE(packaged.holds("/work/game/textures/"));
	EXPECT_FALSE(packaged.holds("/work/game/sounds"));
	EXPECT_FALSE(packaged.holds("/work/game/scenes/level"));
}
