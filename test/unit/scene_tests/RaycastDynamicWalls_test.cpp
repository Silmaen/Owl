/**
 * @file RaycastDynamicWalls_test.cpp
 * @author Silmaen
 * @date 11/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/SerializerImpl.h>
#include <core/Timestep.h>
#include <input/Input.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneSerializer.h>
#include <scene/UiInputSystem.h>
#include <scene/component/RaycastDoor.h>
#include <scene/component/RaycastPushWall.h>
#include <scene/component/UiWindow.h>
#include <scene/component/Visibility.h>
#include <scene/component/components.h>
#include <scene/systems/EngineSystems.h>
#include <window/Window.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace owl;

namespace {

class RaycastDynamicWallsTest : public ::testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }
	void TearDown() override { core::Log::invalidate(); }
};

// Build a default Scene with a single door entity sitting at the origin, ready
// to be ticked. Returns the entity and exposes the underlying scene by reference.
// The slide distance parameter is ignored: a `RaycastDoor` always slides exactly
// one cell, the speed knob shaping the timing.
auto makeDoorScene(scene::Scene& oScene, const float = 1.0f, const float iSlideSpeed = 4.0f,
				   const float iHoldTime = 0.5f) -> scene::Entity {
	auto entity = oScene.createEntity("Door");
	entity.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 0.f};
	auto& door = entity.addComponent<scene::component::RaycastDoor>();
	door.openingDirection = scene::component::RaycastDoor::OpeningDirection::East;
	door.slideSpeed = iSlideSpeed;
	door.holdTime = iHoldTime;
	door.closeSpeed = iSlideSpeed;
	// Disable the engine-handled action path so the test drives state explicitly.
	door.interactionAction.clear();
	return entity;
}

auto makePushScene(scene::Scene& oScene, const float iSlideDistance = 2.0f, const float iSlideSpeed = 4.0f)
		-> scene::Entity {
	auto entity = oScene.createEntity("PushWall");
	entity.getComponent<scene::component::Transform>().transform.translation() = math::vec3{0.f, 0.f, 0.f};
	auto& push = entity.addComponent<scene::component::RaycastPushWall>();
	push.slideDirection = math::vec2{0.f, 1.f};
	push.slideDistance = iSlideDistance;
	push.slideSpeed = iSlideSpeed;
	push.interactionAction.clear();
	return entity;
}

}// namespace

TEST_F(RaycastDynamicWallsTest, DoorDefaultsLookSane) {
	scene::Scene scn;
	const auto door = makeDoorScene(scn);
	const auto& d = door.getComponent<scene::component::RaycastDoor>();
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Idle);
	EXPECT_FLOAT_EQ(d.currentOffset, 0.f);
}

TEST_F(RaycastDynamicWallsTest, DoorOpeningProgressesAndLatchesOpen) {
	scene::Scene scn;
	auto door = makeDoorScene(scn, /*dist*/ 1.f, /*speed*/ 4.f, /*hold*/ 1.f);
	door.getComponent<scene::component::RaycastDoor>().state = scene::component::RaycastDoor::State::Opening;

	// 1.0/4.0 = 0.25 s to reach full open. Step in 0.1 s chunks: 3 ticks should still be Opening,
	// the 4th tick should latch Open.
	scene::systems::updateRaycastDynamicWalls(scn, 0.1f);
	scene::systems::updateRaycastDynamicWalls(scn, 0.1f);
	const auto& d = door.getComponent<scene::component::RaycastDoor>();
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Opening);
	EXPECT_GT(d.currentOffset, 0.f);
	EXPECT_LT(d.currentOffset, 1.f);
	scene::systems::updateRaycastDynamicWalls(scn, 0.3f);
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Open);
	EXPECT_FLOAT_EQ(d.currentOffset, 1.f);
	EXPECT_NEAR(d.holdTimer, 1.f, 1e-3f);
}

