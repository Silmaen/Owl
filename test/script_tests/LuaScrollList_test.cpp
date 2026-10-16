/**
 * @file LuaScrollList_test.cpp
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/UiInputSystem.h>
#include <scene/component/components.h>
#include <scene/ui/HudLayout.h>
#include <script/ScriptInstance.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

constexpr math::vec2ui g_viewport{800u, 600u};

// A scene with a canvas running `iScript`, a 200 x 100 list at (100, 100) and a scroll view at (400, 100).
struct ListFixture {
	explicit ListFixture(const std::string& iScript) {
		core::Log::init(core::Log::Level::Off);
		world->status = scene::Scene::Status::Playing;
		canvas = world->createEntity("Canvas");
		canvas.addComponent<scene::component::Canvas>().scaleMode = scene::component::Canvas::ScaleMode::ConstantPixels;
		list = makeWidget("List", {100.f, 100.f});
		auto& data = list.addComponent<scene::component::UiList>();
		data.rowHeight = 20.f;
		data.rowSpacing = 4.f;
		data.onBindCallback = "on_bind";
		data.onSelectCallback = "on_select";
		view = makeWidget("View", {400.f, 100.f});
		view.addComponent<scene::component::UiScrollView>().contentSize = {0.f, 400.f};
		auto& luaScript = canvas.addComponent<scene::component::LuaScript>();
		luaScript.instance = mkUniq<ScriptInstance>();
		luaScript.instance->setScene(world.get());
		const std::vector<uint8_t> data2(iScript.begin(), iScript.end());
		EXPECT_TRUE(luaScript.instance->createFromBuffer(data2, "list_test", static_cast<uint64_t>(canvas.getUUID())));
	}

	~ListFixture() { core::Log::invalidate(); }

	ListFixture(const ListFixture&) = delete;
	ListFixture(ListFixture&&) = delete;
	auto operator=(const ListFixture&) -> ListFixture& = delete;
	auto operator=(ListFixture&&) -> ListFixture& = delete;

	[[nodiscard]] auto script() const -> const ScriptInstance& {
		return *canvas.getComponent<scene::component::LuaScript>().instance;
	}

	[[nodiscard]] auto integer(const char* iName) const -> int64_t {
		return script().getPropertyInt(iName).value_or(-1);
	}

	void update(const math::vec2& iPoint = {700.f, 500.f}, const bool iPressed = false) const {
		scene::UiInputSystem::update(world.get(), g_viewport, iPoint, iPressed);
	}

	auto makeWidget(const std::string& iName, const math::vec2& iPosition) -> scene::Entity {
		auto widget = world->createEntity(iName);
		auto& rect = widget.addComponent<scene::component::UiRect>();
		rect.setAnchor(scene::component::UiRect::Anchor::TopLeft);
		rect.pivot = {0.f, 0.f};
		rect.size = {200.f, 100.f};
		rect.anchorOffset = iPosition;
		world->setParent(widget, canvas);
		return widget;
	}

	shared<scene::Scene> world = mkShared<scene::Scene>();
	scene::Entity canvas;
	scene::Entity list;
	scene::Entity view;
};

constexpr const char* g_bindScript =
		"binds = 0\n"
		"last = 0\n"
		"function on_create()\n"
		"  ui.list_set_count(ui.find('List'), 1000)\n"
		"end\n"
		"function on_bind()\n"
		"  binds = binds + 1\n"
		"  last = _row_index\n"
		"  ui.list_set_row(_list_entity, _row_index, 'Row ' .. _row_index, 'd' .. _row_index)\n"
		"end\n"
		"function on_select()\n"
		"  picked = _selected_index\n"
		"  who = _list_entity\n"
		"end\n";

}// namespace

TEST(LuaScrollList, aScriptSetsTheCountAndOnlyTheRowsInViewAreBound) {
	const ListFixture fixture{g_bindScript};
	fixture.script().onCreate();
	EXPECT_EQ(fixture.list.getComponent<scene::component::UiList>().itemCount, 1000u);
	fixture.update();
	// Five rows are in view (24 pixels apart, 100 pixels high).
	EXPECT_EQ(fixture.integer("binds"), 5);
	EXPECT_EQ(fixture.integer("last"), 5);
	const auto& data = fixture.list.getComponent<scene::component::UiList>();
	ASSERT_NE(data.findRow(0), nullptr);
	EXPECT_EQ(data.findRow(0)->text, "Row 1");
	EXPECT_EQ(data.findRow(4)->detail, "d5");
	EXPECT_EQ(data.findRow(5), nullptr);
	// A row is asked for once.
	fixture.update();
	fixture.update();
	EXPECT_EQ(fixture.integer("binds"), 5);
}

TEST(LuaScrollList, scrollingBindsTheRowsThatComeIntoView) {
	const ListFixture fixture{g_bindScript};
	fixture.script().onCreate();
	fixture.update();
	EXPECT_EQ(fixture.integer("binds"), 5);
	fixture.list.getComponent<scene::component::UiList>().scrollOffset = 2400.f;
	fixture.update();
	// Rows 101 to 105 (a row at the edge may come in view: at most six).
	EXPECT_GE(fixture.integer("binds"), 10);
	EXPECT_LE(fixture.integer("binds"), 11);
	EXPECT_NE(fixture.list.getComponent<scene::component::UiList>().findRow(100), nullptr);
	EXPECT_EQ(fixture.list.getComponent<scene::component::UiList>().findRow(50), nullptr);
}

TEST(LuaScrollList, invalidatingTheRowsBindsTheOnesInViewAgain) {
	const ListFixture fixture{std::string{g_bindScript} +
							  "function refresh() ui.list_invalidate(ui.find('List')) end\n"};
	fixture.script().onCreate();
	fixture.update();
	EXPECT_EQ(fixture.integer("binds"), 5);
	EXPECT_TRUE(fixture.script().callFunction("refresh"));
	EXPECT_EQ(fixture.list.getComponent<scene::component::UiList>().findRow(0), nullptr);
	fixture.update();
	EXPECT_EQ(fixture.integer("binds"), 10);
}

TEST(LuaScrollList, aClickRunsTheSelectCallbackWithTheRow) {
	const ListFixture fixture{g_bindScript};
	fixture.script().onCreate();
	fixture.update();
	// Row 3 (0-based 2) spans 148 to 168.
	fixture.update({150.f, 160.f}, false);
	fixture.update({150.f, 160.f}, true);
	EXPECT_EQ(fixture.integer("picked"), 3);
	EXPECT_EQ(fixture.integer("who"), static_cast<int64_t>(fixture.list.getUUID()));
	EXPECT_EQ(fixture.list.getComponent<scene::component::UiList>().selectedIndex, 2);
}

TEST(LuaScrollList, theListIsDrivenFromAScript) {
	const ListFixture fixture{"count = 0\n"
							  "selected = -1\n"
							  "function run()\n"
							  "  local list = ui.find('List')\n"
							  "  ui.list_set_count(list, 30)\n"
							  "  count = ui.list_get_count(list)\n"
							  "  ui.list_set_row(list, 2, 'Second', 'right', '')\n"
							  "  ui.list_set_row(list, 99, 'Nowhere')\n"
							  "  ui.list_set_selected(list, 4)\n"
							  "  selected = ui.list_get_selected(list)\n"
							  "  ui.scroll_set(list, 0, 48)\n"
							  "  sx, sy = ui.scroll_get(list)\n"
							  "  none = ui.list_get_visible(list)\n"
							  "end\n"};
	EXPECT_TRUE(fixture.script().callFunction("run"));
	EXPECT_EQ(fixture.integer("count"), 30);
	EXPECT_EQ(fixture.integer("selected"), 4);
	EXPECT_NEAR(fixture.script().getPropertyFloat("sy").value_or(-1.f), 48.f, 1e-3f);
	const auto& data = fixture.list.getComponent<scene::component::UiList>();
	EXPECT_EQ(data.selectedIndex, 3);
	ASSERT_NE(data.findRow(1), nullptr);
	EXPECT_EQ(data.findRow(1)->text, "Second");
	EXPECT_EQ(data.findRow(1)->detail, "right");
	EXPECT_EQ(data.rows.size(), 30u);
}

TEST(LuaScrollList, theVisibleRangeFollowsTheLastLayout) {
	const ListFixture fixture{"first = 0\n"
							  "last = 0\n"
							  "function look()\n"
							  "  first, last = ui.list_get_visible(ui.find('List'))\n"
							  "end\n"
							  "function setup() ui.list_set_count(ui.find('List'), 1000) end\n"};
	EXPECT_TRUE(fixture.script().callFunction("setup"));
	EXPECT_TRUE(fixture.script().callFunction("look"));
	EXPECT_EQ(fixture.integer("first"), 0);
	fixture.world->getHudLayout().compute(*fixture.world, g_viewport, false);
	fixture.list.getComponent<scene::component::UiList>().scrollOffset = 240.f;
	fixture.world->getHudLayout().compute(*fixture.world, g_viewport, false);
	EXPECT_TRUE(fixture.script().callFunction("look"));
	EXPECT_EQ(fixture.integer("first"), 11);
	EXPECT_EQ(fixture.integer("last"), 15);
}

TEST(LuaScrollList, aScrollViewIsReadAndMovedFromAScript) {
	const ListFixture fixture{
			"x = -1\n"
			"y = -1\n"
			"function run()\n"
			"  local view = ui.find('View')\n"
			"  ui.scroll_set(view, 5, 120)\n"
			"  x, y = ui.scroll_get(view)\n"
			"  has = entity.has_component(view, 'UiScrollView') and entity.has_component(ui.find('List'), 'UiList')\n"
			"end\n"};
	EXPECT_TRUE(fixture.script().callFunction("run"));
	EXPECT_NEAR(fixture.script().getPropertyFloat("x").value_or(-1.f), 5.f, 1e-3f);
	EXPECT_NEAR(fixture.script().getPropertyFloat("y").value_or(-1.f), 120.f, 1e-3f);
	EXPECT_EQ(fixture.script().getPropertyBool("has").value_or(false), true);
	// The next layout keeps it inside the content: vertical only, 400 high in 100.
	fixture.world->getHudLayout().compute(*fixture.world, g_viewport, false);
	EXPECT_NEAR(fixture.view.getComponent<scene::component::UiScrollView>().scrollOffset.x(), 0.f, 1e-3f);
	EXPECT_NEAR(fixture.view.getComponent<scene::component::UiScrollView>().scrollOffset.y(), 120.f, 1e-3f);
}
