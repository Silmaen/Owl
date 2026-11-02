/**
 * @file LuaWalk_test.cpp
 * @author Silmaen
 * @date 31/10/2026
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

namespace {

auto runScript(const shared<scene::Scene>& iScene, const std::string& iScript, const uint64_t iEntity,
			   const ScriptInstance& ioInstance) -> bool {
	const std::vector<uint8_t> data(iScript.begin(), iScript.end());
	ioInstance.setScene(iScene.get());
	if (!ioInstance.createFromBuffer(data, "walk_test", iEntity))
		return false;
	ioInstance.onCreate();
	return true;
}

}// namespace

TEST(LuaBindings, walkAreaQueriesAndAgents) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto floor = scn->createEntity("Floor");
	floor.getComponent<scene::component::Transform>().transform.translation() = {10.f, 0.f, 0.f};
	auto& area = floor.addComponent<scene::component::WalkArea>();
	area.polygons.push_back({.outline = {{0.f, 0.f}, {10.f, 0.f}, {10.f, 10.f}, {0.f, 10.f}},
							 .holes = {{{4.f, 2.f}, {6.f, 2.f}, {6.f, 8.f}, {4.f, 8.f}}}});
	area.depthScale = true;
	area.farY = 10.f;
	area.farScale = 0.5f;
	auto other = scn->createEntity("Other");
	other.addComponent<scene::component::WalkArea>().polygons.push_back(
			{.outline = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}}, .holes = {}});
	auto hero = scn->createEntity("Hero");
	hero.getComponent<scene::component::Transform>().transform.translation() = {12.f, 5.f, 0.f};
	auto& agent = hero.addComponent<scene::component::WalkAgent>();
	agent.area = floor.getUUID();
	const std::string script = std::string{"function on_create()\n"
										   "  hero = scene.find_entity('Hero')\n"
										   "  other = scene.find_entity('Other')\n"
										   "  local points = walk.find_path(entity_id, 12, 5, 18, 5)\n"
										   "  corners = #points // 2\n"
										   "  last_x = points[#points - 1]\n"
										   "  inside = walk.is_inside(entity_id, 12, 1)\n"
										   "  in_hole = walk.is_inside(entity_id, 15, 5)\n"
										   "  cx, cy = walk.get_closest_point(entity_id, 25, 5)\n"
										   "  scale = walk.get_scale_at(entity_id, 12, 5)\n"
										   "  no_area = walk.find_path(hero, 0, 0, 1, 1) == nil\n"
										   "  went = walk.go_to(hero, 18, 5)\n"
										   "  moving = walk.is_moving(hero)\n"
										   "  tx, ty = walk.get_target(hero)\n"
										   "  walk.set_speed(hero, 5)\n"
										   "  walk.stop(hero)\n"
										   "  stopped = not walk.is_moving(hero)\n"
										   "  no_target = walk.get_target(hero) == nil\n"
										   "  moved = walk.set_area(hero, other)\n"
										   "  refused = walk.set_area(hero, hero)\n"
										   "  not_agent = walk.go_to(entity_id, 1, 1)\n"
										   "end\n"};
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, static_cast<uint64_t>(floor.getUUID()), inst));
	EXPECT_EQ(inst.getPropertyInt("corners").value_or(0), 4);
	EXPECT_NEAR(inst.getPropertyFloat("last_x").value_or(0), 18.f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("inside").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("in_hole").value_or(true));
	EXPECT_NEAR(inst.getPropertyFloat("cx").value_or(0), 20.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("cy").value_or(0), 5.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("scale").value_or(0), 0.75f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("no_area").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("went").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("moving").value_or(false));
	EXPECT_NEAR(inst.getPropertyFloat("tx").value_or(0), 18.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("ty").value_or(0), 5.f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("stopped").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("no_target").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("moved").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("refused").value_or(true));
	EXPECT_FALSE(inst.getPropertyBool("not_agent").value_or(true));
	EXPECT_FLOAT_EQ(agent.speed, 5.f);
	EXPECT_EQ(agent.area, other.getUUID());
	core::Log::invalidate();
}

TEST(LuaBindings, theGameHidesTheSystemCursor) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto host = scn->createEntity("Host");
	const std::string script = std::string{"function on_create()\n"
										   "  shown_first = input.is_cursor_visible()\n"
										   "  input.set_cursor_visible(false)\n"
										   "  shown_after = input.is_cursor_visible()\n"
										   "end\n"};
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, static_cast<uint64_t>(host.getUUID()), inst));
	EXPECT_TRUE(inst.getPropertyBool("shown_first").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("shown_after").value_or(true));
	EXPECT_TRUE(scn->wantsCursorHidden());
	scn->onStartRuntime();
	scn->onEndRuntime();
	EXPECT_FALSE(scn->wantsCursorHidden());
	core::Log::invalidate();
}
