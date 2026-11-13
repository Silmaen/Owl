/**
 * @file VisualScriptCodegen_test.cpp
 * @author Silmaen
 * @date 08/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <script/LuaEngine.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace owl;
using namespace owl::script;
using namespace owl::script::visual;

namespace {

auto addNode(VisualScript& ioScript, const std::string& iType, std::map<std::string, std::string> iValues = {})
		-> core::UUID {
	const core::UUID id{static_cast<uint64_t>(ioScript.nodes.size() + 1)};
	ioScript.nodes.push_back({.id = id, .type = iType, .position = {0.f, 0.f}, .values = std::move(iValues)});
	return id;
}

void link(VisualScript& ioScript, const core::UUID iFrom, const std::string& iFromPin, const core::UUID iTo,
		  const std::string& iToPin) {
	ioScript.links.push_back({.fromNode = iFrom, .fromPin = iFromPin, .toNode = iTo, .toPin = iToPin, .waypoints = {}});
}

auto toBytes(const std::string& iText) -> std::vector<uint8_t> { return {iText.begin(), iText.end()}; }

auto load(ScriptInstance& ioInstance, const VisualScript& iScript, const std::string& iName) -> bool {
	if (!ioInstance.createFromBuffer(toBytes(serialize(iScript)), iName, 0))
		return false;
	for (const auto& property: ioInstance.getDeclaredProperties()) ioInstance.setProperty(property);
	return true;
}

// The ladder of the Engine Demo cellar: climb once, set a flag, play a sound, leave for the tavern.
auto ladder() -> VisualScript {
	VisualScript script;
	script.variables.push_back({.name = "tavern_scene",
								.type = ScriptPropertyType::String,
								.defaultValue = "scenes/tavern.owl",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "climbing",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "false",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto interact = addNode(script, "event.custom", {{"function", "on_ladder_interact"}});
	const auto climbing = addNode(script, "var.get", {{"variable", "climbing"}});
	const auto branch = addNode(script, "flow.branch");
	const auto set = addNode(script, "var.set", {{"variable", "climbing"}, {"Value", "true"}});
	const auto state = addNode(script, "call.gamestate.set", {{"key", "tavern_from_cellar"}, {"value", "1"}});
	const auto sound = addNode(script, "call.sound.play", {{"asset_path", "sounds/raycast/door.wav"}});
	const auto scene = addNode(script, "var.get", {{"variable", "tavern_scene"}});
	const auto transition = addNode(script, "call.scene.transition_to", {{"duration", "0.5"}, {"kind", "fade"}});
	link(script, interact, "Then", branch, "Exec");
	link(script, climbing, "Value", branch, "Condition");
	link(script, branch, "False", set, "Exec");
	link(script, set, "Then", state, "Exec");
	link(script, state, "Then", sound, "Exec");
	link(script, sound, "Then", transition, "Exec");
	link(script, scene, "Value", transition, "scene_path");
	return script;
}

}// namespace

TEST(VisualScriptCodegen, LinearGraphsReadLikeHandWrittenLua) {
	const auto lua = compile(ladder());
	ASSERT_TRUE(lua.has_value());
	EXPECT_TRUE(lua->contains("function on_ladder_interact(arg)\n"
							  "\tif not climbing then\n"
							  "\t\tclimbing = true\n"
							  "\t\tgamestate.set(\"tavern_from_cellar\", 1)\n"
							  "\t\tsound.play(\"sounds/raycast/door.wav\")\n"
							  "\t\tscene.transition_to(tavern_scene, \"fade\", 0.5)\n"
							  "\tend\n"
							  "end\n"))
			<< *lua;
	EXPECT_FALSE(lua->contains("__vs")) << *lua;
	EXPECT_FALSE(lua->contains("do\n")) << *lua;
}

TEST(VisualScriptCodegen, BranchesMeetAgainAfterTheirEnd) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "count",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "flag",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "false",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto branch = addNode(script, "flow.branch");
	const auto flag = addNode(script, "var.get", {{"variable", "flag"}});
	const auto setTrue = addNode(script, "var.set", {{"variable", "flag"}, {"Value", "true"}});
	const auto increment = addNode(script, "var.set", {{"variable", "count"}});
	const auto count = addNode(script, "var.get", {{"variable", "count"}});
	const auto add = addNode(script, "math.add", {{"B", "1"}});
	link(script, create, "Then", branch, "Exec");
	link(script, flag, "Value", branch, "Condition");
	link(script, branch, "True", increment, "Exec");
	link(script, branch, "False", setTrue, "Exec");
	link(script, setTrue, "Then", increment, "Exec");
	link(script, count, "Value", add, "A");
	link(script, add, "Result", increment, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_EQ(std::ranges::count(std::string_view{*lua}, '+'), 1) << *lua;
	EXPECT_FALSE(lua->contains("__join")) << *lua;
	EXPECT_TRUE(lua->contains("if not flag then")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "merge.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("count").value_or(0.f), 1.f, 1e-5f);
	core::Log::invalidate();
}

// Two events running the same nodes, and a branch whose paths meet inside them.
TEST(VisualScriptCodegen, NodesOfSeveralEventsAreOneJoinFunction) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "count",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "flag",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "false",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto custom = addNode(script, "event.custom", {{"function", "on_again"}});
	const auto branch = addNode(script, "flow.branch");
	const auto flag = addNode(script, "var.get", {{"variable", "flag"}});
	const auto setTrue = addNode(script, "var.set", {{"variable", "flag"}, {"Value", "true"}});
	const auto increment = addNode(script, "var.set", {{"variable", "count"}});
	const auto count = addNode(script, "var.get", {{"variable", "count"}});
	const auto add = addNode(script, "math.add", {{"B", "1"}});
	link(script, create, "Then", branch, "Exec");
	link(script, custom, "Then", branch, "Exec");
	link(script, flag, "Value", branch, "Condition");
	link(script, branch, "True", increment, "Exec");
	link(script, branch, "False", setTrue, "Exec");
	link(script, setTrue, "Then", increment, "Exec");
	link(script, count, "Value", add, "A");
	link(script, add, "Result", increment, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_EQ(std::ranges::count(std::string_view{*lua}, '+'), 1) << *lua;
	EXPECT_TRUE(lua->contains("__join1()")) << *lua;
	EXPECT_FALSE(lua->contains("__join2")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "join.owlvs"));
	instance.onCreate();
	EXPECT_TRUE(instance.callFunction("on_again"));
	EXPECT_NEAR(instance.getPropertyFloat("count").value_or(0.f), 2.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, UnreadResultsOfSeveralCallsShareTheDiscardName) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "total",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto update = addNode(script, "event.on_update");
	const auto first = addNode(script, "call.ui.scroll_get", {{"split.xy", "true"}});
	const auto second = addNode(script, "call.ui.scroll_get", {{"split.xy", "true"}});
	const auto add = addNode(script, "math.add");
	const auto set = addNode(script, "var.set", {{"variable", "total"}});
	link(script, update, "Then", first, "Exec");
	link(script, first, "Then", second, "Exec");
	link(script, second, "Then", set, "Exec");
	link(script, first, "y", add, "A");
	link(script, second, "y", add, "B");
	link(script, add, "Result", set, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("local _\n")) << *lua;
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, NoStatementStartsWithAParenthesis) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	const auto create = addNode(script, "event.on_create");
	const auto trace = addNode(script, "call.log.trace", {{"message", "ready"}});
	const auto list = addNode(script, "table.make_list", {{"A", "1"}});
	const auto set = addNode(script, "table.set", {{"Key", "1"}, {"Value", "2"}});
	link(script, create, "Then", trace, "Exec");
	link(script, trace, "Then", set, "Exec");
	link(script, list, "Result", set, "Table");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_FALSE(lua->contains("\n\t(")) << *lua;
	const ScriptInstance instance;
	EXPECT_TRUE(instance.createFromBuffer(toBytes(serialize(script)), "paren.owlvs", 0)) << *lua;
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, ValuesReadByAnotherEventStayInTheSlotTable) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "seen",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto update = addNode(script, "event.on_update");
	const auto destroy = addNode(script, "event.on_destroy");
	const auto set = addNode(script, "var.set", {{"variable", "seen"}});
	const auto add = addNode(script, "math.add", {{"B", "1"}});
	link(script, destroy, "Then", set, "Exec");
	link(script, update, "dt", add, "A");
	link(script, add, "Result", set, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value());
	EXPECT_TRUE(lua->contains("function on_update(dt)\n\t__vs[1] = dt\nend")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "slots.owlvs"));
	instance.onUpdate(0.5f);
	instance.onDestroy();
	EXPECT_NEAR(instance.getPropertyFloat("seen").value_or(0.f), 1.5f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, CustomNodesRunTheLuaOfTheirAuthor) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "result",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.customNodes.push_back(
			{.name = "clamp_speed",
			 .title = "Clamp Speed",
			 .inputs = {{.name = "speed", .type = PinType::Number}, {.name = "limit", .type = PinType::Number}},
			 .outputs = {{.name = "clamped", .type = PinType::Number}},
			 .code = "if speed > limit then\n\treturn limit\nend\nreturn speed",
			 .pure = true});
	script.customNodes.push_back({.name = "store",
								  .title = "Store",
								  .inputs = {{.name = "value", .type = PinType::Number}},
								  .outputs = {},
								  .code = "result = value * 2",
								  .pure = false});
	const auto create = addNode(script, "event.on_create");
	const auto clamp = addNode(script, "custom.pure", {{"custom", "clamp_speed"}, {"speed", "12"}, {"limit", "5"}});
	const auto store = addNode(script, "custom.call", {{"custom", "store"}});
	link(script, create, "Then", store, "Exec");
	link(script, clamp, "clamped", store, "value");
	const auto pins = getNodePins(script, *script.findNode(store));
	ASSERT_EQ(pins.inputs.size(), 2u);
	EXPECT_EQ(pins.inputs[1].name, "value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value());
	EXPECT_TRUE(lua->contains("store(clamp_speed(12, 5))")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "custom.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 10.f, 1e-5f);

	const auto back = deserialize(serialize(script), "custom.owlvs");
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->customNodes.front().code, script.customNodes.front().code) << serialize(script);
	EXPECT_EQ(*back, script);

	auto wrongKind = script;
	wrongKind.findNode(store)->type = "custom.pure";
	EXPECT_FALSE(compile(wrongKind).has_value());
	auto unknown = script;
	unknown.findNode(store)->values["custom"] = "missing";
	EXPECT_FALSE(compile(unknown).has_value());
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, AnnotatedLuaFunctionsBecomeNodes) {
	const auto functions = parseLuaFunctions("local M = {}\n"
											 "--- Keeps a value between two bounds.\n"
											 "---@pure\n"
											 "---@param value number\n"
											 "---@param low number? the smallest value\n"
											 "---@param high number\n"
											 "---@return number clamped\n"
											 "function M.clamp(value, low, high)\n"
											 "\treturn math.max(low, math.min(high, value))\n"
											 "end\n"
											 "function M.hidden() end\n"
											 "---@param target entity\n"
											 "---@return vector3\n"
											 "---@return boolean\n"
											 "function M.aim(target) end\n"
											 "return M\n");
	ASSERT_EQ(functions.size(), 2u);
	EXPECT_EQ(functions[0].name, "clamp");
	EXPECT_EQ(functions[0].description, "Keeps a value between two bounds.");
	EXPECT_TRUE(functions[0].pure);
	ASSERT_EQ(functions[0].inputs.size(), 3u);
	EXPECT_EQ(functions[0].inputs[1].name, "low");
	EXPECT_EQ(functions[0].inputs[1].type, PinType::Number);
	ASSERT_EQ(functions[0].outputs.size(), 1u);
	EXPECT_EQ(functions[0].outputs[0].name, "clamped");
	EXPECT_EQ(functions[1].name, "aim");
	EXPECT_FALSE(functions[1].pure);
	EXPECT_EQ(functions[1].inputs[0].type, PinType::Entity);
	ASSERT_EQ(functions[1].outputs.size(), 2u);
	EXPECT_EQ(functions[1].outputs[0].name, "Result");
	EXPECT_EQ(functions[1].outputs[1].name, "Result2");
	EXPECT_EQ(functions[1].outputs[1].type, PinType::Boolean);
	EXPECT_EQ(getLuaModulePath("scripts.lib.maths"), "scripts/lib/maths.lua");
}

TEST(VisualScriptCodegen, LuaFunctionNodesCallTheirModule) {
	core::Log::init(core::Log::Level::Off);
	const std::string module = "local M = {}\n"
							   "---@pure\n"
							   "---@param value number\n"
							   "---@return number doubled\n"
							   "function M.double(value) return value * 2 end\n"
							   "---@param value number\n"
							   "function M.store(value) stored = value end\n"
							   "return M\n";
	setLibraryReader([&module](const std::string_view iPath) -> std::optional<std::string> {
		if (iPath == "scripts/lib/maths.lua")
			return module;
		return std::nullopt;
	});
	VisualScript script;
	const auto create = addNode(script, "event.on_create");
	const auto doubled =
			addNode(script, "lua.pure", {{"module", "scripts.lib.maths"}, {"function", "double"}, {"value", "21"}});
	const auto store = addNode(script, "lua.call", {{"module", "scripts.lib.maths"}, {"function", "store"}});
	link(script, create, "Then", store, "Exec");
	link(script, doubled, "doubled", store, "value");
	ASSERT_NE(findNodeLuaFunction(*script.findNode(store)), nullptr);
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("require(\"scripts.lib.maths\").store(require(\"scripts.lib.maths\").double(21))"))
			<< *lua;
	const LuaEngine engine;
	engine.setModuleReader([&module](const std::string& iPath) -> std::optional<std::vector<uint8_t>> {
		if (iPath == "scripts/lib/maths.lua")
			return toBytes(module);
		return std::nullopt;
	});
	ASSERT_TRUE(engine.loadBuffer(toBytes(*lua), "caller.lua"));
	ASSERT_TRUE(engine.callFunction("on_create"));
	EXPECT_EQ(engine.getGlobalInt("stored"), 42);

	auto missing = script;
	missing.findNode(store)->values["function"] = "unknown";
	EXPECT_FALSE(compile(missing).has_value());
	setLibraryReader({});
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, MathNodesComputeTheirFunctions) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "result",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "rounded",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto sqrt = addNode(script, "math.sqrt", {{"A", "16"}});
	const auto clamp = addNode(script, "math.clamp", {{"Value", "5"}, {"Min", "0"}, {"Max", "2"}});
	const auto power = addNode(script, "math.power", {{"A", "2"}, {"Exponent", "3"}});
	const auto lerp = addNode(script, "math.lerp", {{"A", "0"}, {"B", "10"}, {"Alpha", "0.5"}});
	const auto first = addNode(script, "math.add");
	const auto second = addNode(script, "math.add");
	const auto total = addNode(script, "math.add");
	const auto setResult = addNode(script, "var.set", {{"variable", "result"}});
	const auto round = addNode(script, "math.round", {{"A", "2.5"}});
	const auto ceil = addNode(script, "math.ceil", {{"A", "1.2"}});
	const auto sum = addNode(script, "math.add");
	const auto setRounded = addNode(script, "var.set", {{"variable", "rounded"}});
	link(script, create, "Then", setResult, "Exec");
	link(script, sqrt, "Result", first, "A");
	link(script, clamp, "Result", first, "B");
	link(script, power, "Result", second, "A");
	link(script, lerp, "Result", second, "B");
	link(script, first, "Result", total, "A");
	link(script, second, "Result", total, "B");
	link(script, total, "Result", setResult, "Value");
	link(script, setResult, "Then", setRounded, "Exec");
	link(script, round, "Result", sum, "A");
	link(script, ceil, "Result", sum, "B");
	link(script, sum, "Result", setRounded, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("math.sqrt(16) + math.max(0, math.min(2, 5))")) << *lua;
	EXPECT_TRUE(lua->contains("math.pow(2, 3)")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "math.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 19.f, 1e-5f);
	EXPECT_NEAR(instance.getPropertyFloat("rounded").value_or(0.f), 5.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, TableNodesReadLikeLuaTables) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "speed",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "names",
								.type = ScriptPropertyType::Strings,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "last",
								.type = ScriptPropertyType::Int,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "found",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "false",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto config = addNode(script, "value.table", {{"Value", "speed = 3, name = hero"}});
	const auto field = addNode(script, "table.get", {{"Key", "speed"}});
	const auto setSpeed = addNode(script, "var.set", {{"variable", "speed"}});
	const auto split = addNode(script, "text.split", {{"Text", "a,b,c"}});
	const auto loop = addNode(script, "flow.for_each");
	const auto names = addNode(script, "var.get", {{"variable", "names"}});
	const auto upper = addNode(script, "text.upper");
	const auto append = addNode(script, "table.append");
	const auto setLast = addNode(script, "var.set", {{"variable", "last"}});
	const auto names2 = addNode(script, "var.get", {{"variable", "names"}});
	const auto contains = addNode(script, "table.contains", {{"Value", "B"}});
	const auto setFound = addNode(script, "var.set", {{"variable", "found"}});
	link(script, create, "Then", setSpeed, "Exec");
	link(script, config, "Result", field, "Table");
	link(script, field, "Result", setSpeed, "Value");
	link(script, setSpeed, "Then", loop, "Exec");
	link(script, split, "Result", loop, "List");
	link(script, loop, "Body", append, "Exec");
	link(script, names, "Value", append, "List");
	link(script, loop, "Item", upper, "Text");
	link(script, upper, "Result", append, "Value");
	link(script, loop, "Completed", setLast, "Exec");
	link(script, loop, "Index", setLast, "Value");
	link(script, setLast, "Then", setFound, "Exec");
	link(script, names2, "Value", contains, "List");
	link(script, contains, "Result", setFound, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("speed = ({ speed = 3, name = \"hero\" }).speed")) << *lua;
	EXPECT_TRUE(lua->contains("for i, item in ipairs(string.split(\"a,b,c\", \",\")) do")) << *lua;
	EXPECT_TRUE(lua->contains("table.insert(names, string.upper(item))")) << *lua;
	EXPECT_TRUE(lua->contains("{ name = \"names\", type = \"strings\", default = {} }")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "tables.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("speed").value_or(0.f), 3.f, 1e-5f);
	EXPECT_EQ(instance.getPropertyInt("last").value_or(0), 3);
	EXPECT_TRUE(instance.getPropertyBool("found").value_or(false));

	auto badKey = script;
	badKey.findNode(config)->values["Value"] = "1x = 2";
	EXPECT_FALSE(compile(badKey).has_value());
	auto badDefault = script;
	badDefault.variables[1].type = ScriptPropertyType::Numbers;
	badDefault.variables[1].defaultValue = "1, two";
	EXPECT_FALSE(compile(badDefault).has_value());
	badDefault.variables[1].defaultValue = "1, 2.5";
	const auto numbers = compile(badDefault);
	ASSERT_TRUE(numbers.has_value());
	EXPECT_TRUE(numbers->contains("default = { 1, 2.5 }")) << *numbers;
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, TextNodesBuildAndParseStrings) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "label",
								.type = ScriptPropertyType::String,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "part",
								.type = ScriptPropertyType::String,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "value",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "size",
								.type = ScriptPropertyType::Int,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto format = addNode(script, "text.format", {{"Format", "%d / %d blocks"}, {"A", "3"}, {"B", "10"}});
	const auto setLabel = addNode(script, "var.set", {{"variable", "label"}});
	const auto sub = addNode(script, "text.substring", {{"Text", "hello"}, {"First", "2"}, {"Last", "3"}});
	const auto setPart = addNode(script, "var.set", {{"variable", "part"}});
	const auto number = addNode(script, "text.to_number", {{"Text", "4.5"}});
	const auto setValue = addNode(script, "var.set", {{"variable", "value"}});
	const auto list = addNode(script, "table.make_list", {{"A", "1"}, {"B", "2"}});
	const auto length = addNode(script, "table.length");
	const auto setSize = addNode(script, "var.set", {{"variable", "size"}});
	link(script, create, "Then", setLabel, "Exec");
	link(script, format, "Result", setLabel, "Value");
	link(script, setLabel, "Then", setPart, "Exec");
	link(script, sub, "Result", setPart, "Value");
	link(script, setPart, "Then", setValue, "Exec");
	link(script, number, "Result", setValue, "Value");
	link(script, setValue, "Then", setSize, "Exec");
	link(script, list, "Result", length, "List");
	link(script, length, "Result", setSize, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("label = string.format(\"%d / %d blocks\", 3, 10)\n")) << *lua;
	EXPECT_TRUE(lua->contains("size = #({ 1, 2 })\n")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "text.owlvs"));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyString("label").value_or(""), "3 / 10 blocks");
	EXPECT_EQ(instance.getPropertyString("part").value_or(""), "el");
	EXPECT_NEAR(instance.getPropertyFloat("value").value_or(0.f), 4.5f, 1e-5f);
	EXPECT_EQ(instance.getPropertyInt("size").value_or(0), 2);
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, SelectAndDefaultPickAValue) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "flag",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "true",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "picked",
								.type = ScriptPropertyType::String,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.variables.push_back({.name = "clip",
								.type = ScriptPropertyType::String,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto flag = addNode(script, "var.get", {{"variable", "flag"}});
	const auto select = addNode(script, "logic.select", {{"True", "walk"}, {"False", "idle"}});
	const auto setPicked = addNode(script, "var.set", {{"variable", "picked"}});
	const auto sounds = addNode(script, "value.table", {{"Value", "cross = treasure.wav, food = food.wav"}});
	const auto field = addNode(script, "table.get", {{"Key", "gun"}});
	const auto fallback = addNode(script, "logic.default", {{"Default", "coin.wav"}});
	const auto setSound = addNode(script, "var.set", {{"variable", "clip"}});
	link(script, create, "Then", setPicked, "Exec");
	link(script, flag, "Value", select, "Condition");
	link(script, select, "Result", setPicked, "Value");
	link(script, setPicked, "Then", setSound, "Exec");
	link(script, sounds, "Result", field, "Table");
	link(script, field, "Result", fallback, "Value");
	link(script, fallback, "Result", setSound, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("picked = if flag then \"walk\" else \"idle\"\n")) << *lua;
	EXPECT_TRUE(lua->contains("clip = ({ cross = \"treasure.wav\", food = \"food.wav\" }).gun or \"coin.wav\"\n"))
			<< *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "logic.owlvs"));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyString("picked").value_or(""), "walk");
	EXPECT_EQ(instance.getPropertyString("clip").value_or(""), "coin.wav");
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, LoopNodesStepWhileWalkPairsAndBreak) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	for (const auto* name: {"total", "count", "keys"})
		script.variables.push_back({.name = name,
									.type = ScriptPropertyType::Float,
									.defaultValue = "0",
									.min = {},
									.max = {},
									.step = {},
									.exposed = true});
	const auto create = addNode(script, "event.on_create");
	// for i = 10, 1, -3: total = total + i
	const auto loop = addNode(script, "flow.for", {{"First", "10"}, {"Last", "1"}, {"Step", "-3"}});
	const auto total = addNode(script, "var.get", {{"variable", "total"}});
	const auto add = addNode(script, "math.add");
	const auto setTotal = addNode(script, "var.set", {{"variable", "total"}});
	link(script, create, "Then", loop, "Exec");
	link(script, loop, "Body", setTotal, "Exec");
	link(script, total, "Value", add, "A");
	link(script, loop, "Index", add, "B");
	link(script, add, "Result", setTotal, "Value");
	// while count < 100: count = count + 1, break at 5
	const auto whileLoop = addNode(script, "flow.while");
	const auto count = addNode(script, "var.get", {{"variable", "count"}});
	const auto less = addNode(script, "compare.less", {{"B", "100"}});
	const auto count2 = addNode(script, "var.get", {{"variable", "count"}});
	const auto inc = addNode(script, "math.add", {{"B", "1"}});
	const auto setCount = addNode(script, "var.set", {{"variable", "count"}});
	const auto count3 = addNode(script, "var.get", {{"variable", "count"}});
	const auto atFive = addNode(script, "compare.greater_equal", {{"B", "5"}});
	const auto branch = addNode(script, "flow.branch");
	const auto stop = addNode(script, "flow.break");
	link(script, loop, "Completed", whileLoop, "Exec");
	link(script, count, "Value", less, "A");
	link(script, less, "Result", whileLoop, "Condition");
	link(script, whileLoop, "Body", setCount, "Exec");
	link(script, count2, "Value", inc, "A");
	link(script, inc, "Result", setCount, "Value");
	link(script, setCount, "Then", branch, "Exec");
	link(script, count3, "Value", atFive, "A");
	link(script, atFive, "Result", branch, "Condition");
	link(script, branch, "True", stop, "Exec");
	// for key, value in pairs({a = 2, b = 3}): keys = keys + value, skipping nothing
	const auto pairs = addNode(script, "flow.for_each_pair");
	const auto table = addNode(script, "value.table", {{"Value", "a = 2, b = 3"}});
	const auto keys = addNode(script, "var.get", {{"variable", "keys"}});
	const auto sum = addNode(script, "math.add");
	const auto setKeys = addNode(script, "var.set", {{"variable", "keys"}});
	const auto skip = addNode(script, "flow.continue");
	link(script, whileLoop, "Completed", pairs, "Exec");
	link(script, table, "Result", pairs, "Table");
	link(script, pairs, "Body", setKeys, "Exec");
	link(script, keys, "Value", sum, "A");
	link(script, pairs, "Value", sum, "B");
	link(script, sum, "Result", setKeys, "Value");
	link(script, setKeys, "Then", skip, "Exec");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("for i = 10, 1, -3 do")) << *lua;
	EXPECT_TRUE(lua->contains("while count < 100 do")) << *lua;
	EXPECT_TRUE(lua->contains("break")) << *lua;
	EXPECT_TRUE(lua->contains("in pairs(")) << *lua;
	EXPECT_TRUE(lua->contains("continue")) << *lua;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "loops.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("total").value_or(0.f), 22.f, 1e-5f);
	EXPECT_NEAR(instance.getPropertyFloat("count").value_or(0.f), 5.f, 1e-5f);
	EXPECT_NEAR(instance.getPropertyFloat("keys").value_or(0.f), 5.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, ASideThatEndsIsNoMergeOfItsBranch) {
	core::Log::init(core::Log::Level::Off);
	// if a then hit() elseif b then hit() end -- with the second test's False side linked to nothing.
	VisualScript script;
	script.variables.push_back({.name = "hits",
								.type = ScriptPropertyType::Float,
								.defaultValue = "0",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	script.functions.push_back({.name = "check",
								.inputs = {},
								.outputs = {},
								.macro = false,
								.exits = {"Then"},
								.exported = false,
								.category = {},
								.pure = false});
	const auto create = addNode(script, "event.on_create");
	const auto call = addNode(script, "func.call", {{"function", "check"}});
	const auto entry = addNode(script, "func.entry", {{"function", "check"}});
	link(script, create, "Then", call, "Exec");
	const auto first = addNode(script, "flow.branch", {{"Condition", "false"}});
	const auto second = addNode(script, "flow.branch", {{"Condition", "false"}});
	const auto hits = addNode(script, "var.get", {{"variable", "hits"}});
	const auto add = addNode(script, "math.add", {{"B", "1"}});
	const auto hit = addNode(script, "var.set", {{"variable", "hits"}});
	link(script, entry, "Then", first, "Exec");
	link(script, first, "True", hit, "Exec");
	link(script, first, "False", second, "Exec");
	link(script, second, "True", hit, "Exec");
	link(script, hits, "Value", add, "A");
	link(script, add, "Result", hit, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "ends.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("hits").value_or(-1.f), 0.f, 1e-5f) << *lua;
	core::Log::invalidate();
}

TEST(VisualScriptCodegen, TheDebugBuildKeepsTheShortCircuitOfAndOrSelect) {
	core::Log::init(core::Log::Level::Off);
	// found = (box ~= nil) and box.closed; label = box and box.name or "none" -- with box nil.
	VisualScript script;
	script.variables.push_back({.name = "box",
								.type = ScriptPropertyType::Numbers,
								.defaultValue = "",
								.min = {},
								.max = {},
								.step = {},
								.exposed = false});
	script.variables.push_back({.name = "found",
								.type = ScriptPropertyType::Bool,
								.defaultValue = "true",
								.min = {},
								.max = {},
								.step = {},
								.exposed = true});
	const auto create = addNode(script, "event.on_create");
	const auto clear = addNode(script, "var.set", {{"variable", "box"}});
	const auto none = addNode(script, "value.nil");
	const auto box = addNode(script, "var.get", {{"variable", "box"}});
	const auto isSet = addNode(script, "logic.is_set");
	const auto box2 = addNode(script, "var.get", {{"variable", "box"}});
	const auto field = addNode(script, "table.get", {{"Key", "closed"}});
	const auto both = addNode(script, "logic.and");
	const auto box3 = addNode(script, "var.get", {{"variable", "box"}});
	const auto field2 = addNode(script, "table.get", {{"Key", "open"}});
	const auto pick = addNode(script, "logic.select", {{"False", "false"}});
	const auto either = addNode(script, "logic.or");
	const auto setFound = addNode(script, "var.set", {{"variable", "found"}});
	link(script, create, "Then", clear, "Exec");
	link(script, none, "Result", clear, "Value");
	link(script, clear, "Then", setFound, "Exec");
	link(script, box, "Value", isSet, "Value");
	link(script, isSet, "Result", both, "A");
	link(script, box2, "Value", field, "Table");
	link(script, field, "Result", both, "B");
	link(script, isSet, "Result", pick, "Condition");
	link(script, box3, "Value", field2, "Table");
	link(script, field2, "Result", pick, "True");
	link(script, both, "Result", either, "A");
	link(script, pick, "Result", either, "B");
	link(script, either, "Result", setFound, "Value");
	const auto debug = compileForDebug(script);
	ASSERT_TRUE(debug.has_value()) << debug.error().front().message;
	auto scn = mkShared<scene::Scene>();
	const ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(instance.createFromBuffer(toBytes(debug->lua), "short_circuit.lua", 0)) << debug->lua;
	for (const auto& property: instance.getDeclaredProperties()) instance.setProperty(property);
	instance.onCreate();
	EXPECT_FALSE(instance.getPropertyBool("found").value_or(true)) << debug->lua;
	core::Log::invalidate();
}
