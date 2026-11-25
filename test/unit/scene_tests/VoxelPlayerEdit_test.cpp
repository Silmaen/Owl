/**
 * @file VoxelPlayerEdit_test.cpp
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <data/voxel/Block.h>
#include <data/voxel/VoxelWorld.h>
#include <input/Gamepad.h>
#include <input/Input.h>
#include <input/MouseCode.h>
#include <math/vectors.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <window/Window.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>

using namespace owl;
using namespace owl::scene;

namespace {

// A stone slab at height 0 under a voxel player looking straight down at the block (0, 0, 0).
class VoxelPlayerEditTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		input::Input::init(window::Type::Null);
		auto& voxel = scene.createEntity("World").addComponent<component::VoxelWorld>();
		data::voxel::BlockType stone;
		stone.name = "stone";
		stoneId = voxel.registry.registerBlock(stone);
		for (int32_t x = -2; x <= 2; ++x)
			for (int32_t z = -2; z <= 2; ++z) voxel.world.setBlock({x, 0, z}, stoneId);
		playerEntity = scene.createEntity("Player");
		auto& transform = playerEntity.getComponent<component::Transform>().transform;
		transform.translation() = {0.5f, 3.f, 0.5f};
		transform.rotation() = {-1.5f, 0.f, 0.f};
		playerEntity.addComponent<component::VoxelPlayer>().placeBlock = stoneId;
	}

	void TearDown() override {
		scene.onEndRuntime();
		input::Input::invalidate();
		core::Log::invalidate();
	}

	void frame() {
		core::Timestep step;
		step.forceUpdate(std::chrono::milliseconds(16));
		scene.onUpdateRuntime(step, false);
	}

	auto world() -> data::voxel::VoxelWorld& {
		const auto view = scene.registry.view<component::VoxelWorld>();
		return view.get<component::VoxelWorld>(view.front()).world;
	}

	auto player() -> component::VoxelPlayer& { return playerEntity.getComponent<component::VoxelPlayer>(); }

	// Give an entity a script recording the block events and the aim of the player into the game state.
	void addRecorder(Entity& ioEntity, const std::string& iName) {
		const auto path = std::filesystem::temp_directory_path() / (iName + ".lua");
		std::ofstream file(path);
		file << "function on_block_broken(x, y, z, block)\n"
				"    gamestate.set('"
			 << iName
			 << "_broken', string.format('%d %d %d %d', x, y, z, block))\n"
				"end\n"
				"function on_block_placed(x, y, z, block)\n"
				"    gamestate.set('"
			 << iName
			 << "_placed', string.format('%d %d %d %d', x, y, z, block))\n"
				"end\n";
		file.close();
		ioEntity.addComponent<component::LuaScript>().scriptPath = path.string();
	}

	auto recorded(const std::string& iKey) const -> std::string {
		const auto value = scene.getGameState().get(iKey, std::string{});
		const auto* text = std::get_if<std::string>(&value);
		return text != nullptr ? *text : std::string{};
	}

	Scene scene;
	Entity playerEntity;
	data::voxel::BlockId stoneId = 0;
};

}// namespace

TEST_F(VoxelPlayerEditTest, AClickBreaksTheAimedBlockWithAFreeCursor) {
	scene.onStartRuntime();
	frame();
	ASSERT_TRUE(player().hasTarget);
	EXPECT_EQ(player().targetBlock, (math::vec3i{0, 0, 0}));
	input::Input::injectMouseButton(input::mouse::ButtonLeft);
	frame();
	EXPECT_EQ(world().getBlock({0, 0, 0}), data::voxel::g_AirBlock);
}

TEST_F(VoxelPlayerEditTest, ARightClickPlacesABlockAgainstTheAimedFace) {
	scene.onStartRuntime();
	frame();
	input::Input::injectMouseButton(input::mouse::ButtonRight);
	frame();
	EXPECT_EQ(world().getBlock({0, 1, 0}), stoneId);
}

TEST_F(VoxelPlayerEditTest, TheClickThatCapturesTheCursorIsNotAnEdit) {
	player().captureCursor = true;
	scene.onStartRuntime();
	frame();
	input::Input::injectMouseButton(input::mouse::ButtonLeft);
	frame();
	EXPECT_EQ(world().getBlock({0, 0, 0}), stoneId);
}

TEST_F(VoxelPlayerEditTest, AGamepadEditsWithoutACapturedCursor) {
	player().captureCursor = true;
	scene.onStartRuntime();
	frame();
	input::Input::injectGamepadAxis(0, input::gamepad::LeftTrigger, 1.f);
	frame();
	EXPECT_EQ(world().getBlock({0, 1, 0}), stoneId);
	input::Input::injectGamepadAxis(0, input::gamepad::LeftTrigger, 0.f);
	input::Input::injectGamepadAxis(0, input::gamepad::RightTrigger, 1.f);
	frame();
	EXPECT_EQ(world().getBlock({0, 1, 0}), data::voxel::g_AirBlock);
	EXPECT_EQ(world().getBlock({0, 0, 0}), stoneId);
}

TEST_F(VoxelPlayerEditTest, TheAimKeepsTheFaceItLooksAt) {
	scene.onStartRuntime();
	frame();
	ASSERT_TRUE(player().hasTarget);
	EXPECT_EQ(player().targetNormal, (math::vec3i{0, 1, 0}));
}

TEST_F(VoxelPlayerEditTest, AnEditCallsTheScriptsOfThePlayerAndOfTheWorld) {
	OWL_REQUIRE_MODULE(SCRIPT);
	addRecorder(playerEntity, "player");
	const auto view = scene.registry.view<component::VoxelWorld>();
	Entity worldEntity{view.front(), &scene};
	addRecorder(worldEntity, "world");
	scene.onStartRuntime();
	frame();
	input::Input::injectMouseButton(input::mouse::ButtonRight);
	frame();
	const std::string placed = "0 1 0 " + std::to_string(stoneId);
	EXPECT_EQ(recorded("player_placed"), placed);
	EXPECT_EQ(recorded("world_placed"), placed);
	input::Input::injectMouseButton(input::mouse::ButtonRight);
	input::Input::injectMouseButton(input::mouse::ButtonLeft);
	frame();
	const std::string broken = "0 1 0 " + std::to_string(stoneId);
	EXPECT_EQ(recorded("player_broken"), broken);
	EXPECT_EQ(recorded("world_broken"), broken);
	EXPECT_EQ(world().getBlock({0, 1, 0}), data::voxel::g_AirBlock);
}

TEST_F(VoxelPlayerEditTest, ARefusedPlacementCallsNothing) {
	OWL_REQUIRE_MODULE(SCRIPT);
	addRecorder(playerEntity, "player");
	playerEntity.getComponent<component::Transform>().transform.translation() = {0.5f, 1.9f, 0.5f};
	scene.onStartRuntime();
	frame();
	input::Input::injectMouseButton(input::mouse::ButtonRight);
	frame();
	EXPECT_EQ(world().getBlock({0, 1, 0}), data::voxel::g_AirBlock);
	EXPECT_TRUE(recorded("player_placed").empty());
}
