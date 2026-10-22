/**
 * @file component_test.cpp
 * @author Silmaen
 * @date 08/01/2025
 * Copyright (c) 2025 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <gui/UiLayer.h>
#include <gui/component/render.h>
#include <scene/component/components.h>

#include <filesystem>

using namespace owl::core;
using namespace owl::gui;
using namespace owl::gui::component;
using namespace owl::scene::component;

TEST(Component, render) {
	Log::init(owl::core::Log::Level::Off);
	UiLayer layer;
	layer.disableApp();
	layer.enableDocking();
	layer.onAttach();
	layer.begin();
	Transform tran{};
	renderProps(tran);
	Camera cam;
	cam.camera.setProjectionType(owl::scene::SceneCamera::ProjectionType::Orthographic);
	renderProps(cam);
	Camera cam2;
	cam2.camera.setProjectionType(owl::scene::SceneCamera::ProjectionType::Perspective);
	renderProps(cam2);
	SpriteRenderer spr{};
	renderProps(spr);
	AnimatedSpriteRenderer animSpr{};
	renderProps(animSpr);
	CircleRenderer circ;
	renderProps(circ);
	Text text{};
	renderProps(text);
	PhysicBody pbody;
	renderProps(pbody);
	Player play;
	renderProps(play);
	Trigger trig;
	renderProps(trig);
	EntityLink elink;
	renderProps(elink);

	layer.end();
	layer.onDetach();
	Log::invalidate();
	EXPECT_TRUE(true);
}

TEST(Component, OpenAssetGoesToTheHandler) {
	EXPECT_FALSE(owl::gui::component::openAsset("tilemaps/level.owltilemap"));
	std::filesystem::path opened;
	owl::gui::component::setOpenAssetHandler([&opened](const std::filesystem::path& iPath) -> void { opened = iPath; });
	EXPECT_FALSE(owl::gui::component::openAsset({}));
	EXPECT_TRUE(owl::gui::component::openAsset("tilemaps/level.owltilemap"));
	EXPECT_EQ(opened, std::filesystem::path{"tilemaps/level.owltilemap"});
	owl::gui::component::setOpenAssetHandler({});
	EXPECT_FALSE(owl::gui::component::openAsset("scripts/hero.lua"));
}

TEST(Component, OpenAssetAtLineGoesToTheLineHandlerThenFallsBack) {
	EXPECT_FALSE(owl::gui::component::openAssetAtLine("scripts/hero.lua", 12));
	std::filesystem::path opened;
	owl::gui::component::setOpenAssetHandler([&opened](const std::filesystem::path& iPath) -> void { opened = iPath; });
	EXPECT_TRUE(owl::gui::component::openAssetAtLine("scripts/hero.lua", 12));
	EXPECT_EQ(opened, std::filesystem::path{"scripts/hero.lua"});
	uint32_t line = 0;
	owl::gui::component::setOpenAssetAtLineHandler(
			[&opened, &line](const std::filesystem::path& iPath, const uint32_t iLine) -> void {
				opened = iPath;
				line = iLine;
			});
	EXPECT_FALSE(owl::gui::component::openAssetAtLine({}, 3));
	EXPECT_TRUE(owl::gui::component::openAssetAtLine("scripts/door.lua", 7));
	EXPECT_EQ(opened, std::filesystem::path{"scripts/door.lua"});
	EXPECT_EQ(line, 7u);
	owl::gui::component::setOpenAssetHandler({});
	owl::gui::component::setOpenAssetAtLineHandler({});
}
