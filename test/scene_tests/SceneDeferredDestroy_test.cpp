/**
 * @file SceneDeferredDestroy_test.cpp
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

class SceneDeferredDestroyTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_dir = std::filesystem::temp_directory_path() / "owl_scene_deferred_destroy_test";
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
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

constexpr auto g_countDestroy = R"(
function on_destroy()
    gamestate.set("destroyed", gamestate.get("destroyed", 0) + 1)
end
)";

}// namespace

TEST_F(SceneDeferredDestroyTest, SelfDestroyInOnUpdateKeepsVmAlive) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto path = writeScript("self_destroy.lua", std::string(R"(
function on_update(dt)
    scene.destroy_entity(entity_id)
    local garbage = {}
    for i = 1, 1000 do garbage[i] = tostring(i) end
    gamestate.set("after_destroy", #garbage)
end
)") + g_countDestroy);
	Scene scn;
	auto coin = scn.createEntity("Coin");
	coin.addComponent<component::LuaScript>().scriptPath = path;
	const auto coinUuid = coin.getUUID();
	auto other = scn.createEntity("Other");
	other.getComponent<component::Transform>().transform.translation() = {3.f, 4.f, 0.f};
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getInt(scn, "after_destroy"), 1000);
	EXPECT_FALSE(scn.findEntityByUUID(coinUuid));
	ASSERT_TRUE(other);
	EXPECT_EQ(other.getName(), "Other");
	EXPECT_FLOAT_EQ(other.getComponent<component::Transform>().transform.translation().x(), 3.f);
	scn.onUpdateRuntime(makeStep(16), false);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
}

TEST_F(SceneDeferredDestroyTest, CoinTriggerDoesNotCorruptOtherTrigger) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto path = writeScript("coin.lua", std::string(R"(
function on_coin_collected()
    gamestate.set("score", gamestate.get("score", 0) + 10)
    scene.destroy_entity(entity_id)
end
)") + g_countDestroy);
	Scene scn;
	scn.createEntity("Player").addComponent<component::Player>().primary = true;
	auto coin = scn.createEntity("Coin");
	auto& coinTrigger = coin.addComponent<component::Trigger>().trigger;
	coinTrigger.type = SceneTrigger::TriggerType::LuaCallback;
	coinTrigger.callbackName = "on_coin_collected";
	coin.addComponent<component::LuaScript>().scriptPath = path;
	const auto coinUuid = coin.getUUID();
	auto other = scn.createEntity("OtherTrigger");
	other.getComponent<component::Transform>().transform.translation() = {100.f, 0.f, 0.f};
	other.addComponent<component::Trigger>().trigger.type = SceneTrigger::TriggerType::LuaCallback;
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(scn.findEntityByUUID(coinUuid));
	EXPECT_FALSE(other.getComponent<component::Trigger>().trigger.wasOverlapping());
	EXPECT_FLOAT_EQ(other.getComponent<component::Transform>().transform.translation().x(), 100.f);
	EXPECT_EQ(getInt(scn, "score"), 10);
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_EQ(getInt(scn, "score"), 10);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 1);
}

TEST_F(SceneDeferredDestroyTest, DestroyIsDeferredUntilFlush) {
	Scene scn;
	auto a = scn.createEntity("A");
	const auto uuid = a.getUUID();
	scn.destroyEntityDeferred(a);
	scn.destroyEntityDeferred(a);
	EXPECT_TRUE(scn.findEntityByUUID(uuid));
	EXPECT_TRUE(scn.isPendingDestruction(a));
	scn.flushPendingDestructions();
	EXPECT_FALSE(scn.findEntityByUUID(uuid));
	EXPECT_FALSE(scn.isPendingDestruction(a));
	scn.flushPendingDestructions();
}

TEST_F(SceneDeferredDestroyTest, RuntimeDestroyCascadesToChildren) {
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto path = writeScript("parent.lua", std::string(R"(
function on_update(dt)
    scene.destroy_entity(entity_id)
end
)") + g_countDestroy);
	const auto childPath = writeScript("child.lua", g_countDestroy);
	Scene scn;
	auto parent = scn.createEntity("Parent");
	parent.addComponent<component::LuaScript>().scriptPath = path;
	auto child = scn.createEntity("Child");
	child.addComponent<component::LuaScript>().scriptPath = childPath;
	scn.setParent(child, parent);
	const auto parentUuid = parent.getUUID();
	const auto childUuid = child.getUUID();
	scn.onStartRuntime();
	scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_FALSE(scn.findEntityByUUID(parentUuid));
	EXPECT_FALSE(scn.findEntityByUUID(childUuid));
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
	scn.onEndRuntime();
	EXPECT_EQ(getInt(scn, "destroyed"), 2);
}

TEST_F(SceneDeferredDestroyTest, RuntimeDestroyRemovesPhysicsBody) {
	OWL_REQUIRE_MODULE(PHYSICS);
	OWL_REQUIRE_MODULE(SCRIPT);
	const auto path = writeScript("platform.lua", R"(
function on_update(dt)
    scene.destroy_entity(entity_id)
end
)");
	Scene scn;
	auto platform = scn.createEntity("Platform");
	platform.getComponent<component::Transform>().transform.scale() = {10.f, 1.f, 1.f};
	platform.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Static;
	platform.addComponent<component::LuaScript>().scriptPath = path;
	auto box = scn.createEntity("Box");
	box.getComponent<component::Transform>().transform.translation() = {0.f, 1.5f, 0.f};
	box.addComponent<component::PhysicBody>().body.type = SceneBody::BodyType::Dynamic;
	scn.onStartRuntime();
	for (int i = 0; i < 90; ++i) scn.onUpdateRuntime(makeStep(16), false);
	EXPECT_LT(box.getComponent<component::Transform>().transform.translation().y(), -1.f);
	scn.onEndRuntime();
}
