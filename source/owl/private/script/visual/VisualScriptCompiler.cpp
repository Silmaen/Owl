/**
 * @file VisualScriptCompiler.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "script/visual/VisualScript.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::script::visual {

namespace {

// Exec paths that merge are emitted once per path: past this size the graph is refused instead of growing without end.
constexpr size_t g_maxSourceSize = size_t{4} << 20U;

using PinKey = std::pair<uint64_t, std::string>;

auto quote(const std::string_view iText) -> std::string {
	std::string out = "\"";
	for (const char c: iText) {
		switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (static_cast<unsigned char>(c) < 0x20)
					out += std::format("\\{:03}", static_cast<int>(static_cast<unsigned char>(c)));
				else
					out += c;
		}
	}
	return out + "\"";
}

auto parseInteger(const std::string& iText) -> std::optional<int64_t> {
	int64_t value = 0;
	const auto* end = iText.c_str() + iText.size();
	if (const auto [ptr, ec] = std::from_chars(iText.c_str(), end, value); ec != std::errc{} || ptr != end)
		return std::nullopt;
	return value;
}

auto parseNumber(const std::string& iText) -> std::optional<double> {
	double value = 0;
	const auto* end = iText.c_str() + iText.size();
	if (const auto [ptr, ec] = std::from_chars(iText.c_str(), end, value); ec != std::errc{} || ptr != end)
		return std::nullopt;
	if (!std::isfinite(value))
		return std::nullopt;
	return value;
}

// Negative literals are parenthesised: `a - -1` must never become the comment `a--1`.
auto integerLiteral(const int64_t iValue) -> std::string {
	return iValue < 0 ? std::format("({})", iValue) : std::format("{}", iValue);
}

auto numberLiteral(const double iValue) -> std::string {
	return iValue < 0 ? std::format("({})", iValue) : std::format("{}", iValue);
}

// Components of a grouped value typed as `1, 0.5, 0`: one number per component, all present.
auto parseComponents(const std::string& iText, const size_t iCount) -> std::optional<std::vector<double>> {
	std::vector<double> values;
	values.reserve(iCount);
	size_t start = 0;
	while (start <= iText.size()) {
		const auto end = std::min(iText.find(',', start), iText.size());
		auto part = iText.substr(start, end - start);
		std::erase_if(part, [](const char iChar) -> bool { return iChar == ' '; });
		const auto value = parseNumber(part);
		if (!value.has_value())
			return std::nullopt;
		values.push_back(*value);
		start = end + 1;
	}
	if (values.size() != iCount)
		return std::nullopt;
	return values;
}

// A grouped value when the text is empty: opaque white for a colour, the origin for a vector.
auto defaultComponents(const PinType iType, const size_t iCount) -> std::vector<double> {
	return std::vector<double>(iCount, iType == PinType::Color ? 1.0 : 0.0);
}

// Lua table of a grouped value, keyed by component; a colour without alpha is opaque.
auto componentTable(const PinType iType, const std::vector<std::string>& iValues) -> std::string {
	const auto keys = getPinComponents(iType);
	std::string out = "{ ";
	for (size_t i = 0; i < keys.size(); ++i) {
		if (i > 0)
			out += ", ";
		out += std::format("{} = {}", keys[i], i < iValues.size() ? iValues[i] : std::string{"1"});
	}
	return out + " }";
}

auto isGroup(const PinType iType) -> bool { return !getPinComponents(iType).empty(); }

// Table keys of a grouped pin: one per pin it stands for (`x, y` for `vx, vy`), all of its type otherwise.
auto componentsOf(const PinSpec& iPin) -> std::vector<std::string> {
	auto keys = getPinComponents(iPin.type);
	if (iPin.group == iPin.name && !iPin.components.empty() && iPin.components.size() < keys.size())
		keys.resize(iPin.components.size());
	return keys;
}

// NOLINTBEGIN(misc-no-recursion) walks the graph; m_pureStack and m_execStack stop at every cycle
class Compiler final {
public:
	Compiler(const VisualScript& iScript, const CompileMode iMode, std::string iPrefix = "__f_",
			 std::string iSlotTable = "__vs")
		: m_script{iScript}, m_debug{iMode == CompileMode::Debug}, m_prefix{std::move(iPrefix)},
		  m_slotTable{std::move(iSlotTable)}, m_library{m_prefix != "__f_"} {
		for (const auto& link: iScript.links) {
			const auto* from = iScript.findNode(link.fromNode);
			const auto* fromType = from == nullptr ? nullptr : findNodeType(from->type);
			if (fromType != nullptr && isExecOutput(*from, link.fromPin))
				m_execLinks[{link.fromNode, link.fromPin}] = &link;
			else
				m_inputLinks[{link.toNode, link.toPin}] = &link;
		}
	}

	auto run() -> expected<std::string, std::vector<CompileDiagnostic>> {
		emitProperties();
		emitLibraries();
		std::string body;
		std::swap(body, m_out);
		m_lineCount = 0;
		emitFunctions();
		std::set<std::string> functions;
		for (const auto& node: m_script.nodes) {
			const auto* type = findNodeType(node.type);
			if (type == nullptr) {
				error(node.id, std::format("Unknown node type '{}'.", node.type));
				continue;
			}
			if (type->kind == NodeKind::Event || type->kind == NodeKind::CustomEvent)
				emitEvent(node, *type, functions);
		}
		std::string events;
		std::swap(events, m_out);
		m_out = std::move(body);
		line("local __vs = {}");
		if (m_debug)
			line("__owl_probes = __vs");
		const auto offset = static_cast<int>(std::ranges::count(m_out, '\n'));
		for (auto& lines: m_nodeLines | std::views::values)
			for (auto& number: lines) number += offset;
		m_out += events;
		if (m_out.size() > g_maxSourceSize)
			error(0, "The graph expands into too much Lua: merge fewer execution paths into the same nodes.");
		if (!m_errors.empty())
			return unexpected{std::move(m_errors)};
		return std::move(m_out);
	}

	[[nodiscard]] auto getNodeLines() -> std::map<core::UUID, std::vector<int>>& { return m_nodeLines; }

	// A library graph: its functions only, as locals named with the prefix, and the slot table they share.
	auto runLibrary() -> expected<std::string, std::vector<CompileDiagnostic>> {
		line(std::format("local {} = {{}}", m_slotTable));
		emitFunctions();
		if (!m_errors.empty())
			return unexpected{std::move(m_errors)};
		return std::move(m_out);
	}

private:
	const VisualScript& m_script;
	bool m_debug;
	std::string m_prefix;
	std::string m_slotTable;
	bool m_library;
	const GraphFunction* m_function{nullptr};
	std::map<PinKey, std::string> m_localPins;
	std::map<std::string, std::string, std::less<>> m_libraryPrefixes;
	std::map<core::UUID, std::vector<int>> m_nodeLines;
	int m_lineCount{0};
	std::map<PinKey, const GraphLink*> m_execLinks;
	std::map<PinKey, const GraphLink*> m_inputLinks;
	std::map<PinKey, size_t> m_slots;
	std::unordered_map<uint64_t, std::vector<std::string>> m_temps;
	std::set<uint64_t> m_pureStack;
	std::set<uint64_t> m_execStack;
	std::set<uint64_t> m_reported;
	std::vector<CompileDiagnostic> m_errors;
	std::string m_out;
	size_t m_indent{0};
	size_t m_tempCount{0};

	[[nodiscard]] auto isExecOutput(const GraphNode& iNode, const std::string& iPin) const -> bool {
		const auto pins = getNodePins(m_script, iNode);
		return std::ranges::any_of(pins.outputs, [&iPin](const PinSpec& iSpec) -> bool {
			return iSpec.name == iPin && iSpec.type == PinType::Exec;
		});
	}

	void error(const core::UUID iNode, std::string iMessage) {
		if (iNode != 0 && !m_reported.insert(iNode).second)
			return;
		m_errors.push_back({.node = iNode, .message = std::move(iMessage)});
	}

	void line(const std::string_view iText) {
		m_out.append(m_indent, '\t');
		m_out += iText;
		m_out += '\n';
		++m_lineCount;
	}

	void markNode(const core::UUID iNode) {
		if (m_debug)
			m_nodeLines[iNode].push_back(m_lineCount + 1);
	}

	void probe(const core::UUID iNode, const NodePins& iPins, const std::vector<std::string>& iValues) {
		if (!m_debug)
			return;
		for (size_t i = 0; i < iPins.outputs.size() && i < iValues.size(); ++i)
			line(std::format("__vs[{}] = {}", quote(getProbeKey(iNode, iPins.outputs[i].name)), iValues[i]));
	}

	auto slot(const core::UUID iNode, const std::string& iPin) -> std::string {
		if (const auto local = m_localPins.find({iNode, iPin}); local != m_localPins.end())
			return local->second;
		if (m_debug && !m_library)
			return std::format("__vs[{}]", quote(getProbeKey(iNode, iPin)));
		const auto [it, inserted] = m_slots.try_emplace({iNode, iPin}, m_slots.size() + 1);
		return std::format("{}[{}]", m_slotTable, it->second);
	}

	auto literal(const core::UUID iNode, const PinSpec& iPin, const std::string& iValue) -> std::string {
		const auto& text = iValue.empty() ? iPin.defaultValue : iValue;
		if (text.empty() && iPin.optional)
			return "nil";
		switch (iPin.type) {
			case PinType::Exec:
			case PinType::Table:
				return "nil";
			case PinType::Vector2:
			case PinType::Vector3:
			case PinType::Color:
				if (const auto values = groupLiteral(iNode, iPin, iValue); values.has_value())
					return componentTable(iPin.type, *values);
				return "nil";
			case PinType::Boolean:
				if (text == "true" || text == "false")
					return text;
				if (text.empty())
					return "false";
				break;
			case PinType::Integer:
				if (text.empty())
					return "0";
				if (const auto value = parseInteger(text); value.has_value())
					return integerLiteral(*value);
				break;
			case PinType::Number:
				if (text.empty())
					return "0";
				if (const auto value = parseNumber(text); value.has_value())
					return numberLiteral(*value);
				break;
			case PinType::String:
				return quote(text);
			case PinType::Entity:
				if (text.empty() || text == "self")
					return "entity_id";
				if (const auto value = parseInteger(text); value.has_value() && *value >= 0)
					return integerLiteral(*value);
				break;
			case PinType::Any:
				if (text.empty())
					return "nil";
				if (text == "true" || text == "false")
					return text;
				if (const auto value = parseNumber(text); value.has_value())
					return numberLiteral(*value);
				return quote(text);
		}
		error(iNode, std::format("Input '{}' holds '{}', which is not a valid value.", iPin.name, text));
		return "nil";
	}

	// Literal components of a grouped input: its value, or the defaults (nil when optional) when empty.
	auto groupLiteral(const core::UUID iNode, const PinSpec& iPin, const std::string& iValue)
			-> std::optional<std::vector<std::string>> {
		const auto count = componentsOf(iPin).size();
		if (iValue.empty() && iPin.optional)
			return std::vector<std::string>(count, "nil");
		const auto values =
				iValue.empty() ? std::optional{defaultComponents(iPin.type, count)} : parseComponents(iValue, count);
		if (!values.has_value()) {
			error(iNode, std::format("Input '{}' holds '{}', which is not {} numbers.", iPin.name, iValue, count));
			return std::nullopt;
		}
		std::vector<std::string> out;
		out.reserve(values->size());
		for (const auto value: *values) out.push_back(numberLiteral(value));
		return out;
	}

	// The arguments a grouped input of a call passes: one per component.
	auto groupArguments(const GraphNode& iNode, const PinSpec& iPin) -> std::vector<std::string> {
		const auto components = componentsOf(iPin);
		if (!m_inputLinks.contains({iNode.id, iPin.name})) {
			const auto value = iNode.values.find(iPin.name);
			return groupLiteral(iNode.id, iPin, value == iNode.values.end() ? std::string{} : value->second)
					.value_or(std::vector<std::string>(components.size(), "nil"));
		}
		const auto table = inputExpression(iNode, iPin);
		std::vector<std::string> out;
		out.reserve(components.size());
		for (const auto& component: components) out.push_back(std::format("{}.{}", table, component));
		return out;
	}

	// Lua names receiving the results of a call: one per component of a grouped output.
	auto resultNames(const NodePins& iPins, const std::function<std::string(const std::string&)>& iName) const
			-> std::vector<std::string> {
		std::vector<std::string> out;
		for (const auto& pin: iPins.outputs) {
			if (pin.type == PinType::Exec)
				continue;
			if (pin.group == pin.name)
				for (const auto& component: componentsOf(pin)) out.push_back(iName(pin.name + "." + component));
			else
				out.push_back(iName(pin.name));
		}
		return out;
	}

	// Builds the table of each grouped output from its component results.
	void assembleGroups(const NodePins& iPins, const std::function<std::string(const std::string&)>& iName,
						const std::function<std::string(const std::string&)>& iTarget) {
		for (const auto& pin: iPins.outputs) {
			if (pin.group != pin.name || pin.type == PinType::Exec)
				continue;
			std::vector<std::string> values;
			for (const auto& component: componentsOf(pin)) values.push_back(iName(pin.name + "." + component));
			line(std::format("{} = {}", iTarget(pin.name), componentTable(pin.type, values)));
		}
	}

	auto inputExpression(const GraphNode& iNode, const PinSpec& iPin) -> std::string {
		const auto link = m_inputLinks.find({iNode.id, iPin.name});
		if (link == m_inputLinks.end() || iPin.field) {
			const auto value = iNode.values.find(iPin.name);
			return literal(iNode.id, iPin, value == iNode.values.end() ? std::string{} : value->second);
		}
		const auto* source = m_script.findNode(link->second->fromNode);
		const auto* sourceType = source == nullptr ? nullptr : findNodeType(source->type);
		if (sourceType == nullptr) {
			error(iNode.id, std::format("Input '{}' is linked to a missing node.", iPin.name));
			return "nil";
		}
		const auto pins = getNodePins(m_script, *source);
		const auto output = std::ranges::find(pins.outputs, link->second->fromPin, &PinSpec::name);
		if (output == pins.outputs.end()) {
			error(iNode.id, std::format("Input '{}' is linked to a missing pin.", iPin.name));
			return "nil";
		}
		if (!canConnect(output->type, iPin.type)) {
			error(iNode.id, std::format("Input '{}' cannot take the value of '{}'.", iPin.name, output->name));
			return "nil";
		}
		if (!sourceType->pure)
			return slot(source->id, output->name);
		const auto& temps = evaluatePure(*source, *sourceType, pins);
		const auto index = static_cast<size_t>(std::distance(pins.outputs.begin(), output));
		return index < temps.size() ? temps[index] : std::string{"nil"};
	}

	auto arguments(const GraphNode& iNode, const NodePins& iPins) -> std::vector<std::string> {
		std::vector<std::string> args;
		for (const auto& pin: iPins.inputs) {
			if (pin.type == PinType::Exec)
				continue;
			if (pin.group == pin.name && isGroup(pin.type)) {
				for (auto& arg: groupArguments(iNode, pin)) args.push_back(std::move(arg));
			} else {
				args.push_back(inputExpression(iNode, pin));
			}
		}
		return args;
	}

	auto variableName(const GraphNode& iNode) -> std::optional<std::string> {
		const auto name = iNode.values.find("variable");
		if (name == iNode.values.end() || m_script.findVariable(name->second) == nullptr) {
			error(iNode.id, "The node names no variable of the graph.");
			return std::nullopt;
		}
		return name->second;
	}

	auto evaluatePure(const GraphNode& iNode, const NodeType& iType, const NodePins& iPins)
			-> const std::vector<std::string>& {
		if (const auto cached = m_temps.find(iNode.id); cached != m_temps.end())
			return cached->second;
		static const std::vector<std::string> none;
		if (!m_pureStack.insert(iNode.id).second) {
			error(iNode.id, "The node feeds its own inputs.");
			return none;
		}
		const auto args = arguments(iNode, iPins);
		m_pureStack.erase(iNode.id);
		std::vector<std::string> temps;
		temps.reserve(iPins.outputs.size());
		for (size_t i = 0; i < iPins.outputs.size(); ++i) temps.push_back(std::format("__t{}", ++m_tempCount));
		const auto names = joined(temps);
		markNode(iNode.id);
		switch (iType.kind) {
			case NodeKind::Call:
				{
					std::map<std::string, std::string> parts;
					const auto part = [&](const std::string& iKey) -> std::string {
						const auto [it, inserted] = parts.try_emplace(iKey, std::format("__t{}", m_tempCount + 1));
						if (inserted)
							++m_tempCount;
						return it->second;
					};
					const auto results = resultNames(iPins, part);
					line(std::format("local {} = {}({})", joined(results), iType.lua, callArguments(args)));
					std::map<std::string, std::string> groupTemps;
					for (size_t i = 0; i < iPins.outputs.size(); ++i) {
						if (iPins.outputs[i].group == iPins.outputs[i].name)
							groupTemps[iPins.outputs[i].name] = temps[i];
						else
							temps[i] = part(iPins.outputs[i].name);
					}
					assembleGroups(iPins, part,
								   [&](const std::string& iPin) -> std::string { return "local " + groupTemps[iPin]; });
				}
				break;
			case NodeKind::Operator:
				line(std::format("local {} = {}", names, substitute(iType.lua, args)));
				break;
			case NodeKind::GetVariable:
				if (m_library)
					error(iNode.id, "A library function cannot read the variables of its graph.");
				line(std::format("local {} = {}", names, variableName(iNode).value_or("nil")));
				break;
			case NodeKind::Event:
			case NodeKind::CustomEvent:
			case NodeKind::Branch:
			case NodeKind::Sequence:
			case NodeKind::ForLoop:
			case NodeKind::SetVariable:
			case NodeKind::FunctionEntry:
			case NodeKind::FunctionReturn:
			case NodeKind::FunctionCall:
				error(iNode.id, "The node cannot be evaluated as a value.");
				break;
		}
		probe(iNode.id, iPins, temps);
		return m_temps[iNode.id] = std::move(temps);
	}

	// A binding counts its arguments: unset optional parameters at the end are left out, not passed as nil.
	static auto callArguments(std::vector<std::string> iArgs) -> std::string {
		while (!iArgs.empty() && iArgs.back() == "nil") iArgs.pop_back();
		return joined(iArgs);
	}

	static auto joined(const std::vector<std::string>& iParts) -> std::string {
		std::string out;
		for (const auto& part: iParts) {
			if (!out.empty())
				out += ", ";
			out += part;
		}
		return out;
	}

	static auto substitute(const std::string& iTemplate, const std::vector<std::string>& iArgs) -> std::string {
		std::string out;
		for (size_t i = 0; i < iTemplate.size(); ++i) {
			if (iTemplate[i] == '{' && i + 2 < iTemplate.size() && iTemplate[i + 2] == '}') {
				if (const auto index = static_cast<size_t>(iTemplate[i + 1] - '0'); index < iArgs.size()) {
					out += iArgs[index];
					i += 2;
					continue;
				}
			}
			out += iTemplate[i];
		}
		return out;
	}

	// A property default is read by index: `{ 1, 1, 1, 1 }`.
	auto arrayLiteral(const PinSpec& iPin, const std::string& iValue) -> std::string {
		const auto values = groupLiteral(0, iPin, iValue);
		if (!values.has_value())
			return "nil";
		return std::format("{{ {} }}", joined(*values));
	}

	void emitProperties() {
		std::set<std::string> names;
		if (m_script.variables.empty())
			return;
		line("properties = {");
		++m_indent;
		for (const auto& variable: m_script.variables) {
			if (!isValidName(variable.name) || !names.insert(variable.name).second) {
				error(0, std::format("Variable '{}' needs a unique Lua name.", variable.name));
				continue;
			}
			const auto type = getVariablePinType(variable.type);
			if (type == PinType::Table) {
				error(0, std::format("Variable '{}' has a type graphs do not support.", variable.name));
				continue;
			}
			const PinSpec pin{.name = variable.name,
							  .type = type,
							  .defaultValue = {},
							  .optional = false,
							  .group = {},
							  .components = {},
							  .field = false};
			auto value = literal(0, pin, variable.defaultValue);
			if (type == PinType::Entity && value == "entity_id")
				value = "0";
			if (isGroup(type))
				value = arrayLiteral(pin, variable.defaultValue);
			line(std::format("{{ name = {}, type = {}, default = {} }},", quote(variable.name),
							 quote(ScriptEngine::propertyTypeName(variable.type)), value));
		}
		--m_indent;
		line("}");
	}

	void emitEvent(const GraphNode& iNode, const NodeType& iType, std::set<std::string>& ioFunctions) {
		auto function = iType.lua;
		if (iType.kind == NodeKind::CustomEvent) {
			const auto name = iNode.values.find("function");
			function = name == iNode.values.end() ? std::string{} : name->second;
			if (!isValidName(function) || findNodeType("event." + function) != nullptr) {
				error(iNode.id, std::format("'{}' is not a usable function name.", function));
				return;
			}
		}
		if (!ioFunctions.insert(function).second) {
			error(iNode.id, std::format("The graph defines '{}' twice.", function));
			return;
		}
		std::vector<std::string> params;
		for (size_t i = 1; i < iType.outputs.size(); ++i) params.push_back(std::format("__a{}", i));
		line(std::format("function {}({})", function, joined(params)));
		++m_indent;
		for (size_t i = 1; i < iType.outputs.size(); ++i)
			line(std::format("{} = __a{}", slot(iNode.id, iType.outputs[i].name), i));
		emitChain(iNode, "Then");
		--m_indent;
		line("end");
	}

	void emitChain(const GraphNode& iFrom, const std::string& iPin) {
		const auto link = m_execLinks.find({iFrom.id, iPin});
		if (link == m_execLinks.end())
			return;
		const auto* target = m_script.findNode(link->second->toNode);
		const auto* type = target == nullptr ? nullptr : findNodeType(target->type);
		if (type == nullptr || type->pure) {
			error(iFrom.id, std::format("Output '{}' runs a node without an execution input.", iPin));
			return;
		}
		if (!m_execStack.insert(target->id).second) {
			error(target->id, "The execution loops back into this node: use a For Loop.");
			return;
		}
		if (m_out.size() <= g_maxSourceSize)
			emitStatement(*target, *type);
		m_execStack.erase(target->id);
	}

	void emitStatement(const GraphNode& iNode, const NodeType& iType) {
		auto savedTemps = std::exchange(m_temps, {});
		const auto pins = getNodePins(m_script, iNode);
		line("do");
		++m_indent;
		if (m_debug) {
			markNode(iNode.id);
			line(std::format("__vs.at = {}", static_cast<uint64_t>(iNode.id)));
		}
		std::string next = "Then";
		switch (iType.kind) {
			case NodeKind::Call:
				emitCall(iNode, iType, pins);
				break;
			case NodeKind::FunctionReturn:
				emitReturn(iNode, pins);
				next.clear();
				break;
			case NodeKind::FunctionCall:
				if (emitFunctionCall(iNode, pins))
					next.clear();
				break;
			case NodeKind::SetVariable:
				if (m_library)
					error(iNode.id, "A library function cannot change the variables of its graph.");
				if (const auto name = variableName(iNode); name.has_value()) {
					const auto value = arguments(iNode, pins);
					line(std::format("{} = {}", *name, value.empty() ? "nil" : value.front()));
					line(std::format("{} = {}", slot(iNode.id, "Value"), *name));
				}
				break;
			case NodeKind::Branch:
				line(std::format("if {} then", arguments(iNode, pins).front()));
				++m_indent;
				emitChain(iNode, "True");
				--m_indent;
				line("else");
				++m_indent;
				emitChain(iNode, "False");
				--m_indent;
				line("end");
				next.clear();
				break;
			case NodeKind::Sequence:
				for (const auto& pin: pins.outputs) emitChain(iNode, pin.name);
				next.clear();
				break;
			case NodeKind::ForLoop:
				emitLoop(iNode, pins);
				next = "Completed";
				break;
			case NodeKind::Event:
			case NodeKind::CustomEvent:
			case NodeKind::Operator:
			case NodeKind::GetVariable:
			case NodeKind::FunctionEntry:
				error(iNode.id, "The node has no execution input.");
				next.clear();
				break;
		}
		--m_indent;
		line("end");
		m_temps = std::move(savedTemps);
		if (!next.empty())
			emitChain(iNode, next);
	}

	// One library graph per path, compiled ahead of the events with the prefix of its functions.
	void emitLibraries() {
		for (const auto& node: m_script.nodes) {
			const auto library = node.values.find("library");
			if (node.type != "func.call" || library == node.values.end() || library->second.empty() ||
				m_libraryPrefixes.contains(library->second))
				continue;
			if (m_library) {
				error(node.id, "A library function calls only the functions of its own library.");
				continue;
			}
			const auto index = m_libraryPrefixes.size() + 1;
			m_libraryPrefixes.emplace(library->second, std::format("__l{}_", index));
			const auto* graph = findLibrary(library->second);
			if (graph == nullptr) {
				error(node.id, std::format("The library '{}' cannot be read.", library->second));
				continue;
			}
			Compiler compiler{*graph, CompileMode::Game, std::format("__l{}_", index), std::format("__vsl{}", index)};
			auto code = compiler.runLibrary();
			if (!code) {
				error(node.id, std::format("The library '{}' does not compile: {}", library->second,
										   code.error().front().message));
				continue;
			}
			m_out += *code;
			m_lineCount += static_cast<int>(std::ranges::count(*code, '\n'));
		}
	}

	// The functions of the graph, as locals declared first so that they may call each other.
	void emitFunctions() {
		std::map<std::string, const GraphNode*, std::less<>> entries;
		for (const auto& node: m_script.nodes) {
			if (node.type != "func.entry")
				continue;
			const auto* function = findNodeFunction(m_script, node);
			if (function == nullptr) {
				error(node.id, "The entry names no function of the graph.");
				continue;
			}
			if (!entries.emplace(function->name, &node).second)
				error(node.id, std::format("The function '{}' has two entries.", function->name));
		}
		std::vector<std::string> names;
		for (const auto& function: m_script.functions) {
			if (!isValidName(function.name))
				error(0, std::format("'{}' is not a usable function name.", function.name));
			names.push_back(m_prefix + function.name);
		}
		if (names.empty())
			return;
		line(std::format("local {}", joined(names)));
		for (const auto& function: m_script.functions) {
			const auto entry = entries.find(function.name);
			std::vector<std::string> params;
			params.reserve(function.inputs.size());
			for (size_t i = 0; i < function.inputs.size(); ++i) params.push_back(std::format("__a{}", i + 1));
			line(std::format("function {}{}({})", m_prefix, function.name, joined(params)));
			++m_indent;
			if (entry != entries.end()) {
				m_function = &function;
				for (size_t i = 0; i < function.inputs.size(); ++i)
					m_localPins[{entry->second->id, function.inputs[i].name}] = params[i];
				emitChain(*entry->second, "Then");
				m_function = nullptr;
			}
			--m_indent;
			line("end");
		}
	}

	void emitReturn(const GraphNode& iNode, const NodePins& iPins) {
		const auto* function = findNodeFunction(m_script, iNode);
		if (function == nullptr || function != m_function) {
			error(iNode.id, "The Return node runs outside its function.");
			return;
		}
		auto values = arguments(iNode, iPins);
		if (function->exits.size() > 1) {
			const auto exit = iNode.values.find("exit");
			const auto name = exit == iNode.values.end() ? function->exits.front() : exit->second;
			const auto found = std::ranges::find(function->exits, name);
			if (found == function->exits.end()) {
				error(iNode.id, std::format("The macro has no exit '{}'.", name));
				return;
			}
			values.insert(values.begin(), std::format("{}", std::distance(function->exits.begin(), found) + 1));
		}
		line(values.empty() ? std::string{"do return end"} : std::format("do return {} end", joined(values)));
	}

	// True when the call continues by its exits itself (a macro).
	auto emitFunctionCall(const GraphNode& iNode, const NodePins& iPins) -> bool {
		const auto* function = findNodeFunction(m_script, iNode);
		if (function == nullptr) {
			error(iNode.id, "The node calls an unknown function.");
			return true;
		}
		std::string prefix = m_prefix;
		if (const auto library = iNode.values.find("library");
			library != iNode.values.end() && !library->second.empty()) {
			const auto found = m_libraryPrefixes.find(library->second);
			if (found == m_libraryPrefixes.end())
				return true;
			prefix = found->second;
		} else if (std::ranges::none_of(m_script.nodes, [&](const GraphNode& iEntry) -> bool {
					   return iEntry.type == "func.entry" && iEntry.values.contains("function") &&
							  iEntry.values.at("function") == function->name;
				   })) {
			error(iNode.id, std::format("The function '{}' has no entry node.", function->name));
			return true;
		}
		const auto call = std::format("{}{}({})", prefix, function->name, joined(arguments(iNode, iPins)));
		std::vector<std::string> results;
		results.reserve(function->outputs.size() + 1);
		for (const auto& output: function->outputs) results.push_back(slot(iNode.id, output.name));
		if (function->exits.size() <= 1) {
			line(results.empty() ? call : std::format("{} = {}", joined(results), call));
			return false;
		}
		const auto exit = std::format("__e{}", ++m_tempCount);
		line(std::format("local {}", exit));
		results.insert(results.begin(), exit);
		line(std::format("{} = {}", joined(results), call));
		for (size_t i = 0; i < function->exits.size(); ++i) {
			line(std::format("{} {} == {} then", i == 0 ? "if" : "elseif", exit, i + 1));
			++m_indent;
			emitChain(iNode, function->exits[i]);
			--m_indent;
		}
		line("end");
		return true;
	}

	void emitCall(const GraphNode& iNode, const NodeType& iType, const NodePins& iPins) {
		const auto call = std::format("{}({})", iType.lua, callArguments(arguments(iNode, iPins)));
		const auto slotOf = [&](const std::string& iPin) -> std::string { return slot(iNode.id, iPin); };
		const auto results = resultNames(iPins, slotOf);
		if (results.empty())
			line(call);
		else
			line(std::format("{} = {}", joined(results), call));
		assembleGroups(iPins, slotOf, slotOf);
	}

	void emitLoop(const GraphNode& iNode, const NodePins& iPins) {
		const auto bounds = arguments(iNode, iPins);
		const auto index = std::format("__i{}", ++m_tempCount);
		line(std::format("for {} = {}, {} do", index, bounds[0], bounds[1]));
		++m_indent;
		line(std::format("{} = {}", slot(iNode.id, "Index"), index));
		emitChain(iNode, "Body");
		--m_indent;
		line("end");
	}
};
// NOLINTEND(misc-no-recursion)

}// namespace

auto getProbeKey(const core::UUID iNode, const std::string_view iPin) -> std::string {
	return std::format("{}:{}", static_cast<uint64_t>(iNode), iPin);
}

auto compile(const VisualScript& iScript, const CompileMode iMode)
		-> expected<std::string, std::vector<CompileDiagnostic>> {
	return Compiler{iScript, iMode}.run();
}

auto compileForDebug(const VisualScript& iScript) -> expected<DebugBuild, std::vector<CompileDiagnostic>> {
	Compiler compiler{iScript, CompileMode::Debug};
	auto lua = compiler.run();
	if (!lua)
		return unexpected{std::move(lua.error())};
	return DebugBuild{.lua = std::move(*lua), .nodeLines = std::move(compiler.getNodeLines())};
}

auto compileText(const std::string_view iText, const std::string_view iSourceName, const CompileMode iMode)
		-> std::optional<std::string> {
	const auto script = deserialize(iText, iSourceName);
	if (!script)
		return std::nullopt;
	auto lua = compile(*script, iMode);
	if (!lua) {
		for (const auto& diagnostic: lua.error()) {
			std::string_view message = diagnostic.message;
			if (message.ends_with('.'))
				message.remove_suffix(1);
			OWL_CORE_ERROR("VisualScript: In '{}', node {}: {}.", iSourceName, static_cast<uint64_t>(diagnostic.node),
						   message)
		}
		return std::nullopt;
	}
	return std::move(*lua);
}

}// namespace owl::script::visual
