/**
 * @file SceneCollision_test.cpp
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>

using namespace owl;
using namespace owl::scene;

namespace {

class SceneCollisionTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_scene_collision_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	void TearDown() override {
		if (physics::PhysicCommand::isInitialized())
			physics::PhysicCommand::destroy();
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto writeScript(const std::string& iName, const std::string& iCode) const -> std::string {
		const auto path = m_dir / iName;
		std::ofstream file(path);
		file << iCode;
		return path.string();
	}

	std::filesystem::path m_dir;
};

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

auto getInt(const Scene& iScene, const std::string& iKey) -> int64_t {
	const auto value = iScene.getGameState().get(iKey, int64_t{0});
	if (const auto* asInt = std::get_if<int64_t>(&value); asInt != nullptr)
		return *asInt;
	if (const auto* asFloat = std::get_if<float>(&value); asFloat != nullptr)
		return static_cast<int64_t>(*asFloat);
	return -1;
}

auto getString(const Scene& iScene, const std::string& iKey) -> std::string {
	const auto value = iScene.getGameState().get(iKey, std::string{});
	if (const auto* asString = std::get_if<std::string>(&value); asString != nullptr)
		return *asString;
	return {};
}

auto addBody(Scene& ioScene, const std::string& iName, const SceneBody::BodyType iType, const math::vec3f& iPosition,
			 const std::string& iScript) -> Entity {
	auto entity = ioScene.createEntity(iName);
	entity.getComponent<component::Transform>().transform.translation() = iPosition;
	entity.addComponent<component::PhysicBody>().body.type = iType;
	if (!iScript.empty())
		entity.addComponent<component::LuaScript>().scriptPath = iScript;
	return entity;
}

constexpr auto g_recordCollision = R"(
function on_collision(other_id)
    local me = entity.get_name(entity_id)
    gamestate.set(me .. "_hits", gamestate.get(me .. "_hits", 0) + 1)
    gamestate.set(me .. "_other", entity.get_name(other_id))
end
)";

}// namespace

TEST_F(SceneCollisionTest, CollisionCalledOnceOnEachSideWithOtherId) {
	const auto path = writeScript("record.lua", g_recordCollision);
	Scene scn;
	addBody(scn, "Ground", SceneBody::BodyType::Static, {0.f, 0.f, 0.f}, path)
			.getComponent<component::Transform>()
			.transform.scale() = {10.f, 1.f, 1.f};
	addBody(scn, "Box", SceneBody::BodyType::Dynamic, {0.f, 1.5f, 0.f}, path);
	addBody(scn, "FarBox", SceneBody::BodyType::Dynamic, {50.f, 1.5f, 0.f}, path);
	scn.onStartRuntime();
	for (int i = 0; i < 120; ++i) scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getInt(scn, "Ground_hits"), 1);
	EXPECT_EQ(getInt(scn, "Box_hits"), 1);
	EXPECT_EQ(getString(scn, "Ground_other"), "Box");
	EXPECT_EQ(getString(scn, "Box_other"), "Ground");
	EXPECT_EQ(getInt(scn, "FarBox_hits"), 0);
	scn.onEndRuntime();
}

TEST_F(SceneCollisionTest, DestroyBothInCallbackIsDeferredAndSkipsPendingSide) {
	const auto path = writeScript("destroy_both.lua", R"(
function on_collision(other_id)
    gamestate.set("calls", gamestate.get("calls", 0) + 1)
    scene.destroy_entity(entity_id)
    scene.destroy_entity(other_id)
    local garbage = {}
    for i = 1, 1000 do garbage[i] = tostring(i) end
end

function on_destroy()
    gamestate.set("destroyed", gamestate.get("destroyed", 0) + 1)
end
)");
	Scene scn;
	const auto ground = addBody(scn, "Ground", SceneBody::BodyType::Static, {0.f, 0.f, 0.f}, path).getUUID();
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, {0.f, 1.5f, 0.f}, path).getUUID();
	scn.onStartRuntime();
	for (int i = 0; i < 120; ++i) scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getInt(scn, "calls"), 1);
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
	EXPECT_FALSE(scn.findEntityByUUID(ground));
	EXPECT_FALSE(scn.findEntityByUUID(box));
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
}

TEST_F(SceneCollisionTest, ChildOfPendingEntityIsNotNotified) {
	const auto path = writeScript("record.lua", g_recordCollision);
	Scene scn;
	auto parent = scn.createEntity("Parent");
	auto ground = addBody(scn, "Ground", SceneBody::BodyType::Static, {0.f, 0.f, 0.f}, path);
	scn.setParent(ground, parent);
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, {0.f, 1.5f, 0.f}, "");
	scn.onStartRuntime();
	scn.destroyEntityDeferred(parent);
	EXPECT_TRUE(scn.isPendingDestructionInTree(ground));
	EXPECT_FALSE(scn.isPendingDestructionInTree(box));
	for (int i = 0; i < 60; ++i) {
		physics::PhysicCommand::frame(makeStep(16));
		scn.dispatchCollisionEvents();
	}
	EXPECT_LT(box.getComponent<component::Transform>().transform.translation().y(), 1.1f);
	EXPECT_EQ(getInt(scn, "Ground_hits"), 0);
	scn.onEndRuntime();
}

TEST_F(SceneCollisionTest, TriggerCallbacksReceiveOtherId) {
	const auto path = writeScript("trigger.lua", R"(
function on_trigger_enter(other_id)
    gamestate.set(entity.get_name(entity_id) .. "_enter", entity.get_name(other_id))
end

function on_trigger_exit(other_id)
    gamestate.set(entity.get_name(entity_id) .. "_exit", entity.get_name(other_id))
end

function on_triggered(other_id)
    gamestate.set("triggered_by", entity.get_name(other_id))
end
)");
	Scene scn;
	auto player = scn.createEntity("Hero");
	player.addComponent<component::Player>().primary = true;
	player.addComponent<component::LuaScript>().scriptPath = path;
	auto zone = scn.createEntity("Zone");
	zone.addComponent<component::Trigger>().trigger.type = SceneTrigger::TriggerType::LuaCallback;
	zone.addComponent<component::LuaScript>().scriptPath = path;
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getString(scn, "Zone_enter"), "Hero");
	EXPECT_EQ(getString(scn, "Hero_enter"), "Zone");
	EXPECT_EQ(getString(scn, "triggered_by"), "Hero");
	player.getComponent<component::Transform>().transform.translation() = {100.f, 0.f, 0.f};
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getString(scn, "Zone_exit"), "Hero");
	EXPECT_EQ(getString(scn, "Hero_exit"), "Zone");
	scn.onEndRuntime();
}
