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
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
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
	ioTypes.push_back(event("on_collision_exit", "On Collision Exit", "Runs when the body stops touching another body.",
							{other}));
	ioTypes.push_back(event("on_trigger_enter", "On Trigger Enter",
							"Runs when an entity enters the trigger or the sensor body.", {other}));
	ioTypes.push_back(event("on_trigger_exit", "On Trigger Exit",
							"Runs when an entity leaves the trigger or the sensor body.", {other}));
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
	ioTypes.push_back({.id = "flow.for_each",
					   .title = "For Each",
					   .category = "Flow",
					   .description = "Runs Body for each item of a list, in order, then Completed.",
					   .kind = NodeKind::ForEach,
					   .lua = {},
					   .inputs = {execIn(), pin("List", PinType::Table)},
					   .outputs = {execOut("Body"), pin("Index", PinType::Integer), pin("Item", PinType::Any),
								   execOut("Completed")},
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
	ioTypes.push_back(
			pureOp("value.nil", "Nil", "Constant", "No value: clears a variable or a field.", "nil", {}, PinType::Any));
	ioTypes.push_back(constant("value.number", "Number", "A number.", PinType::Number, "0"));
	ioTypes.push_back(constant("value.integer", "Integer", "An integer.", PinType::Integer, "0"));
	ioTypes.push_back(constant("value.boolean", "Boolean", "A boolean.", PinType::Boolean, "false"));
	ioTypes.push_back(constant("value.string", "String", "A string.", PinType::String));
	ioTypes.push_back(constant("value.vector2", "Vector 2", "A 2D vector.", PinType::Vector2));
	ioTypes.push_back(constant("value.vector3", "Vector 3", "A 3D vector.", PinType::Vector3));
	ioTypes.push_back(constant("value.color", "Color", "A colour.", PinType::Color));
	ioTypes.push_back(constant("value.table", "Table",
							   "A table: `a, b` is a list, `key = value` a field; text in quotes keeps its commas.",
							   PinType::Table));
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
	ioTypes.push_back(pureOp(
			"logic.select", "Select", "Logic", "One of two values, depending on the condition.",
			"(if {0} then {1} else {2})",
			{pin("Condition", boolean, "false"), pin("True", PinType::Any), pin("False", PinType::Any)}, PinType::Any));
	ioTypes.push_back(pureOp("logic.default", "Or Default", "Logic",
							 "The value, or the default when it is nil or false.", "({0} or {1})",
							 {pin("Value", PinType::Any), pin("Default", PinType::Any)}, PinType::Any));
	ioTypes.push_back(pureOp("logic.is_set", "Is Set", "Logic", "Whether a value is not nil.", "({0} ~= nil)",
							 {pin("Value", PinType::Any)}, boolean));
	ioTypes.push_back(pureOp("text.concat", "Concatenate", "Text", "Two values joined as text.",
							 "(tostring({0}) .. tostring({1}))", {pin("A", PinType::Any), pin("B", PinType::Any)},
							 PinType::String));
}

void addMath(std::vector<NodeType>& ioTypes) {
	constexpr auto num = PinType::Number;
	constexpr auto integer = PinType::Integer;
	const auto unary = [&ioTypes](const std::string& iId, const std::string& iTitle, const std::string& iDescription,
								  const std::string& iFunction, const PinType iResult) -> void {
		ioTypes.push_back(
				pureOp(iId, iTitle, "Math", iDescription, iFunction + "({0})", {pin("A", num, "0")}, iResult));
	};
	unary("math.sqrt", "Square Root", "Square root of a number.", "math.sqrt", num);
	unary("math.ceil", "Ceil", "Smallest integer above a number.", "math.ceil", integer);
	unary("math.round", "Round", "Nearest integer, halves away from zero.", "math.round", integer);
	unary("math.sin", "Sine", "Sine of an angle in radians.", "math.sin", num);
	unary("math.cos", "Cosine", "Cosine of an angle in radians.", "math.cos", num);
	ioTypes.push_back(pureOp("math.power", "Power", "Math", "A number raised to a power.", "math.pow({0}, {1})",
							 {pin("A", num, "0"), pin("Exponent", num, "2")}, num));
	ioTypes.push_back(pureOp("math.atan2", "Atan2", "Math", "Angle in radians of the direction (X, Y).",
							 "math.atan({0}, {1})", {pin("Y", num, "0"), pin("X", num, "1")}, num));
	ioTypes.push_back(pureOp("math.clamp", "Clamp", "Math", "A number kept between a minimum and a maximum.",
							 "math.max({1}, math.min({2}, {0}))",
							 {pin("Value", num, "0"), pin("Min", num, "0"), pin("Max", num, "1")}, num));
	ioTypes.push_back(pureOp("math.lerp", "Lerp", "Math", "From A to B by Alpha: A at 0, B at 1.",
							 "{0} + ({1} - {0}) * {2}",
							 {pin("A", num, "0"), pin("B", num, "1"), pin("Alpha", num, "0")}, num));
	ioTypes.push_back(pureOp("math.pi", "Pi", "Math", "Half a turn, in radians.", "math.pi", {}, num));
	ioTypes.push_back(pureOp("math.random", "Random", "Math",
							 "A number from 0 to 1 of the scene random stream, drawn where it is read.",
							 "math.random()", {}, num));
	ioTypes.push_back(pureOp("math.random_integer", "Random Integer", "Math",
							 "An integer from Min to Max of the scene random stream, drawn where it is read.",
							 "math.random({0}, {1})", {pin("Min", integer, "1"), pin("Max", integer, "6")}, integer));
}

void addText(std::vector<NodeType>& ioTypes) {
	constexpr auto text = PinType::String;
	ioTypes.push_back(
			pureOp("text.format", "Format", "Text",
				   "Text with its `%s`, `%d`, `%.2f` replaced by the values, in order (`string.format`).",
				   "string.format({0}, {1}, {2}, {3}, {4})",
				   {pin("Format", text, "%s"), pin("A", PinType::Any, {}, true), pin("B", PinType::Any, {}, true),
					pin("C", PinType::Any, {}, true), pin("D", PinType::Any, {}, true)},
				   text));
	ioTypes.push_back(pureOp("text.join", "Join Text", "Text", "Two texts one after the other.", "({0} .. {1})",
							 {pin("A", text), pin("B", text)}, text));
	ioTypes.push_back(pureOp("text.length", "Text Length", "Text", "Number of bytes of a text.", "#{0}",
							 {pin("Text", text)}, PinType::Integer));
	ioTypes.push_back(pureOp("text.split", "Split", "Text", "The parts of a text between the separators, as a list.",
							 "string.split({0}, {1})", {pin("Text", text), pin("Separator", text, ",")},
							 PinType::Table));
	ioTypes.push_back(pureOp("text.to_number", "To Number", "Text", "Number a text spells, nil when it spells none.",
							 "tonumber({0})", {pin("Text", text)}, PinType::Number));
	ioTypes.push_back(pureOp("text.to_text", "To Text", "Text", "Any value as text.", "tostring({0})",
							 {pin("Value", PinType::Any)}, text));
	ioTypes.push_back(pureOp("text.contains", "Contains Text", "Text", "Whether a text holds another, as written.",
							 "string.find({0}, {1}, 1, true) ~= nil", {pin("Text", text), pin("Search", text)},
							 PinType::Boolean));
	ioTypes.push_back(pureOp(
			"text.substring", "Substring", "Text",
			"The characters of a text from First to Last, from 1; negative counts from the end.",
			"string.sub({0}, {1}, {2})",
			{pin("Text", text), pin("First", PinType::Integer, "1"), pin("Last", PinType::Integer, "-1")}, text));
	ioTypes.push_back(pureOp("text.upper", "Upper Case", "Text", "A text in capitals.", "string.upper({0})",
							 {pin("Text", text)}, text));
	ioTypes.push_back(pureOp("text.lower", "Lower Case", "Text", "A text in small letters.", "string.lower({0})",
							 {pin("Text", text)}, text));
}

// An execution node running one Lua statement of its inputs.
auto statement(const std::string& iId, const std::string& iTitle, const std::string& iDescription,
			   const std::string& iLua, std::vector<PinSpec> iInputs) -> NodeType {
	NodeType type{.id = iId,
				  .title = iTitle,
				  .category = "Table",
				  .description = iDescription,
				  .kind = NodeKind::Statement,
				  .lua = iLua,
				  .inputs = {execIn()},
				  .outputs = {execOut()},
				  .pure = false};
	for (auto& input: iInputs) type.inputs.push_back(std::move(input));
	return type;
}

void addTables(std::vector<NodeType>& ioTypes) {
	constexpr auto table = PinType::Table;
	constexpr auto any = PinType::Any;
	ioTypes.push_back(pureOp("table.get", "Get Field", "Table", "Value of a key of a table, nil when it has none.",
							 "{0}[{1}]", {pin("Table", table), pin("Key", any)}, any));
	ioTypes.push_back(pureOp("table.has_key", "Has Key", "Table", "Whether a table holds a value at a key.",
							 "{0}[{1}] ~= nil", {pin("Table", table), pin("Key", any)}, PinType::Boolean));
	ioTypes.push_back(pureOp("table.get_item", "Get Item", "Table", "Item of a list at an index, from 1.", "{0}[{1}]",
							 {pin("List", table), pin("Index", PinType::Integer, "1")}, any));
	ioTypes.push_back(pureOp("table.length", "List Length", "Table", "Number of items of a list.", "#{0}",
							 {pin("List", table)}, PinType::Integer));
	ioTypes.push_back(pureOp("table.contains", "Contains", "Table", "Whether a list holds a value.",
							 "table.find({0}, {1}) ~= nil", {pin("List", table), pin("Value", any)}, PinType::Boolean));
	ioTypes.push_back(
			pureOp("table.make_list", "Make List", "Table", "A new list of the values given, in order.",
				   "{ {0}, {1}, {2}, {3} }",
				   {pin("A", any, {}, true), pin("B", any, {}, true), pin("C", any, {}, true), pin("D", any, {}, true)},
				   table));
	ioTypes.push_back(statement("table.set", "Set Field", "Change the value of a key of a table.", "{0}[{1}] = {2}",
								{pin("Table", table), pin("Key", any), pin("Value", any)}));
	ioTypes.push_back(statement("table.append", "Add Item", "Add a value at the end of a list.",
								"table.insert({0}, {1})", {pin("List", table), pin("Value", any)}));
	ioTypes.push_back(statement("table.remove", "Remove Item", "Remove the item of a list at an index, from 1.",
								"table.remove({0}, {1})", {pin("List", table), pin("Index", PinType::Integer, "1")}));
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

// The run of required (or optional) number pins from iFirst named `<p>x, <p>y [, <p>z]` or `r, g, b [, a]`.
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
			if (pin.type != first.type || pin.optional != first.optional || pin.name.size() != prefix.size() + 1 ||
				!pin.name.starts_with(prefix) || pin.name.back() != iLetters[i])
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

// The pins of a custom node and of a Lua function come from their definition; a pure one has no execution pins.
void addScripted(std::vector<NodeType>& ioTypes) {
	const auto add = [&ioTypes](std::string iId, std::string iTitle, std::string iCategory, std::string iDescription,
								const NodeKind iKind, const bool iPure) -> void {
		ioTypes.push_back({.id = std::move(iId),
						   .title = std::move(iTitle),
						   .category = std::move(iCategory),
						   .description = std::move(iDescription),
						   .kind = iKind,
						   .lua = {},
						   .inputs = iPure ? std::vector<PinSpec>{} : std::vector{execIn()},
						   .outputs = iPure ? std::vector<PinSpec>{} : std::vector{execOut()},
						   .pure = iPure});
	};
	add("custom.call", "Custom Node", "Custom", "Run the Lua of a custom node of this graph.", NodeKind::CustomCall,
		false);
	add("custom.pure", "Custom Value", "Custom", "Compute the outputs of a pure custom node of this graph.",
		NodeKind::CustomCall, true);
	add("lua.call", "Lua Function", "Lua", "Run an annotated function of a Lua module.", NodeKind::LuaCall, false);
	add("lua.pure", "Lua Value", "Lua", "Compute the results of a pure annotated function of a Lua module.",
		NodeKind::LuaCall, true);
}

auto buildNodeTypes() -> std::vector<NodeType> {
	std::vector<NodeType> types;
	addEvents(types);
	addFlow(types);
	addFunctions(types);
	addScripted(types);
	addValues(types);
	addOperators(types);
	addMath(types);
	addText(types);
	addTables(types);
	addCompounds(types);
	for (auto& type: getBindingNodeTypes()) types.push_back(std::move(type));
	return types;
}


auto isNumeric(const PinType iType) -> bool { return iType == PinType::Integer || iType == PinType::Number; }

void emitParameters(YAML::Emitter& ioOut, const char* iKey, const std::vector<GraphParameter>& iParameters) {
	ioOut << YAML::Key << iKey << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const auto& parameter: iParameters)
		ioOut << YAML::BeginMap << YAML::Key << "name" << YAML::Value << parameter.name << YAML::Key << "type"
			  << YAML::Value << std::string{getPinTypeKey(parameter.type)} << YAML::EndMap;
	ioOut << YAML::EndSeq;
}

void emitCustomNodes(YAML::Emitter& ioOut, const std::vector<CustomNode>& iNodes) {
	if (iNodes.empty())
		return;
	ioOut << YAML::Key << "customNodes" << YAML::Value << YAML::BeginSeq;
	for (const auto& custom: iNodes) {
		ioOut << YAML::BeginMap;
		ioOut << YAML::Key << "name" << YAML::Value << custom.name;
		ioOut << YAML::Key << "title" << YAML::Value << custom.title;
		emitParameters(ioOut, "inputs", custom.inputs);
		emitParameters(ioOut, "outputs", custom.outputs);
		if (custom.pure)
			ioOut << YAML::Key << "pure" << YAML::Value << true;
		ioOut << YAML::Key << "code" << YAML::Value << YAML::Literal << custom.code;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

void emitScript(YAML::Emitter& ioOut, const VisualScript& iScript) {
	ioOut << YAML::Key << "variables" << YAML::Value << YAML::BeginSeq;
	for (const auto& variable: iScript.variables) {
		ioOut << YAML::Flow << YAML::BeginMap;
		ioOut << YAML::Key << "name" << YAML::Value << variable.name;
		ioOut << YAML::Key << "type" << YAML::Value << std::string{ScriptEngine::propertyTypeName(variable.type)};
		ioOut << YAML::Key << "default" << YAML::Value << variable.defaultValue;
		for (const auto& [key, bound]:
			 {std::pair{"min", variable.min}, std::pair{"max", variable.max}, std::pair{"step", variable.step}})
			if (bound.has_value())
				ioOut << YAML::Key << key << YAML::Value << *bound;
		if (!variable.exposed)
			ioOut << YAML::Key << "exposed" << YAML::Value << false;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
	if (!iScript.functions.empty()) {
		ioOut << YAML::Key << "functions" << YAML::Value << YAML::BeginSeq;
		for (const auto& function: iScript.functions) {
			ioOut << YAML::BeginMap;
			ioOut << YAML::Key << "name" << YAML::Value << function.name;
			emitParameters(ioOut, "inputs", function.inputs);
			emitParameters(ioOut, "outputs", function.outputs);
			if (function.macro) {
				ioOut << YAML::Key << "macro" << YAML::Value << true;
				ioOut << YAML::Key << "exits" << YAML::Value << YAML::Flow << function.exits;
			}
			if (function.exported)
				ioOut << YAML::Key << "exported" << YAML::Value << true;
			ioOut << YAML::EndMap;
		}
		ioOut << YAML::EndSeq;
	}
	emitCustomNodes(ioOut, iScript.customNodes);
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
		if (!link.waypoints.empty()) {
			ioOut << YAML::Key << "waypoints" << YAML::Value << YAML::BeginSeq;
			for (const auto& [x, y]: link.waypoints) ioOut << YAML::BeginSeq << x << y << YAML::EndSeq;
			ioOut << YAML::EndSeq;
		}
		if (!link.color.empty())
			ioOut << YAML::Key << "color" << YAML::Value << link.color;
		ioOut << YAML::EndMap;
	}
	ioOut << YAML::EndSeq;
}

auto optionalFloat(const YAML::Node& iNode) -> std::optional<float> {
	if (!iNode)
		return std::nullopt;
	return iNode.as<float>();
}

auto parseVariable(const YAML::Node& iNode) -> std::optional<GraphVariable> {
	if (!iNode.IsMap() || !iNode["name"])
		return std::nullopt;
	GraphVariable variable{.name = iNode["name"].as<std::string>(),
						   .type = ScriptPropertyType::Float,
						   .defaultValue = iNode["default"].as<std::string>(""),
						   .min = optionalFloat(iNode["min"]),
						   .max = optionalFloat(iNode["max"]),
						   .step = optionalFloat(iNode["step"]),
						   .exposed = iNode["exposed"].as<bool>(true)};
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
						   .exits = {"Then"},
						   .exported = iNode["exported"].as<bool>(false)};
	if (const auto exits = iNode["exits"]; function.macro && exits && exits.IsSequence() && exits.size() > 0)
		function.exits = exits.as<std::vector<std::string>>();
	return function;
}

// The literal block of the code ends with the line break YAML keeps: the body never needs it.
auto parseCustomNode(const YAML::Node& iNode) -> std::optional<CustomNode> {
	if (!iNode.IsMap() || !iNode["name"])
		return std::nullopt;
	auto custom = CustomNode{.name = iNode["name"].as<std::string>(),
							 .title = iNode["title"].as<std::string>(""),
							 .inputs = parseParameters(iNode["inputs"]),
							 .outputs = parseParameters(iNode["outputs"]),
							 .code = iNode["code"].as<std::string>(""),
							 .pure = iNode["pure"].as<bool>(false)};
	while (custom.code.ends_with('\n')) custom.code.pop_back();
	return custom;
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

auto parseWaypoints(const YAML::Node& iLink) -> std::vector<std::array<float, 2>> {
	std::vector<std::array<float, 2>> waypoints;
	for (const auto& point: iLink["waypoints"])
		if (point.IsSequence() && point.size() == 2)
			waypoints.push_back({point[0].as<float>(), point[1].as<float>()});
	return waypoints;
}

auto parseLink(const YAML::Node& iNode) -> std::optional<GraphLink> {
	if (!iNode.IsMap() || !iNode["from"] || !iNode["to"])
		return std::nullopt;
	return GraphLink{.fromNode = core::UUID{iNode["from"].as<uint64_t>()},
					 .fromPin = iNode["fromPin"].as<std::string>(""),
					 .toNode = core::UUID{iNode["to"].as<uint64_t>()},
					 .toPin = iNode["toPin"].as<std::string>(""),
					 .waypoints = parseWaypoints(iNode),
					 .color = iNode["color"].as<std::string>("")};
}

auto parseScript(const YAML::Node& iRoot) -> VisualScript {
	VisualScript script;
	for (const auto& entry: iRoot["variables"])
		if (auto variable = parseVariable(entry); variable.has_value())
			script.variables.push_back(std::move(*variable));
	for (const auto& entry: iRoot["functions"])
		if (auto function = parseFunction(entry); function.has_value())
			script.functions.push_back(std::move(*function));
	for (const auto& entry: iRoot["customNodes"])
		if (auto custom = parseCustomNode(entry); custom.has_value())
			script.customNodes.push_back(std::move(*custom));
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
	std::map<std::string, std::optional<std::vector<LuaFunction>>, std::less<>> modules;
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

auto trimmed(std::string_view iText) -> std::string_view {
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.front())) != 0) iText.remove_prefix(1);
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.back())) != 0) iText.remove_suffix(1);
	return iText;
}

