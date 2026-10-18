/**
 * @file SceneRuntimeLifecycle_test.cpp
 * @author Silmaen
 * @date 07/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <physics/PhysicCommand.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/SceneBody.h>
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

class SceneRuntimeLifecycleTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_scene_runtime_lifecycle_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
		OWL_REQUIRE_MODULE(PHYSICS);
		OWL_REQUIRE_MODULE(SCRIPT);
	}

	void TearDown() override {
		std::filesystem::remove_all(m_dir);
		core::Log::invalidate();
	}

	[[nodiscard]] auto countingScript() const -> std::string {
		const auto path = m_dir / "count_destroy.lua";
		std::ofstream file(path);
		file << R"(
function on_destroy()
    gamestate.set("destroyed", gamestate.get("destroyed", 0) + 1)
end
)";
		return path.string();
	}

	std::filesystem::path m_dir;
};

auto makeStep(const int iMs) -> core::Timestep {
	core::Timestep ts;
	ts.forceUpdate(std::chrono::milliseconds(iMs));
	return ts;
}

void run(Scene& ioScene, const int iFrames) {
	for (int i = 0; i < iFrames; ++i) ioScene.onUpdateRuntime(makeStep(16), false);
}

auto getInt(const Scene& iScene, const std::string& iKey) -> int64_t {
	const auto value = iScene.getGameState().get(iKey, int64_t{0});
	if (const auto* asInt = std::get_if<int64_t>(&value); asInt != nullptr)
		return *asInt;
	if (const auto* asFloat = std::get_if<float>(&value); asFloat != nullptr)
		return static_cast<int64_t>(*asFloat);
	return -1;
}

auto addBody(Scene& ioScene, const std::string& iName, const SceneBody::BodyType iType, const float iY) -> Entity {
	auto entity = ioScene.createEntity(iName);
	entity.getComponent<component::Transform>().transform.translation().y() = iY;
	entity.addComponent<component::PhysicBody>().body.type = iType;
	return entity;
}

auto heightOf(const Entity& iEntity) -> float {
	return iEntity.getComponent<component::Transform>().transform.translation().y();
}

}// namespace

TEST_F(SceneRuntimeLifecycleTest, GroundStopsTheFallingBox) {
	Scene scn;
	addBody(scn, "Ground", SceneBody::BodyType::Static, 0.f);
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, 3.f);
	scn.onStartRuntime();
	run(scn, 120);
	EXPECT_GT(heightOf(box), 0.5f);
	scn.onEndRuntime();
}

TEST_F(SceneRuntimeLifecycleTest, RemovedPhysicBodyLeavesNoCollider) {
	Scene scn;
	const auto ground = addBody(scn, "Ground", SceneBody::BodyType::Static, 0.f);
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, 3.f);
	scn.onStartRuntime();
	ground.removeComponent<component::PhysicBody>();
	run(scn, 120);
	EXPECT_LT(heightOf(box), -1.f);
	scn.onEndRuntime();
}

TEST_F(SceneRuntimeLifecycleTest, ImmediatelyDestroyedEntityLeavesNoCollider) {
	Scene scn;
	auto ground = addBody(scn, "Ground", SceneBody::BodyType::Static, 0.f);
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, 3.f);
	scn.onStartRuntime();
	scn.destroyEntity(ground);
	run(scn, 120);
	EXPECT_LT(heightOf(box), -1.f);
	scn.onEndRuntime();
}

TEST_F(SceneRuntimeLifecycleTest, PhysicBodyAddedInPlayGetsItsBody) {
	Scene scn;
	auto box = scn.createEntity("Box");
	box.getComponent<component::Transform>().transform.translation().y() = 3.f;
	scn.onStartRuntime();
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	// The body is created at the next physics frame; until then the calls find no body and do nothing.
	physics::PhysicCommand::impulse(box, {0.f, 5.f});
	EXPECT_EQ(physics::PhysicCommand::getVelocity(box), math::vec2f(0.f, 0.f));
	run(scn, 30);
	EXPECT_NE(box.getComponent<component::PhysicBody>().body.bodyId, 0u);
	EXPECT_LT(heightOf(box), 2.f);
	scn.onEndRuntime();
}

TEST_F(SceneRuntimeLifecycleTest, DuplicateOwnsItsBody) {
	Scene scn;
	const auto box = addBody(scn, "Box", SceneBody::BodyType::Dynamic, 3.f);
	scn.onStartRuntime();
	run(scn, 1);
	auto copy = scn.duplicateSubtree(box);
	EXPECT_EQ(copy.getComponent<component::PhysicBody>().body.bodyId, 0u);
	run(scn, 1);
	const auto copyBody = copy.getComponent<component::PhysicBody>().body.bodyId;
	EXPECT_NE(copyBody, 0u);
	EXPECT_NE(copyBody, box.getComponent<component::PhysicBody>().body.bodyId);
	scn.destroyEntity(copy);
	const float before = heightOf(box);
	run(scn, 30);
	EXPECT_LT(heightOf(box), before);
	scn.onEndRuntime();
}

TEST_F(SceneRuntimeLifecycleTest, ImmediateDestroyCallsOnDestroy) {
	Scene scn;
	auto coin = scn.createEntity("Coin");
	coin.addComponent<component::LuaScript>().scriptPath = countingScript();
	scn.onStartRuntime();
	scn.destroyEntity(coin);
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
}

TEST_F(SceneRuntimeLifecycleTest, DestroyWithChildrenCallsOnDestroyOnTheSubtree) {
	Scene scn;
	const auto script = countingScript();
	auto parent = scn.createEntity("Parent");
	parent.addComponent<component::LuaScript>().scriptPath = script;
	auto child = scn.createEntity("Child");
	child.addComponent<component::LuaScript>().scriptPath = script;
	scn.setParent(child, parent);
	scn.onStartRuntime();
	scn.destroyEntityWithChildren(parent);
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
}

TEST_F(SceneRuntimeLifecycleTest, RemovedLuaScriptCallsOnDestroy) {
	Scene scn;
	auto coin = scn.createEntity("Coin");
	coin.addComponent<component::LuaScript>().scriptPath = countingScript();
	scn.onStartRuntime();
	coin.removeComponent<component::LuaScript>();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
}

TEST_F(SceneRuntimeLifecycleTest, EditModeRemovalTriggersNothing) {
	Scene scn;
	auto coin = scn.createEntity("Coin");
	coin.addComponent<component::LuaScript>().scriptPath = countingScript();
	scn.onStartRuntime();
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
	coin.addComponent<component::PhysicBody>();
	coin.removeComponent<component::PhysicBody>();
	coin.removeComponent<component::LuaScript>();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
	EXPECT_FALSE(physics::PhysicCommand::isInitialized(scn));
}

TEST_F(SceneRuntimeLifecycleTest, DestroyBodyIgnoresAnotherScene) {
	Scene running;
	const auto box = addBody(running, "Box", SceneBody::BodyType::Dynamic, 3.f);
	Scene other;
	const auto stranger = addBody(other, "Stranger", SceneBody::BodyType::Dynamic, 0.f);
	running.onStartRuntime();
	run(running, 1);
	const auto boxBody = box.getComponent<component::PhysicBody>().body.bodyId;
	stranger.getComponent<component::PhysicBody>().body.bodyId = boxBody;
	physics::PhysicCommand::destroyBody(stranger);
	EXPECT_EQ(box.getComponent<component::PhysicBody>().body.bodyId, boxBody);
	const float before = heightOf(box);
	run(running, 30);
	EXPECT_LT(heightOf(box), before);
	running.onEndRuntime();
}
