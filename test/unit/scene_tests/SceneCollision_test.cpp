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
		OWL_REQUIRE_MODULE(PHYSICS);
		OWL_REQUIRE_MODULE(SCRIPT);
	}

	void TearDown() override {
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
		physics::PhysicCommand::frame(scn, makeStep(16));
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

TEST_F(SceneCollisionTest, BrokenJointCallsOnJointBreakOnBothSides) {
	const auto path = writeScript("break.lua", R"(
function on_joint_break(other_id)
    local other = other_id and entity.get_name(other_id) or "world"
    gamestate.set(entity.get_name(entity_id) .. "_broke", other)
    gamestate.set("mass_" .. entity.get_name(entity_id), physics.get_mass(entity_id))
end
)");
	Scene scn;
	const auto anchor = addBody(scn, "Anchor", SceneBody::BodyType::Static, {0.f, 5.f, 0.f}, path);
	auto weight = addBody(scn, "Weight", SceneBody::BodyType::Dynamic, {0.f, 3.f, 0.f}, path);
	auto& [joint] = weight.addComponent<component::PhysicJoint>();
	joint.type = SceneJoint::Type::Distance;
	joint.connected = anchor.getUUID();
	joint.breakForce = 5.f;
	scn.onStartRuntime();
	for (int i = 0; i < 20; ++i) scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getString(scn, "Weight_broke"), "Anchor");
	EXPECT_EQ(getString(scn, "Anchor_broke"), "Weight");
	EXPECT_EQ(getInt(scn, "mass_Weight"), 1);
	scn.onEndRuntime();
}

TEST_F(SceneCollisionTest, LuaPushesAndAssemblesBodies) {
	const auto path = writeScript("assembly.lua", R"(
function on_tick(dt)
    local me = entity.get_name(entity_id)
    if me == "Hull" then
        physics.add_force(entity_id, 0, 10)
        physics.add_force_at(entity_id, 0, 1, 1, 0)
        physics.add_torque(entity_id, 0.5)
        gamestate.set("root_is_nil", physics.get_assembly(entity_id) == nil)
        local x, y = physics.get_center_of_mass(entity_id)
        gamestate.set("com_x", math.floor(x + 0.5))
    elseif me == "Wing" then
        local root = physics.get_assembly(entity_id)
        if root then
            gamestate.set("wing_root", entity.get_name(root))
        end
        gamestate.set("wing_mass", physics.get_mass(entity_id))
        if gamestate.get("ticks", 0) == 5 then
            physics.detach(entity_id)
        elseif gamestate.get("ticks", 0) == 8 then
            gamestate.set("attached", physics.attach(entity_id))
        end
        gamestate.set("ticks", gamestate.get("ticks", 0) + 1)
    elseif me == "Probe" then
        physics.add_force_at_3d(entity_id, 0, 1, 0, 0.5, 0, 0)
        physics.add_torque_3d(entity_id, 0, 0, 1)
        local x, y, z = physics.get_center_of_mass(entity_id)
        gamestate.set("probe_z", math.floor(z + 0.5))
    end
end
)");
	Scene scn;
	scn.getPhysicsSettings().gravity = {0.f, 0.f, 0.f};
	const auto hull = addBody(scn, "Hull", SceneBody::BodyType::Dynamic, {0.f, 0.f, 0.f}, path);
	auto wing = addBody(scn, "Wing", SceneBody::BodyType::Dynamic, {2.f, 0.f, 0.f}, path);
	wing.getComponent<component::PhysicBody>().body.part = true;
	scn.setParent(wing, hull);
	auto probe = addBody(scn, "Probe", SceneBody::BodyType::Dynamic, {0.f, 0.f, 7.f}, path);
	probe.getComponent<component::PhysicBody>().body.dimension = SceneBody::Dimension::Space;
	scn.onStartRuntime();
	for (int i = 0; i < 12; ++i) scn.onUpdateRuntime(makeStep(17), false);
	EXPECT_EQ(getString(scn, "wing_root"), "Hull");
	EXPECT_EQ(getInt(scn, "wing_mass"), 2);
	EXPECT_EQ(getInt(scn, "com_x"), 1);
	EXPECT_EQ(getInt(scn, "probe_z"), 7);
	EXPECT_TRUE(std::get<bool>(scn.getGameState().get("attached", false)));
	EXPECT_TRUE(std::get<bool>(scn.getGameState().get("root_is_nil", false)));
	EXPECT_GT(physics::PhysicCommand::getVelocity(hull).y(), 0.f);
	scn.onEndRuntime();
}