auto isIdentifierChar(const char iChar) -> bool {
	return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
}

// Splits off the first word of a text: up to the first space.
auto nextWord(std::string_view& ioText) -> std::string_view {
	ioText = trimmed(ioText);
	const auto end = std::min(ioText.find_first_of(" \t"), ioText.size());
	const auto word = ioText.substr(0, end);
	ioText.remove_prefix(end);
	return word;
}

// A Lua type of an annotation (`number`, `vector3?`, `string|nil`) as a pin type; `Any` for one graphs do not know.
auto annotationType(std::string_view iType) -> PinType {
	if (iType.ends_with('?'))
		iType.remove_suffix(1);
	if (iType.ends_with("|nil"))
		iType.remove_suffix(4);
	const auto* const found = std::ranges::find(g_pinTypeKeys, iType, &std::pair<PinType, std::string_view>::second);
	if (found == g_pinTypeKeys.end() || found->first == PinType::Exec)
		return PinType::Any;
	return found->first;
}

void annotate(LuaFunction& ioFunction, bool& ioAnnotated, std::string_view iText) {
	if (!iText.starts_with('@')) {
		if (!iText.empty())
			ioFunction.description += ioFunction.description.empty() ? std::string{iText} : " " + std::string{iText};
		return;
	}
	const auto tag = nextWord(iText);
	if (tag == "@pure") {
		ioFunction.pure = true;
	} else if (tag == "@param") {
		auto name = nextWord(iText);
		if (name.ends_with('?'))
			name.remove_suffix(1);
		ioFunction.inputs.push_back({.name = std::string{name}, .type = annotationType(nextWord(iText))});
	} else if (tag == "@return") {
		const auto type = annotationType(nextWord(iText));
		auto name = std::string{nextWord(iText)};
		if (name.empty() || !std::ranges::all_of(name, isIdentifierChar))
			name = ioFunction.outputs.empty() ? "Result" : std::format("Result{}", ioFunction.outputs.size() + 1);
		ioFunction.outputs.push_back({.name = std::move(name), .type = type});
	} else {
		return;
	}
	ioAnnotated = true;
}

