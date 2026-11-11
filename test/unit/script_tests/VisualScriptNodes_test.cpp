/**
 * @file VisualScriptNodes_test.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <math/Transform.h>
#include <math/linAlgebra.h>
#include <scene/Entity.h>
#include <scene/Scene.h>
#include <scene/component/Transform.h>
#include <script/ScriptDebugger.h>
#include <script/ScriptInstance.h>
#include <script/ScriptTypeChecker.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <tuple>
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

void addVariable(VisualScript& ioScript, const std::string& iName, const ScriptPropertyType iType) {
	ioScript.variables.push_back(
			{.name = iName, .type = iType, .defaultValue = "0", .min = {}, .max = {}, .step = {}, .exposed = true});
}

// Appends `Set <variable>` to the execution chain ending at ioLast, its value read from an output pin.
void setFrom(VisualScript& ioScript, core::UUID& ioLast, const std::string& iVariable, const core::UUID iFrom,
			 const std::string& iPin) {
	const auto set = addNode(ioScript, "var.set", {{"variable", iVariable}});
	link(ioScript, ioLast, "Then", set, "Exec");
	link(ioScript, iFrom, iPin, set, "Value");
	ioLast = set;
}

auto load(const ScriptInstance& iInstance, const VisualScript& iScript, const uint64_t iEntity = 0) -> bool {
	const auto text = serialize(iScript);
	if (!iInstance.createFromBuffer({text.begin(), text.end()}, "nodes.owlvs", iEntity))
		return false;
	for (const auto& property: iInstance.getDeclaredProperties()) iInstance.setProperty(property);
	return true;
}

}// namespace

TEST(VisualScriptNodes, MathNodesCompute) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addVariable(script, "result", ScriptPropertyType::Float);
	addVariable(script, "angle", ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto square = addNode(script, "math.square", {{"A", "3"}});
	const auto logBase = addNode(script, "math.log_base", {{"A", "8"}, {"Base", "2"}});
	const auto exp = addNode(script, "math.exp", {{"A", "0"}});
	const auto sum = addNode(script, "math.add");
	const auto total = addNode(script, "math.add");
	link(script, square, "Result", sum, "A");
	link(script, logBase, "Result", sum, "B");
	link(script, sum, "Result", total, "A");
	link(script, exp, "Result", total, "B");
	setFrom(script, last, "result", total, "Result");
	const auto degrees = addNode(script, "math.degrees");
	const auto acos = addNode(script, "math.acos", {{"A", "0"}});
	link(script, acos, "Result", degrees, "A");
	setFrom(script, last, "angle", degrees, "Result");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 13.f, 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("angle").value_or(0.f), 90.f, 1e-3f);
	core::Log::invalidate();
}

TEST(VisualScriptNodes, ListNodesFollowTheirItemCount) {
	VisualScript script;
	const auto make = addNode(script, "table.make_list", {{"count", "6"}});
	const auto split = addNode(script, "table.break_list");
	const auto many = addNode(script, "table.make_list", {{"count", "99"}});
	EXPECT_EQ(getNodePins(script, *script.findNode(make)).inputs.size(), 6u);
	EXPECT_EQ(getNodePins(script, *script.findNode(split)).outputs.size(), 2u);
	EXPECT_EQ(getNodePins(script, *script.findNode(many)).inputs.size(), g_MaxListItems);
	EXPECT_EQ(getNodePins(script, *script.findNode(make)).inputs.back().name, "F");
	EXPECT_EQ(getNodeLua(*findNodeType("table.break_list"), *script.findNode(split)), "{0}[1], {0}[2]");
}

TEST(VisualScriptNodes, ListNodesBuildAndSplitAList) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addVariable(script, "third", ScriptPropertyType::Float);
	addVariable(script, "length", ScriptPropertyType::Int);
	auto last = addNode(script, "event.on_create");
	const auto make = addNode(script, "table.make_list", {{"count", "3"}, {"A", "5"}, {"B", "6"}, {"C", "7"}});
	const auto split = addNode(script, "table.break_list", {{"count", "3"}});
	const auto length = addNode(script, "table.length");
	link(script, make, "Result", split, "List");
	link(script, make, "Result", length, "List");
	setFrom(script, last, "third", split, "C");
	setFrom(script, last, "length", length, "Result");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("third").value_or(0.f), 7.f, 1e-6f);
	EXPECT_EQ(instance.getPropertyInt("length").value_or(0), 3);
	core::Log::invalidate();
}

TEST(VisualScriptNodes, TransformNodesReadAndWriteTheEntity) {
	core::Log::init(core::Log::Level::Off);
	auto scn = mkShared<scene::Scene>();
	auto entity = scn->createEntity("Crate");
	entity.getComponent<scene::component::Transform>().transform.translation() = {1.f, 2.f, 3.f};
	VisualScript script;
	addVariable(script, "px", ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto get = addNode(script, "transform.get_transform");
	const auto parts = addNode(script, "transform.break");
	const auto position = addNode(script, "math.break_vector3");
	link(script, get, "Result", parts, "Transform");
	link(script, parts, "Position", position, "Value");
	setFrom(script, last, "px", position, "X");
	const auto value = addNode(script, "value.transform", {{"Value", "4, 5, 6; 0, 0, 0.5; 2, 2, 2"}});
	const auto set = addNode(script, "transform.set_transform");
	link(script, last, "Then", set, "Exec");
	link(script, value, "Result", set, "Transform");
	const ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, static_cast<uint64_t>(entity.getUUID())));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("px").value_or(0.f), 1.f, 1e-6f);
	const auto& transform = entity.getComponent<scene::component::Transform>().transform;
	EXPECT_NEAR(transform.translation().y(), 5.f, 1e-5f);
	EXPECT_NEAR(transform.rotation().z(), 0.5f, 1e-5f);
	EXPECT_NEAR(transform.scale().x(), 2.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptNodes, MatrixNodesMatchTheEngineTransform) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	for (const auto* name: {"rx", "ry", "rz", "diagonal", "offDiagonal"})
		addVariable(script, name, ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto value = addNode(script, "value.transform", {{"Value", "1, 2, 3; 0.3, -0.2, 1.1; 2, 1.5, 0.5"}});
	const auto matrix = addNode(script, "matrix.from_transform");
	const auto point = addNode(script, "matrix.transform_point", {{"Point", "1, -2, 0.5"}});
	const auto split = addNode(script, "math.break_vector3");
	link(script, value, "Result", matrix, "Transform");
	link(script, matrix, "Result", point, "Matrix");
	link(script, point, "Result", split, "Value");
	setFrom(script, last, "rx", split, "X");
	setFrom(script, last, "ry", split, "Y");
	setFrom(script, last, "rz", split, "Z");
	const auto inverse = addNode(script, "matrix.inverse");
	const auto product = addNode(script, "matrix.multiply");
	const auto diagonal = addNode(script, "matrix.get_element", {{"Row", "2"}, {"Column", "2"}});
	const auto offDiagonal = addNode(script, "matrix.get_element", {{"Row", "1"}, {"Column", "3"}});
	link(script, matrix, "Result", inverse, "Matrix");
	link(script, matrix, "Result", product, "A");
	link(script, inverse, "Result", product, "B");
	link(script, product, "Result", diagonal, "Matrix");
	link(script, product, "Result", offDiagonal, "Matrix");
	setFrom(script, last, "diagonal", diagonal, "Result");
	setFrom(script, last, "offDiagonal", offDiagonal, "Result");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	const math::Transform engine{{1.f, 2.f, 3.f}, {0.3f, -0.2f, 1.1f}, {2.f, 1.5f, 0.5f}};
	const auto expected = engine() * math::vec4{1.f, -2.f, 0.5f, 1.f};
	EXPECT_NEAR(instance.getPropertyFloat("rx").value_or(0.f), expected.x(), 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("ry").value_or(0.f), expected.y(), 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("rz").value_or(0.f), expected.z(), 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("diagonal").value_or(0.f), 1.f, 1e-5f);
	EXPECT_NEAR(instance.getPropertyFloat("offDiagonal").value_or(1.f), 0.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScriptNodes, HelpersAreWrittenOnceAndTypeCheck) {
	VisualScript script;
	auto last = addNode(script, "event.on_create");
	for (const auto* type: {"matrix.identity", "matrix.transpose", "matrix.inverse", "matrix.transform_direction",
							"transform.get_transform"}) {
		std::ignore = addNode(script, type);
	}
	const auto first = addNode(script, "matrix.inverse");
	const auto second = addNode(script, "matrix.inverse");
	link(script, first, "Result", second, "Matrix");
	const auto set = addNode(script, "transform.set_transform");
	link(script, last, "Then", set, "Exec");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value());
	EXPECT_EQ(lua->find("local function __owl_mat_inverse"), lua->rfind("local function __owl_mat_inverse"));
	EXPECT_NE(lua->find("local function __owl_transform_set"), std::string::npos);
	EXPECT_EQ(lua->find("local function __owl_mat_mul"), std::string::npos);
	ScriptTypeChecker checker;
	std::string errors;
	for (const auto& diagnostic: checker.check(*lua, "helpers.owlvs"))
		if (diagnostic.severity == ScriptDiagnostic::Severity::Error)
			errors += std::format("{}: {}\n", diagnostic.line, diagnostic.message);
	EXPECT_TRUE(errors.empty()) << errors;
}

TEST(VisualScriptNodes, ConstantsOfTransformsAndMatricesNeedTheirNumbers) {
	VisualScript script;
	const auto create = addNode(script, "event.on_create");
	const auto value = addNode(script, "value.matrix", {{"Value", "1, 2, 3"}});
	const auto set = addNode(script, "transform.set_transform");
	link(script, create, "Then", set, "Exec");
	const auto transform = addNode(script, "value.transform", {{"Value", "1, 2, 3"}});
	link(script, transform, "Result", set, "Transform");
	const auto inverse = addNode(script, "matrix.inverse");
	link(script, value, "Result", inverse, "Matrix");
	const auto result = compile(script);
	ASSERT_FALSE(result.has_value());
	EXPECT_TRUE(std::ranges::any_of(result.error(), [&transform](const CompileDiagnostic& iError) -> bool {
		return iError.node == transform;
	}));
	EXPECT_TRUE(canConnect(PinType::Matrix, PinType::Table));
	EXPECT_TRUE(canConnect(PinType::Table, PinType::Matrix));
	EXPECT_FALSE(canConnect(PinType::Transform, PinType::Matrix));
}

TEST(VisualScriptNodes, WatchedTablesListTheirEntriesInOrder) {
	core::Log::init(core::Log::Level::Off);
	const std::string text = "grid = { 3, 1, 2, name = \"a\", [10] = 4 }\n";
	const ScriptInstance instance;
	ASSERT_TRUE(instance.createFromBuffer({text.begin(), text.end()}, "watch.lua", 0));
	const auto values = instance.getWatchValues();
	const auto grid = std::ranges::find(values, std::string{"grid"}, &WatchValue::name);
	ASSERT_NE(grid, values.end());
	ASSERT_EQ(grid->entries.size(), 5u);
	EXPECT_EQ(grid->entries[0].value, "3");
	EXPECT_EQ(grid->entries[3].name, "10");
	EXPECT_EQ(grid->entries[4].name, "name");
	core::Log::invalidate();
}

TEST(VisualScriptNodes, GroupsAndFunctionFoldersRoundTrip) {
	VisualScript script;
	script.groups.push_back({.id = core::UUID{7},
							 .title = "Movement",
							 .position = {10.f, 20.f},
							 .size = {300.f, 150.f},
							 .color = "Red"});
	script.functions.push_back({.name = "speed",
								.inputs = {},
								.outputs = {{.name = "value", .type = PinType::Number}},
								.macro = false,
								.exits = {"Then"},
								.exported = false,
								.category = "Physics",
								.pure = true});
	const auto read = deserialize(serialize(script), "groups.owlvs");
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->groups, script.groups);
	EXPECT_EQ(read->functions, script.functions);
	const GraphNode inside{.id = core::UUID{1}, .type = "math.add", .position = {50.f, 60.f}, .values = {}};
	const GraphNode outside{.id = core::UUID{2}, .type = "math.add", .position = {400.f, 60.f}, .values = {}};
	EXPECT_TRUE(script.groups.front().holds(inside));
	EXPECT_FALSE(script.groups.front().holds(outside));
}

TEST(VisualScriptNodes, PureFunctionsAreCalledWhereTheirResultIsRead) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addVariable(script, "result", ScriptPropertyType::Float);
	script.functions.push_back({.name = "twice",
								.inputs = {{.name = "x", .type = PinType::Number}},
								.outputs = {{.name = "y", .type = PinType::Number}},
								.macro = false,
								.exits = {"Then"},
								.exported = false,
								.category = {},
								.pure = true});
	const auto entry = addNode(script, "func.entry", {{"function", "twice"}});
	const auto multiply = addNode(script, "math.multiply", {{"B", "2"}});
	const auto ret = addNode(script, "func.return", {{"function", "twice"}});
	link(script, entry, "Then", ret, "Exec");
	link(script, entry, "x", multiply, "A");
	link(script, multiply, "Result", ret, "y");
	auto last = addNode(script, "event.on_create");
	const auto call = addNode(script, "func.pure", {{"function", "twice"}, {"x", "21"}});
	EXPECT_TRUE(std::ranges::none_of(getNodePins(script, *script.findNode(call)).outputs,
									 [](const PinSpec& iPin) -> bool { return iPin.type == PinType::Exec; }));
	setFrom(script, last, "result", call, "y");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 42.f, 1e-6f);
	auto mismatched = script;
	mismatched.functions.front().pure = false;
	EXPECT_FALSE(compile(mismatched).has_value());
	core::Log::invalidate();
}

TEST(VisualScriptNodes, RerouteTakesItsSourceTypeAndFeedsSeveralInputs) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addVariable(script, "first", ScriptPropertyType::Float);
	addVariable(script, "second", ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto square = addNode(script, "math.square", {{"A", "4"}});
	const auto reroute = addNode(script, std::string{g_RerouteNode});
	const auto chained = addNode(script, std::string{g_RerouteNode});
	EXPECT_EQ(getRerouteType(script, *script.findNode(chained)), PinType::Any);
	link(script, square, "Result", reroute, "In");
	link(script, reroute, "Out", chained, "In");
	EXPECT_EQ(getNodePins(script, *script.findNode(chained)).outputs.front().type, PinType::Number);
	setFrom(script, last, "first", reroute, "Out");
	setFrom(script, last, "second", chained, "Out");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("first").value_or(0.f), 16.f, 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("second").value_or(0.f), 16.f, 1e-4f);
	core::Log::invalidate();
}

TEST(VisualScriptNodes, CheckTypesFindsTheMistakesOfHandWrittenLua) {
	core::Log::init(core::Log::Level::Off);
	ScriptTypeChecker checker;
	VisualScript script;
	addVariable(script, "result", ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto sum = addNode(script, "math.add", {{"A", "1"}, {"B", "2"}});
	setFrom(script, last, "result", sum, "Result");
	EXPECT_TRUE(checkTypes(script, checker).empty());
	// The compiler checks every wire and field; the Lua of a custom node only the type checker reads.
	script.customNodes.push_back({.name = "save_level",
								  .title = "Save Level",
								  .inputs = {},
								  .outputs = {},
								  .code = "gamestat.set(\"level\", 1)",
								  .pure = false});
	const auto custom = addNode(script, "custom.call", {{"custom", "save_level"}});
	link(script, last, "Then", custom, "Exec");
	const auto found = checkTypes(script, checker);
	ASSERT_EQ(found.size(), 1u) << compile(script).value_or("");
	EXPECT_TRUE(found.front().message.starts_with("Lua line ")) << found.front().message;
	EXPECT_TRUE(found.front().message.contains("gamestat")) << found.front().message;
	core::Log::invalidate();
}

TEST(VisualScriptNodes, VectorNodesCompute) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addVariable(script, "y", ScriptPropertyType::Float);
	addVariable(script, "length", ScriptPropertyType::Float);
	addVariable(script, "cross", ScriptPropertyType::Float);
	auto last = addNode(script, "event.on_create");
	const auto a = addNode(script, "value.vector3", {{"Value", "4, 7, 3"}});
	const auto b = addNode(script, "value.vector3", {{"Value", "1, 3, 3"}});
	const auto difference = addNode(script, "math.vector3_subtract");
	link(script, a, "Result", difference, "A");
	link(script, b, "Result", difference, "B");
	const auto parts = addNode(script, "math.break_vector3");
	link(script, difference, "Result", parts, "Value");
	setFrom(script, last, "y", parts, "Y");
	const auto length = addNode(script, "math.vector3_length");
	link(script, difference, "Result", length, "A");
	setFrom(script, last, "length", length, "Result");
	const auto cross = addNode(script, "math.vector3_cross");
	const auto x = addNode(script, "value.vector3", {{"Value", "1, 0, 0"}});
	const auto y = addNode(script, "value.vector3", {{"Value", "0, 1, 0"}});
	link(script, x, "Result", cross, "A");
	link(script, y, "Result", cross, "B");
	const auto crossParts = addNode(script, "math.break_vector3");
	link(script, cross, "Result", crossParts, "Value");
	setFrom(script, last, "cross", crossParts, "Z");
	const ScriptInstance instance;
	ASSERT_TRUE(load(instance, script));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("y").value_or(0.f), 4.f, 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("length").value_or(0.f), 5.f, 1e-4f);
	EXPECT_NEAR(instance.getPropertyFloat("cross").value_or(0.f), 1.f, 1e-4f);
	core::Log::invalidate();
}