TEST_F(RaycastDynamicWallsTest, DoorClosesAfterHoldElapses) {
	scene::Scene scn;
	auto door = makeDoorScene(scn, 1.f, 4.f, 0.2f);
	auto& d = door.getComponent<scene::component::RaycastDoor>();
	// Fast-forward to fully open.
	d.state = scene::component::RaycastDoor::State::Opening;
	scene::systems::updateRaycastDynamicWalls(scn, 1.0f);
	ASSERT_EQ(d.state, scene::component::RaycastDoor::State::Open);
	// Hold expires after 0.2 s, then Closing animation runs for another 0.25 s back to zero.
	scene::systems::updateRaycastDynamicWalls(scn, 0.21f);
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Closing);
	scene::systems::updateRaycastDynamicWalls(scn, 1.0f);
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Idle);
	EXPECT_FLOAT_EQ(d.currentOffset, 0.f);
}

TEST_F(RaycastDynamicWallsTest, DoorAdvanceKeepsEntityTransformStaticOnlyPlateMoves) {
	// The cell entity stays put — the door is a 1×1 cell whose internal plate is
	// what slides. After fully opening, `Transform.translation` is unchanged and
	// `currentOffset` equals 1 (the door always slides exactly one cell).
	scene::Scene scn;
	auto door = makeDoorScene(scn, 1.f, 4.f, 1.f);
	door.getComponent<scene::component::RaycastDoor>().state = scene::component::RaycastDoor::State::Opening;
	const float startX = door.getComponent<scene::component::Transform>().transform.translation().x();
	const float startY = door.getComponent<scene::component::Transform>().transform.translation().y();
	scene::systems::updateRaycastDynamicWalls(scn, 0.25f);// 1 cell at speed 4 → 0.25 s to reach the open pose
	const auto& d = door.getComponent<scene::component::RaycastDoor>();
	EXPECT_EQ(d.state, scene::component::RaycastDoor::State::Open);
	EXPECT_FLOAT_EQ(d.currentOffset, 1.f);
	const float newX = door.getComponent<scene::component::Transform>().transform.translation().x();
	const float newY = door.getComponent<scene::component::Transform>().transform.translation().y();
	EXPECT_FLOAT_EQ(newX, startX);
	EXPECT_FLOAT_EQ(newY, startY);
}

TEST_F(RaycastDynamicWallsTest, PushWallSlidesOnceAndStaysFinal) {
	scene::Scene scn;
	auto push = makePushScene(scn, 2.f, 4.f);
	push.getComponent<scene::component::RaycastPushWall>().state = scene::component::RaycastPushWall::State::Moving;
	scene::systems::updateRaycastDynamicWalls(scn, 1.0f);// way past the 0.5 s needed to slide 2 cells
	const auto& p = push.getComponent<scene::component::RaycastPushWall>();
	EXPECT_EQ(p.state, scene::component::RaycastPushWall::State::Final);
	EXPECT_FLOAT_EQ(p.currentOffset, 2.f);
	// Y axis since slideDirection = {0,1}.
	EXPECT_FLOAT_EQ(push.getComponent<scene::component::Transform>().transform.translation().y(), 2.f);
	// Subsequent ticks must not push the wall further.
	scene::systems::updateRaycastDynamicWalls(scn, 1.0f);
	EXPECT_FLOAT_EQ(p.currentOffset, 2.f);
}

TEST_F(RaycastDynamicWallsTest, DoorYamlRoundTrip) {
	scene::component::RaycastDoor source{};
	source.openingDirection = scene::component::RaycastDoor::OpeningDirection::South;
	source.slideSpeed = 3.0f;
	source.holdTime = 1.25f;
	source.closeSpeed = 2.5f;
	source.interactionAction = "open_door";
	source.interactionRange = 2.0f;

	const core::Serializer outSerializer;
	outSerializer.getImpl()->emitter << YAML::BeginMap;
	source.serialize(outSerializer);
	outSerializer.getImpl()->emitter << YAML::EndMap;
	const std::string yamlText = outSerializer.getImpl()->emitter.c_str();

	const core::YamlDocument document{yamlText};
	const auto root = document.getRoot();
	ASSERT_TRUE(root[scene::component::RaycastDoor::key()]);
	scene::component::RaycastDoor restored;
	const core::Serializer inSerializer;
	inSerializer.getImpl()->node = root[scene::component::RaycastDoor::key()];
	restored.deserialize(inSerializer);

	EXPECT_EQ(restored.openingDirection, scene::component::RaycastDoor::OpeningDirection::South);
	EXPECT_FLOAT_EQ(restored.slideSpeed, 3.0f);
	EXPECT_FLOAT_EQ(restored.holdTime, 1.25f);
	EXPECT_FLOAT_EQ(restored.closeSpeed, 2.5f);
	EXPECT_EQ(restored.interactionAction, "open_door");
	EXPECT_FLOAT_EQ(restored.interactionRange, 2.0f);
	// Runtime fields always reset to the closed/idle pose.
	EXPECT_EQ(restored.state, scene::component::RaycastDoor::State::Idle);
	EXPECT_FLOAT_EQ(restored.currentOffset, 0.f);
}

