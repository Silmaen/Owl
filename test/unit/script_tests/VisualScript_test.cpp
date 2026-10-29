/**
 * @file VisualScript_test.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <core/Log.h>
#include <scene/Scene.h>
#include <script/ScriptEngine.h>
#include <script/ScriptInstance.h>
#include <script/visual/VisualScript.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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

// Loads a graph as `loadScriptInstance` does: the declared properties get their default.
auto load(ScriptInstance& ioInstance, const VisualScript& iScript, const std::string& iName) -> bool {
	if (!ioInstance.createFromBuffer(toBytes(serialize(iScript)), iName, 0))
		return false;
	for (const auto& property: ioInstance.getDeclaredProperties()) ioInstance.setProperty(property);
	return true;
}

// on_update: counter = counter + dt.
auto accumulator() -> VisualScript {
	VisualScript script;
	script.variables.push_back({.name = "counter", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const auto update = addNode(script, "event.on_update");
	const auto set = addNode(script, "var.set", {{"variable", "counter"}});
	const auto get = addNode(script, "var.get", {{"variable", "counter"}});
	const auto add = addNode(script, "math.add");
	link(script, update, "Then", set, "Exec");
	link(script, get, "Value", add, "A");
	link(script, update, "dt", add, "B");
	link(script, add, "Result", set, "Value");
	return script;
}

auto errorsOf(const VisualScript& iScript) -> std::vector<CompileDiagnostic> {
	const auto result = compile(iScript);
	return result ? std::vector<CompileDiagnostic>{} : result.error();
}

}// namespace

TEST(VisualScript, PaletteHoldsBuiltInsAndEveryBinding) {
	const auto& types = getNodeTypes();
	EXPECT_NE(findNodeType("event.on_update"), nullptr);
	EXPECT_NE(findNodeType("flow.branch"), nullptr);
	const auto* getPosition = findNodeType("call.transform.get_position");
	ASSERT_NE(getPosition, nullptr);
	EXPECT_TRUE(getPosition->pure);
	EXPECT_EQ(getPosition->outputs.size(), 3u);
	const auto* setPosition = findNodeType("call.transform.set_position");
	ASSERT_NE(setPosition, nullptr);
	EXPECT_FALSE(setPosition->pure);
	EXPECT_EQ(setPosition->inputs.front().type, PinType::Exec);
	EXPECT_TRUE(std::ranges::all_of(types, [](const NodeType& iType) -> bool { return !iType.id.empty(); }));
}

TEST(VisualScript, LinkRules) {
	EXPECT_TRUE(canConnect(PinType::Exec, PinType::Exec));
	EXPECT_FALSE(canConnect(PinType::Exec, PinType::Number));
	EXPECT_TRUE(canConnect(PinType::Integer, PinType::Number));
	EXPECT_FALSE(canConnect(PinType::Number, PinType::Integer));
	EXPECT_TRUE(canConnect(PinType::Entity, PinType::Integer));
	EXPECT_TRUE(canConnect(PinType::String, PinType::Any));
	EXPECT_FALSE(canConnect(PinType::String, PinType::Boolean));
}

TEST(VisualScript, NamesMustBeSafeLuaIdentifiers) {
	EXPECT_TRUE(isValidName("speed"));
	EXPECT_TRUE(isValidName("jump_force2"));
	EXPECT_FALSE(isValidName(""));
	EXPECT_FALSE(isValidName("2fast"));
	EXPECT_FALSE(isValidName("end"));
	EXPECT_FALSE(isValidName("__vs"));
	EXPECT_FALSE(isValidName("transform"));
	EXPECT_FALSE(isValidName("entity_id"));
	EXPECT_FALSE(isValidName("a b"));
}

TEST(VisualScript, SerializationRoundTrip) {
	auto script = accumulator();
	script.nodes.front().position = {12.5f, -3.f};
	script.links.front().waypoints = {{40.f, 20.f}, {80.f, -10.5f}};
	const auto text = serialize(script);
	const auto loaded = deserialize(text, "round_trip.owlvs");
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(*loaded, script);
	EXPECT_EQ(deserialize("not: [valid", "bad.owlvs").error(), VisualScriptError::InvalidYaml);
	EXPECT_EQ(deserialize("Other: {}", "other.owlvs").error(), VisualScriptError::InvalidContent);
}

TEST(VisualScript, VariablePinsTakeTheVariableType) {
	auto script = accumulator();
	const auto pins = getNodePins(script, script.nodes[2]);
	ASSERT_EQ(pins.outputs.size(), 1u);
	EXPECT_EQ(pins.outputs.front().type, PinType::Number);
}

TEST(VisualScript, CompiledGraphRunsAndExposesProperties) {
	core::Log::init(core::Log::Level::Off);
	const auto bytes = toBytes(serialize(accumulator()));
	const auto properties = ScriptEngine::extractPropertiesFromBuffer(bytes, "acc.owlvs");
	ASSERT_EQ(properties.size(), 1u);
	EXPECT_EQ(properties.front().name, "counter");
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, accumulator(), "acc.owlvs"));
	instance.onUpdate(0.25f);
	instance.onUpdate(0.5f);
	EXPECT_NEAR(instance.getPropertyFloat("counter").value_or(0.f), 0.75f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScript, BranchLoopAndCustomEvent) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "total", .type = ScriptPropertyType::Int, .defaultValue = "0"});
	script.variables.push_back({.name = "big", .type = ScriptPropertyType::Bool, .defaultValue = "false"});
	const auto custom = addNode(script, "event.custom", {{"function", "add_numbers"}});
	const auto loop = addNode(script, "flow.for", {{"First", "1"}, {"Last", "4"}});
	const auto set = addNode(script, "var.set", {{"variable", "total"}});
	const auto get = addNode(script, "var.get", {{"variable", "total"}});
	const auto add = addNode(script, "math.add");
	const auto floor = addNode(script, "math.floor");
	const auto branch = addNode(script, "flow.branch");
	const auto greater = addNode(script, "compare.greater", {{"B", "5"}});
	const auto setBig = addNode(script, "var.set", {{"variable", "big"}, {"Value", "true"}});
	link(script, custom, "Then", loop, "Exec");
	link(script, loop, "Body", set, "Exec");
	link(script, get, "Value", add, "A");
	link(script, loop, "Index", add, "B");
	link(script, add, "Result", floor, "A");
	link(script, floor, "Result", set, "Value");
	link(script, loop, "Completed", branch, "Exec");
	link(script, get, "Value", greater, "A");
	link(script, greater, "Result", branch, "Condition");
	link(script, branch, "True", setBig, "Exec");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << (lua ? "" : lua.error().front().message);
	ScriptInstance instance;
	ASSERT_TRUE(load(instance, script, "loop.owlvs"));
	ASSERT_TRUE(instance.callFunction("add_numbers"));
	EXPECT_EQ(instance.getPropertyInt("total").value_or(0), 10);
	EXPECT_TRUE(instance.getPropertyBool("big").value_or(false));
	core::Log::invalidate();
}

TEST(VisualScript, StringValuesCannotInjectCode) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "text", .type = ScriptPropertyType::String, .defaultValue = ""});
	const auto create = addNode(script, "event.on_create");
	const auto set = addNode(script, "var.set", {{"variable", "text"}, {"Value", "\"\n text = 'hacked' --\\ \t]]"}});
	link(script, create, "Then", set, "Exec");
	ScriptInstance instance;
	ASSERT_TRUE(load(instance, script, "inject.owlvs"));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyString("text").value_or(""), "\"\n text = 'hacked' --\\ \t]]");
	core::Log::invalidate();
}

TEST(VisualScript, NegativeNumbersStayNumbers) {
	VisualScript script;
	script.variables.push_back({.name = "value", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const auto create = addNode(script, "event.on_create");
	const auto set = addNode(script, "var.set", {{"variable", "value"}});
	const auto sub = addNode(script, "math.subtract", {{"A", "1"}, {"B", "-2"}});
	link(script, create, "Then", set, "Exec");
	link(script, sub, "Result", set, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value());
	EXPECT_TRUE(lua->contains("(1 - (-2))"));
}

TEST(VisualScript, UnsetOptionalArgumentsAreLeftOut) {
	VisualScript script;
	const auto create = addNode(script, "event.on_create");
	const auto transition = addNode(script, "call.scene.transition_to", {{"scene_path", "a.owl"}, {"kind", "fade"}});
	link(script, create, "Then", transition, "Exec");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value());
	EXPECT_TRUE(lua->contains("scene.transition_to(\"a.owl\", \"fade\")\n"));
}

TEST(VisualScript, NumberComponentsFormOneGroupedPin) {
	VisualScript script;
	const auto set = addNode(script, "call.transform.set_position");
	const auto get = addNode(script, "call.transform.get_position");
	const auto pins = getNodePins(script, *script.findNode(set));
	ASSERT_EQ(pins.inputs.size(), 3u);
	EXPECT_EQ(pins.inputs[2].name, "xyz");
	EXPECT_EQ(pins.inputs[2].type, PinType::Vector3);
	EXPECT_EQ(pins.inputs[2].group, "xyz");
	EXPECT_EQ(getNodePins(script, *script.findNode(get)).outputs.front().type, PinType::Vector3);
	script.findNode(set)->values[std::string{g_SplitPrefix} + "xyz"] = "true";
	const auto split = getNodePins(script, *script.findNode(set));
	ASSERT_EQ(split.inputs.size(), 5u);
	EXPECT_EQ(split.inputs[4].name, "z");
	EXPECT_EQ(split.inputs[4].type, PinType::Number);
	EXPECT_EQ(split.inputs[4].group, "xyz");
	EXPECT_EQ(getPinComponents(PinType::Color).size(), 4u);
	EXPECT_TRUE(getPinComponents(PinType::Number).empty());
	EXPECT_FALSE(canConnect(PinType::Vector3, PinType::Number));
}

TEST(VisualScript, PrefixedComponentsFormANamedGroup) {
	VisualScript script;
	const auto set = addNode(script, "call.physics.set_velocity", {{"velocity", "1.5, -2"}});
	const auto pins = getNodePins(script, *script.findNode(set));
	const auto velocity = std::ranges::find(pins.inputs, std::string{"velocity"}, &PinSpec::name);
	ASSERT_NE(velocity, pins.inputs.end());
	EXPECT_EQ(velocity->type, PinType::Vector2);
	EXPECT_EQ(velocity->components, (std::vector<std::string>{"vx", "vy"}));
	const auto create = addNode(script, "event.on_create");
	link(script, create, "Then", set, "Exec");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("physics.set_velocity(entity_id, 1.5, (-2))")) << *lua;
	const auto cell = addNode(script, "call.isometric.pick_cell");
	const auto outputs = getNodePins(script, *script.findNode(cell)).outputs;
	EXPECT_TRUE(std::ranges::any_of(
			outputs, [](const PinSpec& iPin) -> bool { return iPin.name == "cell" && iPin.type == PinType::Vector2; }));
}

TEST(VisualScript, ConstantsHaveOnlyAnOutput) {
	const auto* number = findNodeType("value.number");
	ASSERT_NE(number, nullptr);
	EXPECT_EQ(number->category, "Constant");
	ASSERT_EQ(number->inputs.size(), 1u);
	EXPECT_TRUE(number->inputs.front().field);
	VisualScript script;
	script.variables.push_back({.name = "speed", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const auto constant = addNode(script, "value.number", {{"Value", "4"}});
	const auto other = addNode(script, "value.number", {{"Value", "9"}});
	const auto create = addNode(script, "event.on_create");
	const auto set = addNode(script, "var.set", {{"variable", "speed"}});
	link(script, create, "Then", set, "Exec");
	link(script, other, "Result", constant, "Value");
	link(script, constant, "Result", set, "Value");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("= 4\n")) << *lua;
	EXPECT_FALSE(lua->contains("= 9\n")) << *lua;
}

TEST(VisualScript, GroupedPinsPassOneArgumentPerComponent) {
	VisualScript script;
	const auto create = addNode(script, "event.on_create");
	const auto get = addNode(script, "call.transform.get_position");
	const auto first = addNode(script, "call.transform.set_position");
	const auto second = addNode(script, "call.transform.set_position", {{"xyz", "1, -2, 3.5"}});
	link(script, create, "Then", first, "Exec");
	link(script, first, "Then", second, "Exec");
	link(script, get, "xyz", first, "xyz");
	const auto lua = compile(script);
	ASSERT_TRUE(lua.has_value()) << lua.error().front().message;
	EXPECT_TRUE(lua->contains("= { x = ")) << *lua;
	EXPECT_TRUE(lua->contains(".x, ")) << *lua;
	EXPECT_TRUE(lua->contains("transform.set_position(entity_id, 1, (-2), 3.5)")) << *lua;
	script.findNode(second)->values["xyz"] = "1, 2";
	EXPECT_FALSE(compile(script).has_value());
}

TEST(VisualScript, ColorVariablesAndCompoundNodesRun) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "tint", .type = ScriptPropertyType::Color, .defaultValue = "0.25, 0.5, 1, 1"});
	script.variables.push_back({.name = "green", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	script.variables.push_back({.name = "depth", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const auto create = addNode(script, "event.on_create");
	const auto setGreen = addNode(script, "var.set", {{"variable", "green"}});
	const auto setDepth = addNode(script, "var.set", {{"variable", "depth"}});
	const auto tint = addNode(script, "var.get", {{"variable", "tint"}});
	const auto split = addNode(script, "math.break_color");
	const auto make = addNode(script, "math.make_vector3", {{"Z", "7"}});
	const auto parts = addNode(script, "math.break_vector3");
	link(script, create, "Then", setGreen, "Exec");
	link(script, setGreen, "Then", setDepth, "Exec");
	link(script, tint, "Value", split, "Value");
	link(script, split, "G", setGreen, "Value");
	link(script, make, "Result", parts, "Value");
	link(script, parts, "Z", setDepth, "Value");
	const auto bytes = toBytes(serialize(script));
	const auto properties = ScriptEngine::extractPropertiesFromBuffer(bytes, "tint.owlvs");
	ASSERT_EQ(properties.size(), 3u);
	EXPECT_EQ(properties.front().type, ScriptPropertyType::Color);
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "tint.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("green").value_or(0.f), 0.5f, 1e-5f);
	EXPECT_NEAR(instance.getPropertyFloat("depth").value_or(0.f), 7.f, 1e-5f);
	core::Log::invalidate();
}

TEST(VisualScript, ErrorsPointAtTheFaultyNode) {
	VisualScript unknown;
	const auto bad = addNode(unknown, "no.such.node");
	ASSERT_EQ(errorsOf(unknown).size(), 1u);
	EXPECT_EQ(errorsOf(unknown).front().node, bad);

	VisualScript twice;
	addNode(twice, "event.on_create");
	addNode(twice, "event.on_create");
	EXPECT_EQ(errorsOf(twice).size(), 1u);

	VisualScript loop;
	const auto create = addNode(loop, "event.on_create");
	const auto first = addNode(loop, "flow.sequence");
	const auto second = addNode(loop, "flow.sequence");
	link(loop, create, "Then", first, "Exec");
	link(loop, first, "Then 1", second, "Exec");
	link(loop, second, "Then 1", first, "Exec");
	ASSERT_FALSE(errorsOf(loop).empty());
	EXPECT_EQ(errorsOf(loop).front().node, first);

	VisualScript literal;
	const auto start = addNode(literal, "event.on_create");
	const auto log = addNode(literal, "call.time.set_scale", {{"scale", "fast"}});
	link(literal, start, "Then", log, "Exec");
	ASSERT_EQ(errorsOf(literal).size(), 1u);
	EXPECT_EQ(errorsOf(literal).front().node, log);

	VisualScript badName;
	badName.variables.push_back({.name = "end", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	EXPECT_EQ(errorsOf(badName).size(), 1u);

	VisualScript missingVariable;
	const auto begin = addNode(missingVariable, "event.on_create");
	const auto set = addNode(missingVariable, "var.set", {{"variable", "ghost"}});
	link(missingVariable, begin, "Then", set, "Exec");
	EXPECT_EQ(errorsOf(missingVariable).front().node, set);
}

TEST(VisualScript, InvalidGraphFailsToLoad) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	addNode(script, "no.such.node");
	const ScriptInstance instance;
	EXPECT_FALSE(instance.createFromBuffer(toBytes(serialize(script)), "broken.owlvs", 0));
	EXPECT_FALSE(isVisualScriptPath("player.lua"));
	EXPECT_TRUE(isVisualScriptPath("scripts/player.owlvs"));
	core::Log::invalidate();
}

TEST(VisualScript, EngineDemoGraphsCompile) {
	core::Log::init(core::Log::Level::Off);
	size_t graphs = 0;
	for (const auto& entry: std::filesystem::recursive_directory_iterator(test::getRootPath() / "engine_demo")) {
		if (!entry.is_regular_file() || entry.path().extension() != g_VisualScriptExtension)
			continue;
		++graphs;
		std::ifstream file(entry.path());
		const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const auto script = deserialize(text, entry.path().string());
		ASSERT_TRUE(script.has_value()) << entry.path();
		const auto lua = compile(*script);
		EXPECT_TRUE(lua.has_value()) << entry.path() << ": " << (lua ? "" : lua.error().front().message);
	}
	EXPECT_GT(graphs, 0u);
	core::Log::invalidate();
}

namespace {

// double(x) -> y = x * 2, called from on_create into `result`.
auto doubler() -> VisualScript {
	VisualScript script;
	script.variables.push_back({.name = "result", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	script.functions.push_back({.name = "double",
								.inputs = {{.name = "x", .type = PinType::Number}},
								.outputs = {{.name = "y", .type = PinType::Number}},
								.macro = false,
								.exits = {"Then"}});
	const auto entry = addNode(script, "func.entry", {{"function", "double"}});
	const auto times = addNode(script, "math.multiply", {{"B", "2"}});
	const auto ret = addNode(script, "func.return", {{"function", "double"}});
	link(script, entry, "Then", ret, "Exec");
	link(script, entry, "x", times, "A");
	link(script, times, "Result", ret, "y");
	const auto create = addNode(script, "event.on_create");
	const auto call = addNode(script, "func.call", {{"function", "double"}, {"x", "21"}});
	const auto set = addNode(script, "var.set", {{"variable", "result"}});
	link(script, create, "Then", call, "Exec");
	link(script, call, "Then", set, "Exec");
	link(script, call, "y", set, "Value");
	return script;
}

}// namespace

TEST(VisualScript, FunctionsTakeParametersAndReturnResults) {
	core::Log::init(core::Log::Level::Off);
	const auto script = doubler();
	const auto pins = getNodePins(script, script.nodes[4]);
	ASSERT_EQ(pins.inputs.size(), 2u);
	EXPECT_EQ(pins.inputs[1].name, "x");
	ASSERT_EQ(pins.outputs.size(), 2u);
	EXPECT_EQ(pins.outputs[0].type, PinType::Exec);
	EXPECT_EQ(pins.outputs[1].name, "y");
	ASSERT_TRUE(errorsOf(script).empty()) << errorsOf(script).front().message;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "double.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 42.f, 1e-5f);
	const auto back = deserialize(serialize(script), "double.owlvs");
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(*back, script);
	core::Log::invalidate();
}

TEST(VisualScript, MacrosContinueByTheExitTheirReturnNames) {
	core::Log::init(core::Log::Level::Off);
	VisualScript script;
	script.variables.push_back({.name = "label", .type = ScriptPropertyType::String, .defaultValue = ""});
	script.functions.push_back({.name = "classify",
								.inputs = {{.name = "x", .type = PinType::Number}},
								.outputs = {},
								.macro = true,
								.exits = {"Big", "Small"}});
	const auto entry = addNode(script, "func.entry", {{"function", "classify"}});
	const auto less = addNode(script, "compare.less", {{"B", "10"}});
	const auto branch = addNode(script, "flow.branch");
	const auto small = addNode(script, "func.return", {{"function", "classify"}, {"exit", "Small"}});
	const auto big = addNode(script, "func.return", {{"function", "classify"}, {"exit", "Big"}});
	link(script, entry, "Then", branch, "Exec");
	link(script, entry, "x", less, "A");
	link(script, less, "Result", branch, "Condition");
	link(script, branch, "True", small, "Exec");
	link(script, branch, "False", big, "Exec");
	const auto create = addNode(script, "event.on_create");
	const auto call = addNode(script, "func.call", {{"function", "classify"}, {"x", "5"}});
	const auto setBig = addNode(script, "var.set", {{"variable", "label"}, {"Value", "big"}});
	const auto setSmall = addNode(script, "var.set", {{"variable", "label"}, {"Value", "small"}});
	link(script, create, "Then", call, "Exec");
	link(script, call, "Big", setBig, "Exec");
	link(script, call, "Small", setSmall, "Exec");
	ASSERT_TRUE(errorsOf(script).empty()) << errorsOf(script).front().message;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "classify.owlvs"));
	instance.onCreate();
	EXPECT_EQ(instance.getPropertyString("label").value_or(""), "small");
	core::Log::invalidate();
}

TEST(VisualScript, LibraryFunctionsCompileIntoTheCaller) {
	core::Log::init(core::Log::Level::Off);
	auto library = doubler();
	std::erase_if(library.nodes, [](const GraphNode& iNode) -> bool { return static_cast<uint64_t>(iNode.id) > 3; });
	std::erase_if(library.links,
				  [](const GraphLink& iLink) -> bool { return static_cast<uint64_t>(iLink.toNode) > 3; });
	library.variables.clear();
	const auto libraryText = serialize(library);
	setLibraryReader([&libraryText](const std::string_view iPath) -> std::optional<std::string> {
		if (iPath == "scripts/lib/maths.owlvs")
			return libraryText;
		return std::nullopt;
	});
	VisualScript script;
	script.variables.push_back({.name = "result", .type = ScriptPropertyType::Float, .defaultValue = "0"});
	const auto create = addNode(script, "event.on_create");
	const auto call =
			addNode(script, "func.call", {{"function", "double"}, {"library", "scripts/lib/maths.owlvs"}, {"x", "4"}});
	const auto set = addNode(script, "var.set", {{"variable", "result"}});
	link(script, create, "Then", call, "Exec");
	link(script, call, "Then", set, "Exec");
	link(script, call, "y", set, "Value");
	ASSERT_NE(findNodeFunction(script, *script.findNode(call)), nullptr);
	ASSERT_TRUE(errorsOf(script).empty()) << errorsOf(script).front().message;
	auto scn = mkShared<scene::Scene>();
	ScriptInstance instance;
	instance.setScene(scn.get());
	ASSERT_TRUE(load(instance, script, "caller.owlvs"));
	instance.onCreate();
	EXPECT_NEAR(instance.getPropertyFloat("result").value_or(0.f), 8.f, 1e-5f);

	auto missing = script;
	missing.findNode(call)->values["library"] = "scripts/lib/none.owlvs";
	EXPECT_EQ(errorsOf(missing).size(), 1u);
	setLibraryReader({});
	core::Log::invalidate();
}

TEST(VisualScript, FunctionNodesReportTheirMisuse) {
	auto script = doubler();
	auto outside = script;
	link(outside, outside.nodes[3].id, "Then", outside.nodes[2].id, "Exec");
	std::erase_if(outside.links, [&](const GraphLink& iLink) -> bool {
		return iLink.fromNode == outside.nodes[3].id && iLink.toNode == outside.nodes[4].id;
	});
	EXPECT_FALSE(errorsOf(outside).empty());
	auto unknown = script;
	unknown.nodes[4].values["function"] = "nope";
	EXPECT_FALSE(errorsOf(unknown).empty());
	auto noEntry = script;
	noEntry.removeNode(noEntry.nodes[0].id);
	EXPECT_FALSE(errorsOf(noEntry).empty());
	EXPECT_EQ(parsePinTypeKey(getPinTypeKey(PinType::Vector3)), PinType::Vector3);
	EXPECT_FALSE(parsePinTypeKey("matrix").has_value());
}
