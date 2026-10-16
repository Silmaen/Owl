/**
 * @file LuaIsometricBindings_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <renderer/RendererIsometric.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/TilemapAsset.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <format>
#include <string>
#include <tuple>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

/// A scene with a camera, an 8x8 isometric map (two layers, a one-step plateau on the corners 2 to 4) and a script
/// running `iBody` on creation.
class LuaIsometric : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_scene = mkShared<scene::Scene>();
		auto camera = m_scene->createEntity("Camera");
		camera.getComponent<scene::component::Transform>().transform.translation() = {4.f, 4.f, 5.f};
		camera.addComponent<scene::component::Camera>().primary = true;
		m_map = m_scene->createEntity("Map");
		auto asset = mkShared<scene::TilemapAsset>();
		asset->resize(8, 8);
		asset->addLayer("ground");
		asset->addLayer("roads");
		std::ignore = asset->raiseArea({2, 2}, {4, 4});
		m_asset = asset;
		m_map.addComponent<scene::component::Tilemap>().asset = asset;
	}

	void TearDown() override { core::Log::invalidate(); }

	/// Run a script on creation and return the instance holding its globals.
	auto run(const std::string& iBody) -> shared<ScriptInstance> {
		const std::string text = std::format("map = {}\nfunction on_create()\n{}\nend\n",
											 static_cast<int64_t>(static_cast<uint64_t>(m_map.getUUID())), iBody);
		const std::vector<uint8_t> data(text.begin(), text.end());
		auto instance = mkShared<ScriptInstance>();
		instance->setScene(m_scene.get());
		EXPECT_TRUE(instance->createFromBuffer(data, "iso_test", static_cast<uint64_t>(m_map.getUUID())));
		instance->onCreate();
		return instance;
	}

	/// Window pixel showing a world point of the plateau, through the camera the bindings rebuild.
	[[nodiscard]] static auto pixelOf(const math::vec3& iPoint) -> math::vec2 {
		renderer::IsometricCamera camera;
		camera.setViewportSize({1280u, 720u});
		camera.setTarget({4.f, 4.f, 0.f});
		return camera.worldToViewport(iPoint);
	}

	shared<scene::Scene> m_scene;
	scene::Entity m_map;
	shared<scene::TilemapAsset> m_asset;
};

}// namespace

TEST_F(LuaIsometric, pickCellFindsTheCellUnderAPixel) {
	const auto pixel = pixelOf({3.5f, 2.5f, 0.5f});
	const auto instance = run(std::format("cx, cy = isometric.pick_cell(map, {}, {})", pixel.x(), pixel.y()));
	EXPECT_EQ(instance->getPropertyInt("cx").value_or(-1), 3);
	EXPECT_EQ(instance->getPropertyInt("cy").value_or(-1), 2);
}

TEST_F(LuaIsometric, pickCellReturnsNothingOffTheMap) {
	const auto instance = run("cx, cy = isometric.pick_cell(map, 2, 2)\nmissed = cx == nil and cy == nil");
	EXPECT_TRUE(instance->getPropertyBool("missed").value_or(false));
}

TEST_F(LuaIsometric, cellsAreReadAndWrittenPerLayer) {
	const auto instance = run("before_t, before_o = isometric.get_cell(map, 1, 3, 3)\n"
							  "done = isometric.set_cell(map, 1, 3, 3, -1, 3)\n"
							  "after_t, after_o = isometric.get_cell(map, 1, 3, 3)\n"
							  "ground_t, ground_o = isometric.get_cell(map, 0, 3, 3)\n"
							  "outside = isometric.set_cell(map, 1, 8, 3, -1, 3)\n"
							  "bad_layer = isometric.set_cell(map, 5, 3, 3, -1, 3)\n"
							  "far_t, far_o = isometric.get_cell(map, 1, 99, 99)");
	EXPECT_EQ(instance->getPropertyInt("before_t").value_or(0), -1);
	EXPECT_EQ(instance->getPropertyInt("before_o").value_or(0), -1);
	EXPECT_TRUE(instance->getPropertyBool("done").value_or(false));
	EXPECT_EQ(instance->getPropertyInt("after_t").value_or(0), -1);
	EXPECT_EQ(instance->getPropertyInt("after_o").value_or(0), 3);
	EXPECT_EQ(instance->getPropertyInt("ground_o").value_or(0), -1);
	EXPECT_FALSE(instance->getPropertyBool("outside").value_or(true));
	EXPECT_FALSE(instance->getPropertyBool("bad_layer").value_or(true));
	EXPECT_EQ(instance->getPropertyInt("far_t").value_or(0), -1);
	EXPECT_EQ(m_asset->layers[1].getCell(3, 3).overlay, 3);
}

TEST_F(LuaIsometric, slopeAndHeightFollowTheCorners) {
	const auto instance = run("flat = isometric.get_slope(map, 0, 0)\n"
							  "plateau = isometric.get_slope(map, 2, 2)\n"
							  "edge = isometric.get_slope(map, 1, 2)\n"
							  "ground_z = isometric.get_height(map, 6.5, 6.5)\n"
							  "plateau_z = isometric.get_height(map, 2.5, 2.5)\n"
							  "outside_z = isometric.get_height(map, 20, 20)");
	EXPECT_EQ(instance->getPropertyInt("flat").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("plateau").value_or(-1), 0);
	EXPECT_EQ(instance->getPropertyInt("edge").value_or(-1), 10) << "the cell left of the plateau rises on its right";
	EXPECT_NEAR(instance->getPropertyFloat("ground_z").value_or(-1.f), 0.f, 1e-3f);
	EXPECT_NEAR(instance->getPropertyFloat("plateau_z").value_or(-1.f), 0.5f, 1e-3f);
	EXPECT_NEAR(instance->getPropertyFloat("outside_z").value_or(-1.f), 0.f, 1e-3f);
}

TEST_F(LuaIsometric, entitiesWithoutATilemapAreIgnored) {
	const auto instance = run("map = 0\ncx = isometric.pick_cell(map, 640, 360)\n"
							  "done = isometric.set_cell(map, 0, 1, 1, 0, 0)\n"
							  "slope = isometric.get_slope(map, 1, 1)\nz = isometric.get_height(map, 1, 1)");
	EXPECT_FALSE(instance->getPropertyBool("done").value_or(true));
	EXPECT_EQ(instance->getPropertyInt("slope").value_or(-1), 0);
	EXPECT_NEAR(instance->getPropertyFloat("z").value_or(-1.f), 0.f, 1e-3f);
}

TEST_F(LuaIsometric, mouseOverUiFollowsTheHudState) {
	EXPECT_FALSE(run("over = ui.is_mouse_over_ui()")->getPropertyBool("over").value_or(true));
	m_scene->getUiInputState().consuming = true;
	EXPECT_TRUE(run("over = ui.is_mouse_over_ui()")->getPropertyBool("over").value_or(false));
}