TEST_F(RaycastDynamicWallsTest, PushWallYamlRoundTrip) {
	scene::component::RaycastPushWall source{};
	source.slideDirection = math::vec2{-1.f, 0.f};
	source.slideDistance = 3.5f;
	source.slideSpeed = 0.75f;
	source.interactionAction = "push";
	source.interactionRange = 1.75f;

	const core::Serializer outSerializer;
	outSerializer.getImpl()->emitter << YAML::BeginMap;
	source.serialize(outSerializer);
	outSerializer.getImpl()->emitter << YAML::EndMap;
	const std::string yamlText = outSerializer.getImpl()->emitter.c_str();

	const core::YamlDocument document{yamlText};
	const auto root = document.getRoot();
	ASSERT_TRUE(root[scene::component::RaycastPushWall::key()]);
	scene::component::RaycastPushWall restored;
	const core::Serializer inSerializer;
	inSerializer.getImpl()->node = root[scene::component::RaycastPushWall::key()];
	restored.deserialize(inSerializer);

	EXPECT_FLOAT_EQ(restored.slideDirection.x(), -1.f);
	EXPECT_FLOAT_EQ(restored.slideDirection.y(), 0.f);
	EXPECT_FLOAT_EQ(restored.slideDistance, 3.5f);
	EXPECT_FLOAT_EQ(restored.slideSpeed, 0.75f);
	EXPECT_EQ(restored.interactionAction, "push");
	EXPECT_FLOAT_EQ(restored.interactionRange, 1.75f);
	EXPECT_EQ(restored.state, scene::component::RaycastPushWall::State::Idle);
	EXPECT_FLOAT_EQ(restored.currentOffset, 0.f);
}

TEST_F(RaycastDynamicWallsTest, DoorSurvivesSceneSerializerRoundTrip) {
	// Full Scene-level round-trip via SceneSerializer: write the scene to a buffer, reload it
	// and verify the door component made it through with its fields intact.
	auto sourceScene = mkShared<scene::Scene>();
	auto door = sourceScene->createEntity("Door");
	auto& d = door.addComponent<scene::component::RaycastDoor>();
	d.openingDirection = scene::component::RaycastDoor::OpeningDirection::East;
	d.slideSpeed = 2.0f;
	d.holdTime = 1.5f;
	d.closeSpeed = 3.0f;
	d.interactionAction = "interact";
	d.interactionRange = 1.25f;
	const auto uuid = door.getUUID();

	const scene::SceneSerializer serializer(sourceScene);
	const std::string yaml = serializer.serializeToString();
	ASSERT_FALSE(yaml.empty());

	auto loadedScene = mkShared<scene::Scene>();
	const scene::SceneSerializer dser(loadedScene);
	const std::vector<uint8_t> buffer(yaml.begin(), yaml.end());
	ASSERT_TRUE(dser.deserializeFromBuffer(buffer));
	const auto restored = loadedScene->findEntityByUUID(uuid);
	ASSERT_TRUE(restored);
	ASSERT_TRUE(restored.hasComponent<scene::component::RaycastDoor>());
	const auto& r = restored.getComponent<scene::component::RaycastDoor>();
	EXPECT_EQ(r.openingDirection, scene::component::RaycastDoor::OpeningDirection::East);
	EXPECT_FLOAT_EQ(r.holdTime, 1.5f);
	EXPECT_EQ(r.interactionAction, "interact");
	EXPECT_EQ(r.state, scene::component::RaycastDoor::State::Idle);
}

