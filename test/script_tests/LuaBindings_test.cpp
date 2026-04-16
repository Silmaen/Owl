/**
 * @file LuaBindings_test.cpp
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Transform.h>
#include <scene/component/components.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>

using namespace owl;
using namespace owl::script;

TEST(LuaBindings, transformGetSetPosition) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	const auto entity = scn->createEntity("TestEntity");
	auto& [transform] = entity.getComponent<scene::component::Transform>();
	transform.translation() = {1.0f, 2.0f, 3.0f};

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	// Use entity_id global (set by ScriptInstance::create*) to avoid uint64 literal precision issues.
	const std::string script = "px = 0\npy = 0\npz = 0\n"
							   "function on_create()\n"
							   "  px, py, pz = transform.get_position(entity_id)\n"
							   "  transform.set_position(entity_id, 10.0, 20.0, 30.0)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "transform_test", uuid));
	inst.onCreate();

	// Verify Lua read the original position.
	EXPECT_NEAR(inst.getPropertyFloat("px").value_or(0), 1.0f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("py").value_or(0), 2.0f, 0.01f);
	EXPECT_NEAR(inst.getPropertyFloat("pz").value_or(0), 3.0f, 0.01f);

	// Verify Lua wrote back to the component.
	EXPECT_NEAR(transform.translation().x(), 10.0f, 0.01f);
	EXPECT_NEAR(transform.translation().y(), 20.0f, 0.01f);
	EXPECT_NEAR(transform.translation().z(), 30.0f, 0.01f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneFindEntity) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("MyTarget");
	const auto expectedUuid = static_cast<uint64_t>(entity.getUUID());

	ScriptEngine::init(scn.get());

	const std::string script = "found_id = 0\n"
							   "function on_create()\n"
							   "  found_id = scene.find_entity('MyTarget')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	const ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "find_test", 1));
	inst.onCreate();

	const auto foundId = inst.getPropertyInt("found_id");
	ASSERT_TRUE(foundId.has_value());
	EXPECT_EQ(static_cast<uint64_t>(foundId.value()), expectedUuid);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, sceneFindEntityNotFound) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "found_id = -1\n"
							   "function on_create()\n"
							   "  found_id = scene.find_entity('NonExistent')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "notfound_test", 1));
	inst.onCreate();

	const auto foundId = inst.getPropertyInt("found_id");
	ASSERT_TRUE(foundId.has_value());
	EXPECT_EQ(foundId.value(), 0);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, entityHasComponent) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("CompTest");
	// Entity has Transform by default, no PhysicBody.

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const std::string script = "has_transform = false\n"
							   "has_physic = false\n"
							   "function on_create()\n"
							   "  has_transform = entity.has_component(entity_id, 'Transform')\n"
							   "  has_physic = entity.has_component(entity_id, 'PhysicBody')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "comp_test", uuid));
	inst.onCreate();

	EXPECT_TRUE(inst.getPropertyBool("has_transform").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("has_physic").value_or(true));

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, entityGetName) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("NamedEntity");

	ScriptEngine::init(scn.get());

	const auto uuid = static_cast<uint64_t>(entity.getUUID());
	const std::string script = "ent_name = ''\n"
							   "function on_create()\n"
							   "  ent_name = entity.get_name(entity_id)\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "name_test", uuid));
	inst.onCreate();

	EXPECT_EQ(inst.getPropertyString("ent_name").value_or(""), "NamedEntity");

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, logDoesNotCrash) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "function on_create()\n"
							   "  log.trace('trace msg')\n"
							   "  log.info('info msg')\n"
							   "  log.warn('warn msg')\n"
							   "  log.error('error msg')\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "log_test", 1));
	inst.onCreate();// Should not crash.

	ScriptEngine::shutdown();
	core::Log::invalidate();
}

TEST(LuaBindings, timeDelta) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	ScriptEngine::init(scn.get());

	const std::string script = "dt_val = 0\n"
							   "function on_update(dt)\n"
							   "  dt_val = time.delta()\n"
							   "end\n";
	const std::vector<uint8_t> data(script.begin(), script.end());

	ScriptInstance inst;
	ASSERT_TRUE(inst.createFromBuffer(data, "time_test", 1));
	inst.onUpdate(0.033f);

	const auto dt = inst.getPropertyFloat("dt_val");
	ASSERT_TRUE(dt.has_value());
	EXPECT_NEAR(dt.value(), 0.033f, 0.001f);

	ScriptEngine::shutdown();
	core::Log::invalidate();
}
