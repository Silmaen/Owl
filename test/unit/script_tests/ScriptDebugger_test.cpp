/**
 * @file ScriptDebugger_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <script/ScriptDebugger.h>
#include <script/ScriptInstance.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

using namespace owl;
using namespace owl::script;

namespace {

auto toBytes(const std::string& iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

auto find(const std::vector<WatchValue>& iValues, const std::string& iName) -> const WatchValue* {
	const auto found = std::ranges::find(iValues, iName, &WatchValue::name);
	return found == iValues.end() ? nullptr : &*found;
}

class ScriptDebuggerTest : public testing::Test {
protected:
	void SetUp() override { core::Log::init(core::Log::Level::Off); }

	void TearDown() override {
		ScriptDebugger::clear();
		ScriptDebugger::setEnabled(false);
		core::Log::invalidate();
	}
};

constexpr auto g_counter = R"(count = 0
label = "start"
function on_update(dt)
	local step = 2
	count = count + step
end
)";

}// namespace

TEST_F(ScriptDebuggerTest, PathsNameTheSameScriptOnAFolderBoundary) {
	EXPECT_TRUE(ScriptDebugger::isSameScript("scripts/a.lua", "/project/scripts/a.lua"));
	EXPECT_TRUE(ScriptDebugger::isSameScript("@/project/scripts/a.lua", "scripts\\a.lua"));
	EXPECT_TRUE(ScriptDebugger::isSameScript("a.lua", "a.lua"));
	EXPECT_FALSE(ScriptDebugger::isSameScript("scripts/a.lua", "/project/scripts/ba.lua"));
	EXPECT_FALSE(ScriptDebugger::isSameScript("", "a.lua"));
}

TEST_F(ScriptDebuggerTest, BreakpointsAreSetAndRemovedPerScript) {
	ScriptDebugger::setBreakpoint("/p/scripts/a.lua", 4, true);
	ScriptDebugger::setBreakpoint("/p/scripts/a.lua", 2, true);
	EXPECT_EQ(ScriptDebugger::getBreakpoints("/p/scripts/a.lua"), (std::vector{2, 4}));
	EXPECT_FALSE(ScriptDebugger::hasBreakpoints());
	ScriptDebugger::setEnabled(true);
	EXPECT_TRUE(ScriptDebugger::hasBreakpoints());
	EXPECT_TRUE(ScriptDebugger::isBreakpoint("@scripts/a.lua", 4));
	EXPECT_FALSE(ScriptDebugger::isBreakpoint("scripts/a.lua", 3));
	ScriptDebugger::setBreakpoint("/p/scripts/a.lua", 4, false);
	ScriptDebugger::setBreakpoints("/p/scripts/a.lua", {});
	EXPECT_FALSE(ScriptDebugger::hasBreakpoints());
}

TEST_F(ScriptDebuggerTest, AHitKeepsTheLineAndTheLocals) {
	ScriptDebugger::setEnabled(true);
	ScriptDebugger::setBreakpoint("counter.lua", 5, true);
	const ScriptInstance instance;
	ASSERT_TRUE(instance.createFromBuffer(toBytes(g_counter), "counter.lua", 7));
	instance.onUpdate(0.5f);
	EXPECT_EQ(instance.getPropertyInt("count").value_or(-1), 2);
	const auto hit = ScriptDebugger::takeHit();
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->line, 5);
	EXPECT_TRUE(ScriptDebugger::isSameScript(hit->script, "counter.lua"));
	const auto* step = find(hit->locals, "step");
	ASSERT_NE(step, nullptr);
	EXPECT_EQ(step->value, "2");
	EXPECT_NE(find(hit->locals, "dt"), nullptr);
	EXPECT_FALSE(ScriptDebugger::takeHit().has_value());
}

TEST_F(ScriptDebuggerTest, ADisabledDebuggerNeverStops) {
	ScriptDebugger::setBreakpoint("counter.lua", 5, true);
	const ScriptInstance instance;
	ASSERT_TRUE(instance.createFromBuffer(toBytes(g_counter), "counter.lua", 7));
	instance.onUpdate(0.5f);
	EXPECT_FALSE(ScriptDebugger::takeHit().has_value());
}

TEST_F(ScriptDebuggerTest, WatchValuesListTheScriptVariablesOnly) {
	const ScriptInstance instance;
	ASSERT_TRUE(instance.createFromBuffer(toBytes(g_counter), "counter.lua", 7));
	instance.onUpdate(0.5f);
	const auto values = instance.getWatchValues();
	const auto* count = find(values, "count");
	ASSERT_NE(count, nullptr);
	EXPECT_EQ(count->type, "number");
	EXPECT_EQ(count->value, "2");
	ASSERT_NE(find(values, "label"), nullptr);
	EXPECT_EQ(find(values, "label")->value, "\"start\"");
	EXPECT_EQ(find(values, "on_update"), nullptr);
	EXPECT_EQ(find(values, "math"), nullptr);
	EXPECT_EQ(find(values, "transform"), nullptr);
	EXPECT_TRUE(instance.getProbeValues().empty());
}

TEST_F(ScriptDebuggerTest, VisualScriptsProbeTheirWiresAndMapTheirNodes) {
	using namespace owl::script::visual;
	VisualScript script;
	script.variables.push_back({.name = "total", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const core::UUID update{1};
	const core::UUID add{2};
	const core::UUID set{3};
	script.nodes.push_back({.id = update, .type = "event.on_update", .position = {0.f, 0.f}, .values = {}});
	script.nodes.push_back({.id = add, .type = "math.add", .position = {0.f, 0.f}, .values = {{"B", "3"}}});
	script.nodes.push_back({.id = set, .type = "var.set", .position = {0.f, 0.f}, .values = {{"variable", "total"}}});
	script.links.push_back({.fromNode = update, .fromPin = "Then", .toNode = set, .toPin = "Exec", .waypoints = {}});
	script.links.push_back({.fromNode = update, .fromPin = "dt", .toNode = add, .toPin = "A", .waypoints = {}});
	script.links.push_back({.fromNode = add, .fromPin = "Result", .toNode = set, .toPin = "Value", .waypoints = {}});

	const auto build = compileForDebug(script);
	ASSERT_TRUE(build.has_value());
	ASSERT_TRUE(build->nodeLines.contains(set));
	ASSERT_TRUE(build->nodeLines.contains(add));
	EXPECT_EQ(compile(script).value(), compile(script, CompileMode::Game).value());

	ScriptDebugger::setEnabled(true);
	ScriptDebugger::setBreakpoint("sum.owlvs", build->nodeLines.at(set).front(), true);
	const ScriptInstance instance;
	const auto text = serialize(script);
	ASSERT_TRUE(instance.createFromBuffer(toBytes(text), "sum.owlvs", 0));
	instance.onUpdate(0.5f);
	const auto probes = instance.getProbeValues();
	const auto* result = find(probes, getProbeKey(add, "Result"));
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->value, "3.5");
	const auto hit = ScriptDebugger::takeHit();
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->line, build->nodeLines.at(set).front());
}