namespace {

auto deserializeDoor(const std::string& iYaml) -> scene::component::RaycastDoor {
	const core::YamlDocument document{iYaml};
	scene::component::RaycastDoor door;
	const core::Serializer serializer;
	serializer.getImpl()->node = document.getRoot()[scene::component::RaycastDoor::key()];
	door.deserialize(serializer);
	return door;
}

}// namespace

TEST_F(RaycastDynamicWallsTest, LegacyInteractionKeyZeroDisablesTheAction) {
	const auto disabled = deserializeDoor("RaycastDoor:\n  interactionKey: 0\n");
	EXPECT_TRUE(disabled.interactionAction.empty());
	const auto enabled = deserializeDoor("RaycastDoor:\n  interactionKey: 69\n");
	EXPECT_EQ(enabled.interactionAction, "interact");
	const auto renamed = deserializeDoor("RaycastDoor:\n  interactionAction: use\n  interactionKey: 0\n");
	EXPECT_EQ(renamed.interactionAction, "use");
}

TEST_F(RaycastDynamicWallsTest, DoorOpensWhenItsActionGoesDownInRange) {
	input::Input::init(window::Type::Null);
	scene::Scene scn;
	auto entity = scn.createEntity("Door");
	entity.addComponent<scene::component::RaycastDoor>().interactionAction = "interact";
	auto player = scn.createEntity("Player");
	player.addComponent<scene::component::Player>();
	scn.onStartRuntime();
	scn.onUpdateRuntime(core::Timestep{}, false);
	EXPECT_EQ(entity.getComponent<scene::component::RaycastDoor>().state, scene::component::RaycastDoor::State::Idle);
	input::Input::injectKey(input::key::E);
	scn.onUpdateRuntime(core::Timestep{}, false);
	EXPECT_NE(entity.getComponent<scene::component::RaycastDoor>().state, scene::component::RaycastDoor::State::Idle);
	scn.onEndRuntime();
	input::Input::invalidate();
}

TEST_F(RaycastDynamicWallsTest, ShownWindowHoldsTheInteractionBack) {
	input::Input::init(window::Type::Null);
	scene::Scene scn;
	auto door = scn.createEntity("Door");
	door.addComponent<scene::component::RaycastDoor>().interactionAction = "interact";
	auto push = scn.createEntity("PushWall");
	push.addComponent<scene::component::RaycastPushWall>().interactionAction = "interact";
	auto player = scn.createEntity("Player");
	player.addComponent<scene::component::Player>();
	auto window = scn.createEntity("PauseWindow");
	window.addComponent<scene::component::UiWindow>();
	scn.onStartRuntime();
	EXPECT_TRUE(scene::UiInputSystem::isWorldInteractionBlocked(scn));
	input::Input::injectKey(input::key::E);
	scn.onUpdateRuntime(core::Timestep{}, false);
	EXPECT_EQ(door.getComponent<scene::component::RaycastDoor>().state, scene::component::RaycastDoor::State::Idle);
	EXPECT_EQ(push.getComponent<scene::component::RaycastPushWall>().state,
			  scene::component::RaycastPushWall::State::Idle);

	// The window closed by the click of this frame (Resume) still holds that frame back.
	input::Input::injectKey(input::key::E);
	window.getComponent<scene::component::Visibility>().gameVisible = false;
	scn.onUpdateRuntime(core::Timestep{}, false);
	scn.getUiInputState().windowShown = true;
	input::Input::injectKey(input::key::E);
	scn.onUpdateRuntime(core::Timestep{}, false);
	EXPECT_EQ(door.getComponent<scene::component::RaycastDoor>().state, scene::component::RaycastDoor::State::Idle);

	input::Input::injectKey(input::key::E);
	scn.getUiInputState().windowShown = false;
	EXPECT_FALSE(scene::UiInputSystem::isWorldInteractionBlocked(scn));
	scn.onUpdateRuntime(core::Timestep{}, false);
	input::Input::injectKey(input::key::E);
	scn.onUpdateRuntime(core::Timestep{}, false);
	EXPECT_NE(door.getComponent<scene::component::RaycastDoor>().state, scene::component::RaycastDoor::State::Idle);
	EXPECT_NE(push.getComponent<scene::component::RaycastPushWall>().state,
			  scene::component::RaycastPushWall::State::Idle);
	scn.onEndRuntime();
	input::Input::invalidate();
}
