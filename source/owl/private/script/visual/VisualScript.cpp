/**
 * @file VisualScript.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "script/visual/VisualScript.h"

#include "BindingNodes.h"
#include "core/FormatVersionYaml.h"
#include "core/external/yaml.h"
#include "data/assets/AssetSearchPaths.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::script::visual {

namespace {

auto pin(std::string iName, const PinType iType, std::string iDefault = {}, const bool iOptional = false) -> PinSpec {
	return {.name = std::move(iName),
			.type = iType,
			.defaultValue = std::move(iDefault),
			.optional = iOptional,
			.group = {},
			.components = {},
			.field = false};
}

constexpr std::array<core::MigrationStep, 0> g_visualScriptMigrations{};
constexpr core::DocumentFormat g_visualScriptFormat{.name = "VisualScript", .migrations = g_visualScriptMigrations};

auto execIn() -> PinSpec { return pin("Exec", PinType::Exec); }

auto execOut(const std::string& iName = "Then") -> PinSpec { return pin(iName, PinType::Exec); }

auto event(const std::string& iHook, const std::string& iTitle, const std::string& iDescription,
		   std::vector<PinSpec> iParams = {}) -> NodeType {
	NodeType type{.id = "event." + iHook,
				  .title = iTitle,
				  .category = "Events",
				  .description = iDescription,
				  .kind = NodeKind::Event,
				  .lua = iHook,
				  .inputs = {},
				  .outputs = {execOut()},
				  .pure = false};
	for (auto& param: iParams) type.outputs.push_back(std::move(param));
	return type;
}

auto pureOp(const std::string& iId, const std::string& iTitle, const std::string& iCategory,
			const std::string& iDescription, const std::string& iExpression, std::vector<PinSpec> iInputs,
			const PinType iResult) -> NodeType {
	return {.id = iId,
			.title = iTitle,
			.category = iCategory,
			.description = iDescription,
			.kind = NodeKind::Operator,
			.lua = iExpression,
			.inputs = std::move(iInputs),
			.outputs = {pin("Result", iResult)},
			.pure = true};
}

auto binaryOp(const std::string& iId, const std::string& iTitle, const std::string& iCategory,
			  const std::string& iExpression, const PinType iOperand, const PinType iResult) -> NodeType {
	return pureOp(iId, iTitle, iCategory, iTitle + " of two values.", iExpression,
				  {pin("A", iOperand, iOperand == PinType::Boolean ? "false" : "0"),
				   pin("B", iOperand, iOperand == PinType::Boolean ? "false" : "0")},
				  iResult);
}

void addEvents(std::vector<NodeType>& ioTypes) {
	const PinSpec deltaTime = pin("dt", PinType::Number);
	const PinSpec other = pin("other_id", PinType::Entity);
	ioTypes.push_back(event("on_create", "On Create", "Runs once when the scene enters Play."));
	ioTypes.push_back(event("on_update", "On Update", "Runs every frame, with the frame time.", {deltaTime}));
	ioTypes.push_back(
			event("on_tick", "On Tick", "Runs every fixed simulation tick, with the tick time.", {deltaTime}));
	ioTypes.push_back(event("on_destroy", "On Destroy", "Runs when the entity is destroyed or Play ends."));
	ioTypes.push_back(
			event("on_collision", "On Collision", "Runs when the body starts touching another body.", {other}));
	ioTypes.push_back(
			event("on_trigger_enter", "On Trigger Enter", "Runs when an entity enters the trigger.", {other}));
	ioTypes.push_back(event("on_trigger_exit", "On Trigger Exit", "Runs when an entity leaves the trigger.", {other}));
	ioTypes.push_back(event("on_triggered", "On Triggered", "Runs when the trigger fires.", {other}));
	ioTypes.push_back(event("on_interact", "On Interact", "Runs when the player interacts with the trigger."));
	ioTypes.push_back(event("on_timer", "On Timer", "Runs when the timer of the trigger elapses."));
	ioTypes.push_back({.id = "event.custom",
					   .title = "Custom Event",
					   .category = "Events",
					   .description = "Lua function of a chosen name, for a trigger or HUD callback.",
					   .kind = NodeKind::CustomEvent,
					   .lua = {},
					   .inputs = {},
					   .outputs = {execOut(), pin("Arg", PinType::Any)},
					   .pure = false});
}

void addFlow(std::vector<NodeType>& ioTypes) {
	ioTypes.push_back({.id = "flow.branch",
					   .title = "Branch",
					   .category = "Flow",
					   .description = "Runs True or False depending on the condition.",
					   .kind = NodeKind::Branch,
					   .lua = {},
					   .inputs = {execIn(), pin("Condition", PinType::Boolean, "false")},
					   .outputs = {execOut("True"), execOut("False")},
					   .pure = false});
	ioTypes.push_back({.id = "flow.sequence",
					   .title = "Sequence",
					   .category = "Flow",
					   .description = "Runs each output in order.",
					   .kind = NodeKind::Sequence,
					   .lua = {},
					   .inputs = {execIn()},
					   .outputs = {execOut("Then 1"), execOut("Then 2"), execOut("Then 3")},
					   .pure = false});
	ioTypes.push_back({.id = "flow.for",
					   .title = "For Loop",
					   .category = "Flow",
					   .description = "Runs Body for each integer from First to Last, then Completed.",
					   .kind = NodeKind::ForLoop,
					   .lua = {},
					   .inputs = {execIn(), pin("First", PinType::Integer, "1"), pin("Last", PinType::Integer, "1")},
					   .outputs = {execOut("Body"), pin("Index", PinType::Integer), execOut("Completed")},
					   .pure = false});
}

// A constant: its value typed on the node, its only pin an output.
auto constant(const std::string& iId, const std::string& iTitle, const std::string& iDescription, const PinType iType,
			  std::string iDefault = {}) -> NodeType {
	auto value = pin("Value", iType, std::move(iDefault));
	value.field = true;
	return pureOp(iId, iTitle, "Constant", iDescription, "{0}", {std::move(value)}, iType);
}

void addValues(std::vector<NodeType>& ioTypes) {
	ioTypes.push_back(pureOp("value.self", "Self", "Constant", "The entity running the script.", "entity_id", {},
							 PinType::Entity));
	ioTypes.push_back(constant("value.number", "Number", "A number.", PinType::Number, "0"));
	ioTypes.push_back(constant("value.integer", "Integer", "An integer.", PinType::Integer, "0"));
	ioTypes.push_back(constant("value.boolean", "Boolean", "A boolean.", PinType::Boolean, "false"));
	ioTypes.push_back(constant("value.string", "String", "A string.", PinType::String));
	ioTypes.push_back(constant("value.vector2", "Vector 2", "A 2D vector.", PinType::Vector2));
	ioTypes.push_back(constant("value.vector3", "Vector 3", "A 3D vector.", PinType::Vector3));
	ioTypes.push_back(constant("value.color", "Color", "A colour.", PinType::Color));
	ioTypes.push_back({.id = "var.get",
					   .title = "Get Variable",
					   .category = "Variables",
					   .description = "Value of a graph variable.",
					   .kind = NodeKind::GetVariable,
					   .lua = {},
					   .inputs = {},
					   .outputs = {pin("Value", PinType::Any)},
					   .pure = true});
	ioTypes.push_back({.id = "var.set",
					   .title = "Set Variable",
					   .category = "Variables",
					   .description = "Change the value of a graph variable.",
					   .kind = NodeKind::SetVariable,
					   .lua = {},
					   .inputs = {execIn(), pin("Value", PinType::Any)},
					   .outputs = {execOut(), pin("Value", PinType::Any)},
					   .pure = false});
}

void addOperators(std::vector<NodeType>& ioTypes) {
	constexpr auto num = PinType::Number;
	constexpr auto boolean = PinType::Boolean;
	ioTypes.push_back(binaryOp("math.add", "Add", "Math", "({0} + {1})", num, num));
	ioTypes.push_back(binaryOp("math.subtract", "Subtract", "Math", "({0} - {1})", num, num));
	ioTypes.push_back(binaryOp("math.multiply", "Multiply", "Math", "({0} * {1})", num, num));
	ioTypes.push_back(binaryOp("math.divide", "Divide", "Math", "({0} / {1})", num, num));
	ioTypes.push_back(binaryOp("math.modulo", "Modulo", "Math", "({0} % {1})", num, num));
	ioTypes.push_back(binaryOp("math.min", "Min", "Math", "math.min({0}, {1})", num, num));
	ioTypes.push_back(binaryOp("math.max", "Max", "Math", "math.max({0}, {1})", num, num));
	ioTypes.push_back(
			pureOp("math.negate", "Negate", "Math", "Opposite of a number.", "(- {0})", {pin("A", num, "0")}, num));
	ioTypes.push_back(pureOp("math.floor", "Floor", "Math", "Largest integer below a number.", "math.floor({0})",
							 {pin("A", num, "0")}, PinType::Integer));
	ioTypes.push_back(pureOp("math.abs", "Absolute", "Math", "Absolute value of a number.", "math.abs({0})",
							 {pin("A", num, "0")}, num));
	ioTypes.push_back(binaryOp("compare.equal", "Equal", "Compare", "({0} == {1})", PinType::Any, boolean));
	ioTypes.push_back(binaryOp("compare.not_equal", "Not Equal", "Compare", "({0} ~= {1})", PinType::Any, boolean));
	ioTypes.push_back(binaryOp("compare.less", "Less", "Compare", "({0} < {1})", num, boolean));
	ioTypes.push_back(binaryOp("compare.less_equal", "Less or Equal", "Compare", "({0} <= {1})", num, boolean));
	ioTypes.push_back(binaryOp("compare.greater", "Greater", "Compare", "({0} > {1})", num, boolean));
	ioTypes.push_back(binaryOp("compare.greater_equal", "Greater or Equal", "Compare", "({0} >= {1})", num, boolean));
	ioTypes.push_back(binaryOp("logic.and", "And", "Logic", "({0} and {1})", boolean, boolean));
	ioTypes.push_back(binaryOp("logic.or", "Or", "Logic", "({0} or {1})", boolean, boolean));
	ioTypes.push_back(pureOp("logic.not", "Not", "Logic", "Opposite of a boolean.", "(not {0})",
							 {pin("A", boolean, "false")}, boolean));
	ioTypes.push_back(pureOp("text.concat", "Concatenate", "Text", "Two values joined as text.",
							 "(tostring({0}) .. tostring({1}))", {pin("A", PinType::Any), pin("B", PinType::Any)},
							 PinType::String));
}

void addCompounds(std::vector<NodeType>& ioTypes) {
	constexpr auto num = PinType::Number;
	ioTypes.push_back(pureOp("math.make_vector2", "Make Vector 2", "Vector", "A 2D vector from its components.",
							 "{ x = {0}, y = {1} }", {pin("X", num, "0"), pin("Y", num, "0")}, PinType::Vector2));
	ioTypes.push_back(pureOp("math.make_vector3", "Make Vector 3", "Vector", "A 3D vector from its components.",
							 "{ x = {0}, y = {1}, z = {2} }",
							 {pin("X", num, "0"), pin("Y", num, "0"), pin("Z", num, "0")}, PinType::Vector3));
	ioTypes.push_back(pureOp("math.make_color", "Make Color", "Color", "A colour from its channels, between 0 and 1.",
							 "{ r = {0}, g = {1}, b = {2}, a = {3} }",
							 {pin("R", num, "1"), pin("G", num, "1"), pin("B", num, "1"), pin("A", num, "1")},
							 PinType::Color));
	const auto breakNode = [](const std::string& iId, const std::string& iTitle, const std::string& iCategory,
							  const PinType iType) -> NodeType {
		NodeType type{.id = iId,
					  .title = iTitle,
					  .category = iCategory,
					  .description = "Components of the value.",
					  .kind = NodeKind::Operator,
					  .lua = {},
					  .inputs = {pin("Value", iType)},
					  .outputs = {},
					  .pure = true};
		for (const auto& component: getPinComponents(iType)) {
			if (!type.lua.empty())
				type.lua += ", ";
			type.lua += "{0}." + component;
			type.outputs.push_back(pin(std::string(1, static_cast<char>(std::toupper(component.front()))), num));
		}
		return type;
	};
	ioTypes.push_back(breakNode("math.break_vector2", "Break Vector 2", "Vector", PinType::Vector2));
	ioTypes.push_back(breakNode("math.break_vector3", "Break Vector 3", "Vector", PinType::Vector3));
	ioTypes.push_back(breakNode("math.break_color", "Break Color", "Color", PinType::Color));
}

// A run of pins read as one vector or colour.
struct PinGroup {
	// Number of pins in the run.
	size_t count{0};
	// Type of the grouped pin.
	PinType type{PinType::Vector2};
	// Name of the grouped pin.
	std::string name;
};

// Name of the grouped pin of a prefix: `vx, vy` is the velocity, `cell_x, cell_y` the cell.
auto groupName(const std::string_view iPrefix, const std::string_view iLetters) -> std::string {
	static constexpr std::array<std::pair<std::string_view, std::string_view>, 5> named{
			{{"v", "velocity"}, {"s", "scale"}, {"r", "rotation"}, {"f", "force"}, {"d", "delta"}}};
	if (iPrefix.empty())
		return std::string{iLetters};
	for (const auto& [prefix, name]: named)
		if (prefix == iPrefix)
			return std::string{name};
	return std::string{iPrefix.ends_with('_') ? iPrefix.substr(0, iPrefix.size() - 1) : iPrefix};
}

// The run of number (or integer) pins from iFirst named `<prefix>x, <prefix>y [, <prefix>z]` or `r, g, b [, a]`.
auto findGroup(const std::vector<PinSpec>& iPins, const size_t iFirst) -> std::optional<PinGroup> {
	const auto& first = iPins[iFirst];
	if ((first.type != PinType::Number && first.type != PinType::Integer) || first.name.empty())
		return std::nullopt;
	const std::string_view prefix = std::string_view{first.name}.substr(0, first.name.size() - 1);
	if (!prefix.empty() && !prefix.ends_with('_') && prefix.size() > 1)
		return std::nullopt;
	const auto spells = [&](const std::string_view iLetters) -> bool {
		if (iFirst + iLetters.size() > iPins.size())
			return false;
		for (size_t i = 0; i < iLetters.size(); ++i) {
			const auto& pin = iPins[iFirst + i];
			if (pin.type != first.type || pin.name.size() != prefix.size() + 1 || !pin.name.starts_with(prefix) ||
				pin.name.back() != iLetters[i])
				return false;
		}
		return true;
	};
	if (prefix.empty() && first.type == PinType::Number) {
		if (spells("rgba"))
			return PinGroup{.count = 4, .type = PinType::Color, .name = "rgba"};
		if (spells("rgb"))
			return PinGroup{.count = 3, .type = PinType::Color, .name = "rgb"};
	}
	if (spells("xyz"))
		return PinGroup{.count = 3, .type = PinType::Vector3, .name = groupName(prefix, "xyz")};
	if (spells("xy"))
		return PinGroup{.count = 2, .type = PinType::Vector2, .name = groupName(prefix, "xy")};
	return std::nullopt;
}

// Grouped pins of a binding: each group stays as its components when the node splits it.
auto groupPins(const std::vector<PinSpec>& iPins, const GraphNode& iNode) -> std::vector<PinSpec> {
	std::vector<PinSpec> out;
	out.reserve(iPins.size());
	for (size_t i = 0; i < iPins.size();) {
		const auto group = findGroup(iPins, i);
		if (!group.has_value()) {
			out.push_back(iPins[i++]);
			continue;
		}
		const auto first = iPins.begin() + static_cast<std::ptrdiff_t>(i);
		const auto last = first + static_cast<std::ptrdiff_t>(group->count);
		const auto split = iNode.values.find(std::string{g_SplitPrefix} + group->name);
		if (split != iNode.values.end() && split->second == "true") {
			for (auto it = first; it != last; ++it) {
				out.push_back(*it);
				out.back().group = group->name;
			}
		} else {
			std::vector<std::string> components;
			components.reserve(group->count);
			for (auto it = first; it != last; ++it) components.push_back(it->name);
			out.push_back(
					{.name = group->name,
					 .type = group->type,
					 .defaultValue = {},
					 .optional = std::all_of(first, last, [](const PinSpec& iPin) -> bool { return iPin.optional; }),
					 .group = group->name,
					 .components = std::move(components),
					 .field = false});
		}
		i += group->count;
	}
	return out;
}

void addFunctions(std::vector<NodeType>& ioTypes) {
	ioTypes.push_back({.id = "func.entry",
					   .title = "Function Entry",
					   .category = "Functions",
					   .description = "Start of a function: its parameters.",
					   .kind = NodeKind::FunctionEntry,
					   .lua = {},
					   .inputs = {},
					   .outputs = {execOut()},
					   .pure = false});
	ioTypes.push_back({.id = "func.return",
					   .title = "Return",
					   .category = "Functions",
					   .description = "End of a function: its results, and for a macro the exit the call continues by.",
					   .kind = NodeKind::FunctionReturn,
					   .lua = {},
					   .inputs = {execIn()},
					   .outputs = {},
					   .pure = false});
	ioTypes.push_back({.id = "func.call",
					   .title = "Call Function",
					   .category = "Functions",
					   .description = "Run a function of this graph or of a library graph.",
					   .kind = NodeKind::FunctionCall,
					   .lua = {},
					   .inputs = {execIn()},
					   .outputs = {},
					   .pure = false});
}

auto buildNodeTypes() -> std::vector<NodeType> {
	std::vector<NodeType> types;
	addEvents(types);
	addFlow(types);
	addFunctions(types);
	addValues(types);
	addOperators(types);
	addCompounds(types);
	for (auto& type: getBindingNodeTypes()) types.push_back(std::move(type));
	return types;
}


auto isNumeric(const PinType iType) -> bool { return iType == PinType::Integer || iType == PinType::Number; }

void emitScript(YAML::Emitter& ioOut, const VisualScript& iScript) {
	ioOut << YAML::Key << "variables" << YAML::Value << YAML::BeginSeq;
	for (const auto& variable: iScript.variables) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "name" << YAML::Value << variable.name;
		ioOut << YAML::Key << "type" << YAML::Value << std::string{ScriptEngine::propertyTypeName(variable.type)};
		ioOut << YAML::Key << "default" << YAML::Value << variable.defaultValue;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	if (!iScript.functions.empty()) {
		const auto emitParameters = [&ioOut](const char* iKey, const std::vector<GraphParameter>& iParameters) -> void {
			ioOut << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq;
			for (const auto& parameter: iParameters)
				ioOut << YAML::BeginMap << YAML::Key << "name" << YAML::Value << parameter.name << YAML::Key << "type"
					  << YAML::Value << std::string{getPinTypeKey(parameter.type)} << YAML::EndMap;
			ioOut << YAML::EndSeq;
		};
		ioOut << YAML::Key << "functions" << YAML::Value << YAML::BeginSeq;
		for (const auto& function: iScript.functions) {
			ioOut << YAML::BeginMap;
			ioOut << YAML::Key << "name" << YAML::Value << function.name;
			emitParameters("inputs", function.inputs);
			emitParameters("outputs", function.outputs);
			if (function.macro) {
				ioOut << YAML::Key << "macro" << YAML::Value << true;
				ioOut << YAML::Key << "exits" << YAML::Value << YAML::Flow << function.exits;
			}
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndSeq;
	}
	ioOut << YAML::Key << "nodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& node: iScript.nodes) {
		ioOut << YAML::BeginMap;
		ioOut << YAML::Key << "id" << YAML::Value << static_cast<uint64_t>(node.id);
		ioOut << YAML::Key << "type" << YAML::Value << node.type;
		ioOut << YAML::Key << "position" << YAML::Value << YAML::Flow << YAML::BeginSeq << node.position[0]
			  << node.position[1] << YAML::EndSeq;
		if (!node.values.empty()) {
			ioOut << YAML::Key << "values" << YAML::Value << YAML::BeginMap;
			for (const auto& [key, value]: node.values) ioOut << YAML::Key << key << YAML::Value << value;
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	ioOut << YAML::Key << "links" << YAML::Value << YAML::BeginSeq;
	for (const auto& link: iScript.links) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "from" << YAML::Value << static_cast<uint64_t>(link.fromNode);
		ioOut << YAML::Key << "fromPin" << YAML::Value << link.fromPin;
		ioOut << YAML::Key << "to" << YAML::Value << static_cast<uint64_t>(link.toNode);
		ioOut << YAML::Key << "toPin" << YAML::Value << link.toPin;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

auto parseVariable(const YAML::Node& iNode) -> std::optional<GraphVariable> {
	if (!iNode.IsMap() || !iNode["name"])
		return std::nullopt;
	GraphVariable variable{.name = iNode["name"].as<std::string>(),
						   .type = ScriptPropertyType::Float,
						   .defaultValue = iNode["default"].as<std::string>("")};
	if (const auto type = ScriptEngine::parsePropertyType(iNode["type"].as<std::string>("float")); type.has_value())
		variable.type = *type;
	return variable;
}

auto parseParameters(const YAML::Node& iNode) -> std::vector<GraphParameter> {
	std::vector<GraphParameter> parameters;
	if (!iNode || !iNode.IsSequence())
		return parameters;
	for (const auto& entry: iNode) {
		if (!entry.IsMap() || !entry["name"])
			continue;
		parameters.push_back({.name = entry["name"].as<std::string>(),
							  .type = parsePinTypeKey(entry["type"].as<std::string>("number")).value_or(PinType::Any)});
	}
	return parameters;
}

auto parseFunction(const YAML::Node& iNode) -> std::optional<GraphFunction> {
	if (!iNode.IsMap() || !iNode["name"])
		return std::nullopt;
	GraphFunction function{.name = iNode["name"].as<std::string>(),
						   .inputs = parseParameters(iNode["inputs"]),
						   .outputs = parseParameters(iNode["outputs"]),
						   .macro = iNode["macro"].as<bool>(false),
						   .exits = {"Then"}};
	if (const auto exits = iNode["exits"]; function.macro && exits && exits.IsSequence() && exits.size() > 0)
		function.exits = exits.as<std::vector<std::string>>();
	return function;
}

auto parseNode(const YAML::Node& iNode) -> std::optional<GraphNode> {
	if (!iNode.IsMap() || !iNode["id"] || !iNode["type"])
		return std::nullopt;
	GraphNode node{.id = core::UUID{iNode["id"].as<uint64_t>()},
				   .type = iNode["type"].as<std::string>(),
				   .position = {0.f, 0.f},
				   .values = {}};
	if (const auto pos = iNode["position"]; pos && pos.IsSequence() && pos.size() >= 2)
		node.position = {pos[0].as<float>(), pos[1].as<float>()};
	if (const auto values = iNode["values"]; values && values.IsMap())
		for (const auto& entry: values) node.values[entry.first.as<std::string>()] = entry.second.as<std::string>("");
	return node;
}

auto parseLink(const YAML::Node& iNode) -> std::optional<GraphLink> {
	if (!iNode.IsMap() || !iNode["from"] || !iNode["to"])
		return std::nullopt;
	return GraphLink{.fromNode = core::UUID{iNode["from"].as<uint64_t>()},
					 .fromPin = iNode["fromPin"].as<std::string>(""),
					 .toNode = core::UUID{iNode["to"].as<uint64_t>()},
					 .toPin = iNode["toPin"].as<std::string>("")};
}

auto parseScript(const YAML::Node& iRoot) -> VisualScript {
	VisualScript script;
	for (const auto& entry: iRoot["variables"])
		if (auto variable = parseVariable(entry); variable.has_value())
			script.variables.push_back(std::move(*variable));
	for (const auto& entry: iRoot["functions"])
		if (auto function = parseFunction(entry); function.has_value())
			script.functions.push_back(std::move(*function));
	for (const auto& entry: iRoot["nodes"])
		if (auto node = parseNode(entry); node.has_value())
			script.nodes.push_back(std::move(*node));
	for (const auto& entry: iRoot["links"])
		if (auto link = parseLink(entry); link.has_value())
			script.links.push_back(std::move(*link));
	return script;
}

}// namespace

namespace {

struct LibraryCache {
	std::mutex mutex;
	LibraryReader reader;
	std::map<std::string, std::optional<VisualScript>, std::less<>> graphs;
};

auto libraryCache() -> LibraryCache& {
	static LibraryCache s_cache;
	return s_cache;
}

auto readLibraryText(const std::string_view iPath) -> std::optional<std::string> {
	const auto bytes = data::assets::readAssetBytes(std::filesystem::path{std::string{iPath}});
	if (!bytes.has_value())
		return std::nullopt;
	return std::string{bytes->begin(), bytes->end()};
}

constexpr std::array<std::pair<PinType, std::string_view>, 11> g_pinTypeKeys{{{PinType::Exec, "exec"},
																			  {PinType::Boolean, "boolean"},
																			  {PinType::Integer, "integer"},
																			  {PinType::Number, "number"},
																			  {PinType::String, "string"},
																			  {PinType::Table, "table"},
																			  {PinType::Entity, "entity"},
																			  {PinType::Vector2, "vector2"},
																			  {PinType::Vector3, "vector3"},
																			  {PinType::Color, "color"},
																			  {PinType::Any, "any"}}};

}// namespace

void setLibraryReader(LibraryReader iReader) {
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	cache.reader = std::move(iReader);
	cache.graphs.clear();
}

void clearLibraryCache() {
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	cache.graphs.clear();
}

auto findLibrary(const std::string_view iPath) -> const VisualScript* {
	if (iPath.empty())
		return nullptr;
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	if (const auto found = cache.graphs.find(iPath); found != cache.graphs.end())
		return found->second.has_value() ? &*found->second : nullptr;
	std::optional<VisualScript> graph;
	if (const auto text = cache.reader ? cache.reader(iPath) : readLibraryText(iPath); text.has_value()) {
		if (auto parsed = deserialize(*text, iPath); parsed)
			graph = std::move(*parsed);
		else
			OWL_CORE_WARN("VisualScript: Cannot read the library '{}'.", iPath)
	} else {
		OWL_CORE_WARN("VisualScript: The library '{}' is in no asset folder.", iPath)
	}
	const auto [it, inserted] = cache.graphs.emplace(std::string{iPath}, std::move(graph));
	return it->second.has_value() ? &*it->second : nullptr;
}

auto findNodeFunction(const VisualScript& iScript, const GraphNode& iNode) -> const GraphFunction* {
	const auto name = iNode.values.find("function");
	if (name == iNode.values.end())
		return nullptr;
	if (const auto library = iNode.values.find("library"); library != iNode.values.end() && !library->second.empty()) {
		const auto* graph = findLibrary(library->second);
		return graph == nullptr ? nullptr : graph->findFunction(name->second);
	}
	return iScript.findFunction(name->second);
}

auto getPinTypeKey(const PinType iType) -> std::string_view {
	const auto* const found = std::ranges::find(g_pinTypeKeys, iType, &std::pair<PinType, std::string_view>::first);
	return found == g_pinTypeKeys.end() ? "any" : found->second;
}

auto parsePinTypeKey(const std::string_view iName) -> std::optional<PinType> {
	const auto* const found = std::ranges::find(g_pinTypeKeys, iName, &std::pair<PinType, std::string_view>::second);
	if (found == g_pinTypeKeys.end())
		return std::nullopt;
	return found->first;
}

auto VisualScript::findFunction(const std::string_view iName) const -> const GraphFunction* {
	const auto it = std::ranges::find(functions, iName, &GraphFunction::name);
	return it == functions.end() ? nullptr : &*it;
}

auto VisualScript::findNode(const core::UUID iId) const -> const GraphNode* {
	const auto it = std::ranges::find(nodes, iId, &GraphNode::id);
	return it == nodes.end() ? nullptr : &*it;
}

auto VisualScript::findNode(const core::UUID iId) -> GraphNode* {
	const auto it = std::ranges::find(nodes, iId, &GraphNode::id);
	return it == nodes.end() ? nullptr : &*it;
}

auto VisualScript::findVariable(const std::string_view iName) const -> const GraphVariable* {
	const auto it = std::ranges::find(variables, iName, &GraphVariable::name);
	return it == variables.end() ? nullptr : &*it;
}

void VisualScript::removeNode(const core::UUID iId) {
	std::erase_if(nodes, [iId](const GraphNode& iNode) -> bool { return iNode.id == iId; });
	std::erase_if(links,
				  [iId](const GraphLink& iLink) -> bool { return iLink.fromNode == iId || iLink.toNode == iId; });
}

auto getNodeTypes() -> const std::vector<NodeType>& {
	static const std::vector<NodeType> types = buildNodeTypes();
	return types;
}

auto findNodeType(const std::string_view iId) -> const NodeType* {
	const auto& types = getNodeTypes();
	const auto it = std::ranges::find(types, iId, &NodeType::id);
	return it == types.end() ? nullptr : &*it;
}

auto getNodePins(const VisualScript& iScript, const GraphNode& iNode) -> NodePins {
	const auto* type = findNodeType(iNode.type);
	if (type == nullptr)
		return {};
	NodePins pins{.inputs = type->inputs, .outputs = type->outputs};
	if (type->kind == NodeKind::Call) {
		pins.inputs = groupPins(pins.inputs, iNode);
		pins.outputs = groupPins(pins.outputs, iNode);
	}
	if (type->kind == NodeKind::FunctionEntry || type->kind == NodeKind::FunctionReturn ||
		type->kind == NodeKind::FunctionCall) {
		const auto* function = findNodeFunction(iScript, iNode);
		if (function == nullptr)
			return pins;
		const auto toPins = [](const std::vector<GraphParameter>& iParameters) -> std::vector<PinSpec> {
			std::vector<PinSpec> out;
			out.reserve(iParameters.size());
			for (const auto& parameter: iParameters) out.push_back(pin(parameter.name, parameter.type));
			return out;
		};
		if (type->kind == NodeKind::FunctionEntry) {
			for (auto& spec: toPins(function->inputs)) pins.outputs.push_back(std::move(spec));
		} else if (type->kind == NodeKind::FunctionReturn) {
			for (auto& spec: toPins(function->outputs)) pins.inputs.push_back(std::move(spec));
		} else {
			for (auto& spec: toPins(function->inputs)) pins.inputs.push_back(std::move(spec));
			for (const auto& exit: function->exits) pins.outputs.push_back(execOut(exit));
			for (auto& spec: toPins(function->outputs)) pins.outputs.push_back(std::move(spec));
		}
		return pins;
	}
	if (type->kind == NodeKind::GetVariable || type->kind == NodeKind::SetVariable) {
		const auto name = iNode.values.find("variable");
		const auto* variable = name == iNode.values.end() ? nullptr : iScript.findVariable(name->second);
		const auto pinType = variable == nullptr ? PinType::Any : getVariablePinType(variable->type);
		for (auto& pin: pins.inputs)
			if (pin.type != PinType::Exec)
				pin.type = pinType;
		for (auto& pin: pins.outputs)
			if (pin.type != PinType::Exec)
				pin.type = pinType;
	}
	return pins;
}

auto canConnect(const PinType iFrom, const PinType iTo) -> bool {
	if (iFrom == PinType::Exec || iTo == PinType::Exec)
		return iFrom == iTo;
	if (iFrom == iTo || iFrom == PinType::Any || iTo == PinType::Any)
		return true;
	if (iFrom == PinType::Integer && iTo == PinType::Number)
		return true;
	if (iFrom == PinType::Entity && isNumeric(iTo))
		return true;
	return iFrom == PinType::Integer && iTo == PinType::Entity;
}

auto getVariablePinType(const ScriptPropertyType iType) -> PinType {
	switch (iType) {
		case ScriptPropertyType::Float:
			return PinType::Number;
		case ScriptPropertyType::Int:
			return PinType::Integer;
		case ScriptPropertyType::String:
			return PinType::String;
		case ScriptPropertyType::Bool:
			return PinType::Boolean;
		case ScriptPropertyType::Entity:
			return PinType::Entity;
		case ScriptPropertyType::Color:
			return PinType::Color;
		case ScriptPropertyType::Vec2:
			return PinType::Vector2;
		case ScriptPropertyType::Vec3:
			return PinType::Vector3;
		case ScriptPropertyType::Numbers:
		case ScriptPropertyType::Strings:
			return PinType::Table;
	}
	return PinType::Any;
}

auto getPinComponents(const PinType iType) -> std::vector<std::string> {
	switch (iType) {
		case PinType::Vector2:
			return {"x", "y"};
		case PinType::Vector3:
			return {"x", "y", "z"};
		case PinType::Color:
			return {"r", "g", "b", "a"};
		case PinType::Exec:
		case PinType::Boolean:
		case PinType::Integer:
		case PinType::Number:
		case PinType::String:
		case PinType::Table:
		case PinType::Entity:
		case PinType::Any:
			break;
	}
	return {};
}

auto isValidName(const std::string_view iName) -> bool {
	constexpr std::array<std::string_view, 23> keywords{
			"and", "break", "do",  "else", "elseif", "end",    "false",  "for",  "function", "global", "goto", "if",
			"in",  "local", "nil", "not",  "or",     "repeat", "return", "then", "true",     "until",  "while"};
	constexpr std::array<std::string_view, 20> globals{"entity_id", "properties", "math",     "string",  "table",
													   "utf8",      "coroutine",  "print",    "pairs",   "ipairs",
													   "type",      "tostring",   "tonumber", "select",  "error",
													   "assert",    "pcall",      "next",     "require", "_G"};
	if (iName.empty() || iName.starts_with("__") || std::isdigit(static_cast<unsigned char>(iName.front())) != 0)
		return false;
	if (!std::ranges::all_of(iName, [](const char iChar) -> bool {
			return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
		}))
		return false;
	if (std::ranges::find(keywords, iName) != keywords.end() || std::ranges::find(globals, iName) != globals.end())
		return false;
	return std::ranges::none_of(getNodeTypes(),
								[iName](const NodeType& iType) -> bool { return iType.category == iName; });
}

auto isVisualScriptPath(const std::string_view iPath) -> bool { return iPath.ends_with(g_VisualScriptExtension); }

auto serialize(const VisualScript& iScript) -> std::string {
	YAML::Emitter out;
	out << YAML::BeginMap;
	core::emitFormatVersion(out, g_visualScriptFormat);
	out << YAML::Key << "VisualScript" << YAML::Value << YAML::BeginMap;
	emitScript(out, iScript);
	out << YAML::EndMap;
	out << YAML::EndMap;
	return std::string{out.c_str()} + "\n";
}

auto deserialize(const std::string_view iText, const std::string_view iSourceName)
		-> expected<VisualScript, VisualScriptError> {
	YAML::Node root;
	try {
		root = YAML::Load(std::string{iText});
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("VisualScript: Cannot parse '{}': {}.", iSourceName, iError.what())
		return unexpected{VisualScriptError::InvalidYaml};
	}
	if (!root.IsMap() || !root["VisualScript"] || !root["VisualScript"].IsMap()) {
		OWL_CORE_WARN("VisualScript: '{}' has no VisualScript map.", iSourceName)
		return unexpected{VisualScriptError::InvalidContent};
	}
	if (!core::upgradeYamlDocument(g_visualScriptFormat, root, iSourceName)) {
		OWL_CORE_WARN("VisualScript: Cannot read the format of '{}'.", iSourceName)
		return unexpected{VisualScriptError::UnsupportedVersion};
	}
	try {
		return parseScript(root["VisualScript"]);
	} catch (const std::exception& iError) {
		OWL_CORE_WARN("VisualScript: Invalid content in '{}': {}.", iSourceName, iError.what())
		return unexpected{VisualScriptError::InvalidContent};
	}
}

}// namespace owl::script::visual
