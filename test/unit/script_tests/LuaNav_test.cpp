/**
 * @file LuaNav_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Core.h>
#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

TEST(LuaBindings, navMeshQueriesBakeAndAgents) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto floor = scn->createEntity("Floor");
	floor.getComponent<scene::component::Transform>().transform.translation() = {0.f, -0.1f, 0.f};
	auto& body = floor.addComponent<scene::component::PhysicBody>().body;
	body.type = scene::SceneBody::BodyType::Static;
	body.dimension = scene::SceneBody::Dimension::Space;
	body.colliderSize = {10.f, 0.2f, 10.f};
	auto volume = scn->createEntity("Navigation");
	volume.addComponent<scene::component::NavMeshVolume>().size = {12.f, 4.f, 12.f};
	auto hero = scn->createEntity("Hero");
	hero.getComponent<scene::component::Transform>().transform.translation() = {-3.f, 0.f, 0.f};
	auto& agent = hero.addComponent<scene::component::NavAgent>();
	const std::string script = std::string{"function on_create()\n"
										   "  hero = scene.find_entity('Hero')\n"
										   "  not_baked = nav.find_path(entity_id, -3, 0, 0, 3, 0, 0) == nil\n"
										   "  baked = nav.bake(entity_id)\n"
										   "  local points = nav.find_path(entity_id, -3, 0, 0, 3, 0, 2)\n"
										   "  corners = #points // 3\n"
										   "  last_z = points[#points]\n"
										   "  cx, cy, cz = nav.get_closest_point(entity_id, 6, 0.5, 0)\n"
										   "  far = nav.get_closest_point(entity_id, 0, 30, 0) == nil\n"
										   "  no_volume = not nav.bake(hero)\n"
										   "  went = nav.go_to(hero, 3, 0, 2)\n"
										   "  moving = nav.is_moving(hero)\n"
										   "  tx, ty, tz = nav.get_target(hero)\n"
										   "  nav.set_speed(hero, 5)\n"
										   "  nav.stop(hero)\n"
										   "  stopped = not nav.is_moving(hero)\n"
										   "  no_target = nav.get_target(hero) == nil\n"
										   "  not_agent = nav.go_to(entity_id, 1, 0, 1)\n"
										   "end\n"};
	const std::vector<uint8_t> data(script.begin(), script.end());
	const ScriptInstance inst;
	inst.setScene(scn.get());
	ASSERT_TRUE(inst.createFromBuffer(data, "nav_test", static_cast<uint64_t>(volume.getUUID())));
	inst.onCreate();
	EXPECT_TRUE(inst.getPropertyBool("not_baked").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("baked").value_or(false));
	EXPECT_EQ(inst.getPropertyInt("corners").value_or(0), 2);
	EXPECT_NEAR(inst.getPropertyFloat("last_z").value_or(0), 2.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("cx").value_or(0), 4.6f, 0.21f);
	EXPECT_NEAR(inst.getPropertyFloat("cy").value_or(1), 0.f, 0.11f);
	EXPECT_TRUE(inst.getPropertyBool("far").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("no_volume").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("went").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("moving").value_or(false));
	EXPECT_NEAR(inst.getPropertyFloat("tx").value_or(0), 3.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("tz").value_or(0), 2.f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("stopped").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("no_target").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("not_agent").value_or(true));
	EXPECT_FLOAT_EQ(agent.speed, 5.f);
	core::Log::invalidate();
}
