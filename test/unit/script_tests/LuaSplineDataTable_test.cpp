/**
 * @file LuaSplineDataTable_test.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <app/EngineContext.h>
#include <core/Core.h>
#include <core/Log.h>
#include <data/table/DataTable.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/components.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

auto runScript(const shared<scene::Scene>& iScene, const std::string& iScript, const uint64_t iEntity,
			   const ScriptInstance& ioInstance) -> bool {
	const std::vector<uint8_t> data(iScript.begin(), iScript.end());
	ioInstance.setScene(iScene.get());
	if (!ioInstance.createFromBuffer(data, "spline_table_test", iEntity))
		return false;
	ioInstance.onCreate();
	return true;
}

}// namespace

TEST(LuaBindings, splineSampledInTheWorld) {
	core::Log::init(core::Log::Level::Off);
	const auto scn = mkShared<scene::Scene>();
	auto rail = scn->createEntity("Rail");
	rail.getComponent<scene::component::Transform>().transform.translation() = {0.f, 0.f, 5.f};
	auto& spline = rail.addComponent<scene::component::Spline>();
	spline.curve.setInterpolation(math::SplineInterpolation::Linear);
	spline.curve.setPoints({{.position = {0.f, 0.f, 0.f}}, {.position = {10.f, 0.f, 0.f}}});
	spline.lanes.push_back({.name = "right", .offset = 1.f, .height = 0.f});
	auto cart = scn->createEntity("Cart");
	auto& follower = cart.addComponent<scene::component::SplineFollower>();
	follower.spline = rail.getUUID();
	follower.startDistance = 3.f;
	const std::string script = std::string{"function on_create()\n"
										   "  cart = scene.find_entity('Cart')\n"
										   "  length = spline.get_length(entity_id)\n"
										   "  sx, sy, sz = spline.sample(entity_id, 4)\n"
										   "  lx, ly, lz = spline.sample(entity_id, 4, 'right')\n"
										   "  tx, ty, tz = spline.get_tangent(entity_id, 2)\n"
										   "  near = spline.get_nearest(entity_id, 6, 3, 5)\n"
										   "  count = spline.get_point_count(entity_id)\n"
										   "  moved = spline.set_point(entity_id, 2, 20, 0, 5)\n"
										   "  bad = spline.set_point(entity_id, 3, 0, 0, 0)\n"
										   "  px, py, pz = spline.get_point(entity_id, 2)\n"
										   "  none = spline.get_length(cart) == nil\n"
										   "  start = spline.get_distance(cart)\n"
										   "  spline.set_speed(cart, 4)\n"
										   "  speed = spline.get_speed(cart)\n"
										   "  spline.set_distance(cart, 7)\n"
										   "  distance = spline.get_distance(cart)\n"
										   "  laned = spline.set_lane(cart, 'right')\n"
										   "  noFollower = spline.set_speed(entity_id, 1)\n"
										   "  entity.set_spline_follower_enabled(cart, false)\n"
										   "  following = entity.is_spline_follower_enabled(cart)\n"
										   "end\n"};
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, static_cast<uint64_t>(rail.getUUID()), inst));
	EXPECT_NEAR(inst.getPropertyFloat("length").value_or(0), 10.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("sx").value_or(0), 4.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("sz").value_or(0), 5.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("lz").value_or(0), 6.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("tx").value_or(0), 1.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("near").value_or(0), 6.f, 1e-3f);
	EXPECT_EQ(inst.getPropertyInt("count").value_or(0), 2);
	EXPECT_TRUE(inst.getPropertyBool("moved").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("bad").value_or(true));
	EXPECT_NEAR(inst.getPropertyFloat("px").value_or(0), 20.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("pz").value_or(0), 5.f, 1e-3f);
	EXPECT_NEAR(spline.curve.getPoints()[1].position.z(), 0.f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("none").value_or(false));
	EXPECT_NEAR(inst.getPropertyFloat("start").value_or(0), 3.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("speed").value_or(0), 4.f, 1e-3f);
	EXPECT_NEAR(inst.getPropertyFloat("distance").value_or(0), 7.f, 1e-3f);
	EXPECT_TRUE(inst.getPropertyBool("laned").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("noFollower").value_or(true));
	EXPECT_FALSE(inst.getPropertyBool("following").value_or(true));
	EXPECT_EQ(follower.lane, "right");
	core::Log::invalidate();
}

TEST(LuaBindings, dataTableRowsAndReferences) {
	core::Log::init(core::Log::Level::Off);
	using data::table::ColumnType;
	const auto folder = std::filesystem::temp_directory_path() / "lua_tables";
	std::filesystem::create_directories(folder);
	const auto markets = (folder / "markets.owltable").generic_string();
	const auto goods = (folder / "goods.owltable").generic_string();
	data::table::DataTable market;
	ASSERT_TRUE(market.addColumn({.name = "name", .type = ColumnType::Text, .choices = {}, .table = {}}, 0));
	ASSERT_TRUE(market.addRow("harbour", 0));
	ASSERT_TRUE(market.setCell(0, 0, std::string{"The Harbour"}));
	ASSERT_TRUE(market.saveToFile(markets));
	data::table::DataTable table;
	ASSERT_TRUE(table.addColumn({.name = "price", .type = ColumnType::Float, .choices = {}, .table = {}}, 0));
	ASSERT_TRUE(table.addColumn({.name = "stock", .type = ColumnType::Integer, .choices = {}, .table = {}}, 1));
	ASSERT_TRUE(table.addColumn({.name = "legal", .type = ColumnType::Bool, .choices = {}, .table = {}}, 2));
	ASSERT_TRUE(table.addColumn({.name = "market", .type = ColumnType::Reference, .choices = {}, .table = markets}, 3));
	ASSERT_TRUE(table.addRow("wheat", 0));
	ASSERT_TRUE(table.addRow("iron", 1));
	ASSERT_TRUE(table.setCell(0, 0, 2.5));
	ASSERT_TRUE(table.setCell(1, 1, int64_t{12}));
	ASSERT_TRUE(table.setCell(1, 2, true));
	ASSERT_TRUE(table.setCell(1, 3, std::string{"harbour"}));
	ASSERT_TRUE(table.saveToFile(goods));

	const auto scn = mkShared<scene::Scene>();
	app::EngineContext context;
	scn->setEngineContext(&context);
	const std::string script = std::format("goods = '{}'\n"
										   "function on_create()\n"
										   "  price = datatable.get(goods, 'wheat', 'price')\n"
										   "  stock = datatable.get(goods, 'iron', 'stock')\n"
										   "  legal = datatable.get(goods, 'iron', 'legal')\n"
										   "  missing = datatable.get(goods, 'gold', 'price') == nil\n"
										   "  local row = datatable.get_row(goods, 'iron')\n"
										   "  rowStock = row.stock\n"
										   "  local keys = datatable.get_keys(goods)\n"
										   "  second = keys[2]\n"
										   "  local columns = datatable.get_columns(goods)\n"
										   "  lastColumn = columns[4]\n"
										   "  count = datatable.get_count(goods)\n"
										   "  has = datatable.has_row(goods, 'wheat')\n"
										   "  hasNot = datatable.has_row(goods, 'gold')\n"
										   "  local target, key = datatable.resolve(goods, 'iron', 'market')\n"
										   "  marketName = datatable.get(target, key, 'name')\n"
										   "  notRef = datatable.resolve(goods, 'iron', 'price') == nil\n"
										   "  found = datatable.find(goods, 'stock', 12)\n"
										   "  foundFlag = datatable.find(goods, 'legal', true)\n"
										   "  nothing = datatable.find(goods, 'stock', 99) == nil\n"
										   "  unread = datatable.get_count('nowhere.owltable') == nil\n"
										   "end\n",
										   goods);
	const ScriptInstance inst;
	ASSERT_TRUE(runScript(scn, script, 1, inst));
	EXPECT_NEAR(inst.getPropertyFloat("price").value_or(0), 2.5f, 1e-5f);
	EXPECT_EQ(inst.getPropertyInt("stock").value_or(0), 12);
	EXPECT_TRUE(inst.getPropertyBool("legal").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("missing").value_or(false));
	EXPECT_EQ(inst.getPropertyInt("rowStock").value_or(0), 12);
	EXPECT_EQ(inst.getPropertyString("second").value_or(""), "iron");
	EXPECT_EQ(inst.getPropertyString("lastColumn").value_or(""), "market");
	EXPECT_EQ(inst.getPropertyInt("count").value_or(0), 2);
	EXPECT_TRUE(inst.getPropertyBool("has").value_or(false));
	EXPECT_FALSE(inst.getPropertyBool("hasNot").value_or(true));
	EXPECT_EQ(inst.getPropertyString("marketName").value_or(""), "The Harbour");
	EXPECT_TRUE(inst.getPropertyBool("notRef").value_or(false));
	EXPECT_EQ(inst.getPropertyString("found").value_or(""), "iron");
	EXPECT_EQ(inst.getPropertyString("foundFlag").value_or(""), "iron");
	EXPECT_TRUE(inst.getPropertyBool("nothing").value_or(false));
	EXPECT_TRUE(inst.getPropertyBool("unread").value_or(false));
	EXPECT_EQ(context.getDataTables().getLoadCount(), 3u);
	scn->setEngineContext(nullptr);
	std::filesystem::remove_all(folder);
	core::Log::invalidate();
}