// The name of `function M.name(`, the only form a graph calls: a field of the table the module returns.
auto moduleFunctionName(std::string_view iLine) -> std::optional<std::string> {
	if (!iLine.starts_with("function "))
		return std::nullopt;
	iLine = trimmed(iLine.substr(9));
	const auto dot = iLine.find('.');
	const auto paren = iLine.find('(');
	if (dot == std::string_view::npos || paren == std::string_view::npos || dot == 0 || paren < dot)
		return std::nullopt;
	const auto table = iLine.substr(0, dot);
	const auto name = trimmed(iLine.substr(dot + 1, paren - dot - 1));
	if (name.empty() || !std::ranges::all_of(table, isIdentifierChar) || !std::ranges::all_of(name, isIdentifierChar))
		return std::nullopt;
	return std::string{name};
}

void appendPins(std::vector<PinSpec>& ioPins, const std::vector<GraphParameter>& iParameters) {
	for (const auto& parameter: iParameters) ioPins.push_back(pin(parameter.name, parameter.type));
}

}// namespace

void setLibraryReader(LibraryReader iReader) {
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	cache.reader = std::move(iReader);
	cache.graphs.clear();
	cache.modules.clear();
}

void clearLibraryCache() {
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	cache.graphs.clear();
	cache.modules.clear();
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

auto findNodeCustom(const VisualScript& iScript, const GraphNode& iNode) -> const CustomNode* {
	const auto name = iNode.values.find("custom");
	return name == iNode.values.end() ? nullptr : iScript.findCustomNode(name->second);
}

auto parseLuaFunctions(const std::string_view iSource) -> std::vector<LuaFunction> {
	std::vector<LuaFunction> functions;
	LuaFunction pending;
	bool annotated = false;
	for (const auto range: std::views::split(iSource, '\n')) {
		const auto text = trimmed(std::string_view{range.begin(), range.end()});
		if (text.starts_with("---")) {
			annotate(pending, annotated, trimmed(text.substr(3)));
			continue;
		}
		if (annotated)
			if (auto name = moduleFunctionName(text); name.has_value()) {
				pending.name = std::move(*name);
				functions.push_back(std::move(pending));
			}
		pending = LuaFunction{};
		annotated = false;
	}
	return functions;
}

auto getLuaModulePath(const std::string_view iModule) -> std::string {
	std::string path{iModule};
	std::ranges::replace(path, '.', '/');
	return path + ".lua";
}

auto findLuaModule(const std::string_view iModule) -> const std::vector<LuaFunction>* {
	if (iModule.empty())
		return nullptr;
	auto& cache = libraryCache();
	const std::scoped_lock<std::mutex> lock{cache.mutex};
	if (const auto found = cache.modules.find(iModule); found != cache.modules.end())
		return found->second.has_value() ? &*found->second : nullptr;
	std::optional<std::vector<LuaFunction>> functions;
	const auto path = getLuaModulePath(iModule);
	if (const auto text = cache.reader ? cache.reader(path) : readLibraryText(path); text.has_value())
		functions = parseLuaFunctions(*text);
	else
		OWL_CORE_WARN("VisualScript: The Lua module '{}' is in no asset folder.", iModule)
	const auto [it, inserted] = cache.modules.emplace(std::string{iModule}, std::move(functions));
	return it->second.has_value() ? &*it->second : nullptr;
}

auto findNodeLuaFunction(const GraphNode& iNode) -> const LuaFunction* {
	const auto module = iNode.values.find("module");
	const auto name = iNode.values.find("function");
	if (module == iNode.values.end() || name == iNode.values.end())
		return nullptr;
	const auto* functions = findLuaModule(module->second);
	if (functions == nullptr)
		return nullptr;
	const auto found = std::ranges::find(*functions, name->second, &LuaFunction::name);
	return found == functions->end() ? nullptr : &*found;
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

auto VisualScript::findCustomNode(const std::string_view iName) const -> const CustomNode* {
	const auto it = std::ranges::find(customNodes, iName, &CustomNode::name);
	return it == customNodes.end() ? nullptr : &*it;
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
	if (type->kind == NodeKind::CustomCall || type->kind == NodeKind::LuaCall) {
		const auto* custom = type->kind == NodeKind::CustomCall ? findNodeCustom(iScript, iNode) : nullptr;
		const auto* function = type->kind == NodeKind::LuaCall ? findNodeLuaFunction(iNode) : nullptr;
		if (custom == nullptr && function == nullptr)
			return pins;
		appendPins(pins.inputs, custom != nullptr ? custom->inputs : function->inputs);
		appendPins(pins.outputs, custom != nullptr ? custom->outputs : function->outputs);
		return pins;
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
