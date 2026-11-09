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
#include <array>
#include <cctype>
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
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
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

// A Lua name made from a pin name: `TargetEntity` gives `target_entity`, `Then 1` gives `then_1`.
auto luaName(const std::string_view iText) -> std::string {
	std::string out;
	for (size_t i = 0; i < iText.size(); ++i) {
		const auto c = static_cast<unsigned char>(iText[i]);
		if (std::isupper(c) != 0) {
			if (i > 0 && (std::islower(static_cast<unsigned char>(iText[i - 1])) != 0 ||
						  std::isdigit(static_cast<unsigned char>(iText[i - 1])) != 0))
				out += '_';
			out += static_cast<char>(std::tolower(c));
		} else if (std::isalnum(c) != 0) {
			out += static_cast<char>(c);
		} else if (!out.empty() && out.back() != '_') {
			out += '_';
		}
	}
	while (!out.empty() && out.back() == '_') out.pop_back();
	if (out.empty())
		return "value";
	if (std::isdigit(static_cast<unsigned char>(out.front())) != 0)
		out.insert(0, "v");
	return out;
}

auto isNameChar(const char iChar) -> bool {
	return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_' || iChar == '.';
}

// Position after the bracket closing the one at iOpen, quoted strings skipped; npos when it never closes.
auto closingOf(const std::string_view iText, const size_t iOpen) -> size_t {
	int depth = 0;
	for (size_t i = iOpen; i < iText.size(); ++i) {
		const char c = iText[i];
		if (c == '"') {
			for (++i; i < iText.size() && iText[i] != '"'; ++i)
				if (iText[i] == '\\')
					++i;
			continue;
		}
		if (c == '(' || c == '{' || c == '[')
			++depth;
		else if ((c == ')' || c == '}' || c == ']') && --depth == 0)
			return i + 1;
	}
	return std::string_view::npos;
}

// Whether an expression stands alone in an operator: a name, a field, a literal, a call or one parenthesised group.
auto isAtomic(const std::string_view iExpression) -> bool {
	if (iExpression.empty() || std::ranges::all_of(iExpression, isNameChar))
		return true;
	if (iExpression.front() == '"')
		return closingOf(iExpression, 0) == std::string_view::npos && iExpression.size() >= 2 &&
			   iExpression.back() == '"' && iExpression.find('"', 1) == iExpression.size() - 1;
	if (iExpression.front() == '(') {
		const auto close = closingOf(iExpression, 0);
		const auto rest = close == std::string_view::npos ? std::string_view{} : iExpression.substr(close);
		if (rest.size() > 1 && rest.front() == '.' && std::ranges::all_of(rest, isNameChar))
			return true;
	}
	const auto open = iExpression.find('(');
	if (open == std::string_view::npos || iExpression.front() == '{' ||
		!std::ranges::all_of(iExpression.substr(0, open), isNameChar))
		return false;
	return closingOf(iExpression, open) == iExpression.size();
}

// An expression without the parentheses around all of it, for a place where nothing binds to it (argument, value).
auto bare(std::string iExpression) -> std::string {
	while (iExpression.size() >= 2 && iExpression.front() == '(' && closingOf(iExpression, 0) == iExpression.size())
		iExpression = iExpression.substr(1, iExpression.size() - 2);
	return iExpression;
}

// The expression without the parentheses around its leading prefix: `(__vs[2]).grown` is `__vs[2].grown`.
auto unwrapPrefix(std::string iExpression) -> std::string {
	static const std::regex prefix{R"(^[A-Za-z_][A-Za-z0-9_]*(\[[^\[\]()]*\]|\.[A-Za-z_][A-Za-z0-9_]*)*$)"};
	for (iExpression = bare(std::move(iExpression)); iExpression.starts_with('(');) {
		const auto close = closingOf(iExpression, 0);
		if (close == std::string::npos)
			break;
		const auto inner = iExpression.substr(1, close - 2);
		if (!std::regex_match(inner, prefix))
			break;
		iExpression.replace(0, close, inner);
	}
	return iExpression;
}

auto isLuaKeyword(const std::string_view iName) -> bool {
	constexpr std::array<std::string_view, 22> keywords{
			"and",   "break", "do",  "else", "elseif", "end",    "false", "for",  "function", "if",    "in",
			"local", "nil",   "not", "or",   "repeat", "return", "then",  "true", "until",    "while", "continue"};
	return std::ranges::contains(keywords, iName);
}

// A string literal `"name"` that reads as a field: `t["name"]` is written `t.name`.
auto fieldName(const std::string_view iExpression) -> std::optional<std::string_view> {
	if (iExpression.size() < 3 || iExpression.front() != '"' || iExpression.back() != '"')
		return std::nullopt;
	const auto name = iExpression.substr(1, iExpression.size() - 2);
	if (std::isdigit(static_cast<unsigned char>(name.front())) != 0 || isLuaKeyword(name) ||
		!std::ranges::all_of(name, [](const char iChar) -> bool {
			return std::isalnum(static_cast<unsigned char>(iChar)) != 0 || iChar == '_';
		}))
		return std::nullopt;
	return name;
}

auto trimmedText(std::string_view iText) -> std::string_view {
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.front())) != 0) iText.remove_prefix(1);
	while (!iText.empty() && std::isspace(static_cast<unsigned char>(iText.back())) != 0) iText.remove_suffix(1);
	return iText;
}

// The items of a typed table, split at the commas outside double quotes.
auto splitItems(const std::string_view iText) -> std::vector<std::string_view> {
	std::vector<std::string_view> items;
	bool quoted = false;
	size_t start = 0;
	for (size_t i = 0; i <= iText.size(); ++i) {
		if (i < iText.size() && iText[i] == '"')
			quoted = !quoted;
		if (i < iText.size() && (quoted || iText[i] != ','))
			continue;
		if (const auto item = trimmedText(iText.substr(start, i - start)); !item.empty())
			items.push_back(item);
		start = i + 1;
	}
	return items;
}

// Position of the `=` of a `key = value` item, outside double quotes; npos for a plain item.
auto assignmentOf(const std::string_view iItem) -> size_t {
	bool quoted = false;
	for (size_t i = 0; i < iItem.size(); ++i) {
		if (iItem[i] == '"')
			quoted = !quoted;
		else if (iItem[i] == '=' && !quoted)
			return i;
	}
	return std::string_view::npos;
}

auto isModuleName(const std::string_view iName) -> bool {
	if (iName.empty() || iName.front() == '.' || iName.back() == '.' || iName.contains(".."))
		return false;
	return std::ranges::all_of(iName, isNameChar);
}

// A line that leaves its block early (`do return x end`) needs no `do` when the block ends right after it.
auto tidyReturns(const std::string& iLua) -> std::string {
	std::vector<std::string> lines;
	for (const auto range: std::views::split(iLua, '\n')) lines.emplace_back(range.begin(), range.end());
	const auto indentOf = [](const std::string& iLine) -> size_t { return iLine.find_first_not_of('\t'); };
	for (size_t i = 0; i + 1 < lines.size(); ++i) {
		auto& current = lines[i];
		const auto indent = indentOf(current);
		if (indent == std::string::npos || !current.substr(indent).starts_with("do return") ||
			!current.ends_with(" end"))
			continue;
		const auto& next = lines[i + 1];
		const auto nextIndent = indentOf(next);
		if (nextIndent == std::string::npos || nextIndent >= indent)
			continue;
		const auto word = next.substr(nextIndent);
		if (word != "end" && !word.starts_with("else"))
			continue;
		current = current.substr(0, indent) + current.substr(indent + 3, current.size() - indent - 7);
	}
	std::string out;
	for (size_t i = 0; i < lines.size(); ++i) {
		out += lines[i];
		if (i + 1 < lines.size())
			out += '\n';
	}
	return out;
}

// Where each value of the graph is written and read: the Lua function of each, counted in emission order.
struct Usage {
	std::map<PinKey, int> owners;
	std::map<PinKey, std::set<int>> readers;
	// Loop values read after their loop ended: they are copied out of the loop variables.
	std::set<PinKey> escaped;
};

// The parts of the Lua source, in order; the lines of a node are counted in its part and shifted when joined.
enum struct Part : uint8_t { Header, Custom, Declarations, Functions, Joins, Events };
constexpr size_t g_partCount = 6;

struct Section {
	std::string text;
	int lines{0};
};

// A Lua function being written: its local names, those declared at its top, and the values it holds in locals.
struct FunctionScope {
	int id{0};
	std::set<std::string> names;
	std::vector<std::string> hoisted;
	std::map<PinKey, std::string> locals;
	size_t hoistAt{0};
	size_t indent{0};
};

struct CompilerSetup {
	CompileMode mode{CompileMode::Game};
	const Usage* usage{nullptr};
	std::string prefix;
	std::string slotTable{"__vs"};
	bool library{false};
};

// NOLINTBEGIN(misc-no-recursion) walks the graph; m_pureStack and m_execStack stop at every cycle
class Compiler final {
public:
	Compiler(const VisualScript& iScript, CompilerSetup iSetup)
		: m_script{iScript}, m_debug{iSetup.mode == CompileMode::Debug}, m_usage{iSetup.usage},
		  m_prefix{std::move(iSetup.prefix)}, m_slotTable{std::move(iSetup.slotTable)}, m_library{iSetup.library} {
		for (const auto& link: iScript.links) {
			const auto* from = iScript.findNode(link.fromNode);
			const auto* fromType = from == nullptr ? nullptr : findNodeType(from->type);
			if (fromType != nullptr && isExecOutput(*from, link.fromPin)) {
				m_execLinks[{link.fromNode, link.fromPin}] = &link;
				++m_execInDegree[link.toNode];
			} else {
				m_inputLinks[{link.toNode, link.toPin}] = &link;
				++m_outputReaders[{link.fromNode, link.fromPin}];
			}
		}
		reserveNames();
	}

	auto run() -> expected<std::string, std::vector<CompileDiagnostic>> {
		m_part = Part::Header;
		emitProperties();
		emitLibraries();
		emitCustomNodes();
		emitFunctions();
		m_part = Part::Events;
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
		auto lua = assemble();
		if (lua.size() > g_maxSourceSize)
			error(0, "The graph expands into too much Lua: merge fewer execution paths into the same nodes.");
		if (!m_errors.empty())
			return unexpected{std::move(m_errors)};
		return lua;
	}

	// A library graph: its custom nodes and functions only, named with the prefix, and the slot table they share.
	auto runLibrary() -> expected<std::string, std::vector<CompileDiagnostic>> {
		emitCustomNodes();
		emitFunctions();
		auto lua = assemble();
		if (!m_errors.empty())
			return unexpected{std::move(m_errors)};
		return lua;
	}

	[[nodiscard]] auto getNodeLines() -> std::map<core::UUID, std::vector<int>>& { return m_nodeLines; }

	[[nodiscard]] auto getUsage() const -> const Usage& { return m_record; }

private:
	const VisualScript& m_script;
	bool m_debug;
	const Usage* m_usage;
	Usage m_record;
	std::string m_prefix;
	std::string m_slotTable;
	bool m_library;
	const GraphFunction* m_function{nullptr};
	std::array<Section, g_partCount> m_parts;
	Part m_part{Part::Header};
	std::map<core::UUID, std::vector<std::pair<Part, int>>> m_partLines;
	std::map<core::UUID, std::vector<int>> m_nodeLines;
	std::map<std::string, std::string, std::less<>> m_libraryPrefixes;
	std::map<PinKey, const GraphLink*> m_execLinks;
	std::map<PinKey, const GraphLink*> m_inputLinks;
	std::map<uint64_t, int> m_execInDegree;
	std::map<PinKey, int> m_outputReaders;
	std::map<PinKey, size_t> m_slots;
	std::map<uint64_t, std::string> m_joins;
	std::vector<std::string> m_declarations;
	std::set<std::string> m_reserved;
	FunctionScope m_scope;
	int m_functionCount{0};
	bool m_statementLocals{false};
	bool m_usesSlotTable{false};
	std::unordered_map<uint64_t, std::vector<std::string>> m_temps;
	std::set<uint64_t> m_pureStack;
	std::set<uint64_t> m_execStack;
	std::map<uint64_t, std::set<uint64_t>> m_postDominators;
	std::set<uint64_t> m_postDominatorsPending;
	std::vector<uint64_t> m_stops;
	std::set<uint64_t> m_reported;
	std::set<PinKey> m_closedLoops;
	std::vector<CompileDiagnostic> m_errors;
	size_t m_indent{0};

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

	auto out() -> Section& { return m_parts[static_cast<size_t>(m_part)]; }

	void line(const std::string_view iText) {
		auto& section = out();
		section.text.append(m_indent, '\t');
		section.text += iText;
		section.text += '\n';
		++section.lines;
	}

	void markNode(const core::UUID iNode) {
		if (m_debug)
			m_partLines[iNode].emplace_back(m_part, out().lines + 1);
	}

	// The names a local may not take: what the graph defines at the top of its Lua.
	void reserveNames() {
		for (const auto& variable: m_script.variables) m_reserved.insert(variable.name);
		for (const auto& function: m_script.functions) m_reserved.insert(function.name);
		for (const auto& custom: m_script.customNodes) m_reserved.insert(custom.name);
		for (const auto& type: getNodeTypes())
			if (type.kind == NodeKind::Event)
				m_reserved.insert(type.lua);
		for (const auto& node: m_script.nodes)
			if (const auto name = node.values.find("function");
				node.type == "event.custom" && name != node.values.end())
				m_reserved.insert(name->second);
	}

	auto allocate(const std::string_view iBase) -> std::string {
		// Lua lets a discarded result be `_` as often as it likes.
		if (iBase == "_")
			return "_";
		const auto base = luaName(iBase);
		for (size_t index = 1;; ++index) {
			auto name = index == 1 ? base : std::format("{}_{}", base, index);
			if (isValidName(name) && !m_reserved.contains(name) && m_scope.names.insert(name).second)
				return name;
		}
	}

	void openScope() {
		m_scope = FunctionScope{.id = ++m_functionCount,
								.names = {},
								.hoisted = {},
								.locals = {},
								.hoistAt = 0,
								.indent = 0};
	}

	void openBody(const std::string_view iSignature) {
		line(iSignature);
		m_scope.hoistAt = out().text.size();
		m_scope.indent = m_indent + 1;
		++m_indent;
	}

	void closeBody() {
		--m_indent;
		if (!m_scope.hoisted.empty()) {
			auto& section = out();
			section.text.insert(m_scope.hoistAt,
								std::string(m_scope.indent, '\t') + "local " + joined(m_scope.hoisted) + "\n");
			++section.lines;
		}
		line("end");
	}

	[[nodiscard]] auto isOnlyReadBy(const PinKey& iKey, const int iFunction) const -> bool {
		const auto readers = m_usage->readers.find(iKey);
		return readers == m_usage->readers.end() ||
			   std::ranges::all_of(readers->second,
								   [iFunction](const int iReader) -> bool { return iReader == iFunction; });
	}

	[[nodiscard]] auto isRead(const PinKey& iKey) const -> bool {
		return m_usage == nullptr || m_usage->readers.contains(iKey);
	}

	auto tableSlot(const PinKey& iKey) -> std::string {
		m_usesSlotTable = true;
		if (m_debug && !m_library)
			return std::format("__vs[{}]", quote(getProbeKey(core::UUID{iKey.first}, iKey.second)));
		const auto [it, inserted] = m_slots.try_emplace(iKey, m_slots.size() + 1);
		return std::format("{}[{}]", m_slotTable, it->second);
	}

	// Where a value lives in the current function: a local when only this function reads it, the slot table otherwise.
	auto storage(const PinKey& iKey) -> std::string {
		if (const auto local = m_scope.locals.find(iKey); local != m_scope.locals.end())
			return local->second;
		if (!m_debug && m_usage != nullptr) {
			const auto owner = m_usage->owners.find(iKey);
			if (owner != m_usage->owners.end() && owner->second == m_scope.id && isOnlyReadBy(iKey, m_scope.id)) {
				auto name = allocate(iKey.second);
				m_scope.hoisted.push_back(name);
				m_scope.locals[iKey] = name;
				return name;
			}
		}
		return tableSlot(iKey);
	}

	auto read(const core::UUID iNode, const std::string& iPin) -> std::string {
		const PinKey key{iNode, iPin};
		m_record.readers[key].insert(m_scope.id);
		if (m_closedLoops.contains(key))
			m_record.escaped.insert(key);
		return storage(key);
	}

	// Where an execution node writes an output, nothing when no node reads it.
	auto target(const core::UUID iNode, const std::string& iPin) -> std::optional<std::string> {
		const PinKey key{iNode, iPin};
		m_record.owners[key] = m_scope.id;
		if (!m_debug && !isRead(key))
			return std::nullopt;
		return storage(key);
	}

	// A parameter of the current function standing for an output pin, copied to the slot table when read elsewhere.
	void bindParameter(const core::UUID iNode, const std::string& iPin, const std::string& iName) {
		const PinKey key{iNode, iPin};
		m_record.owners[key] = m_scope.id;
		m_scope.locals[key] = iName;
		if (m_debug || m_usage == nullptr || (isRead(key) && !isOnlyReadBy(key, m_scope.id)))
			line(std::format("{} = {}", tableSlot(key), iName));
	}

	// A local of the statement: the statement is then wrapped in `do ... end`.
	auto statementLocal(const std::string_view iBase) -> std::string {
		m_statementLocals = true;
		return allocate(iBase);
	}

	auto literal(const core::UUID iNode, const PinSpec& iPin, const std::string& iValue) -> std::string {
		const auto& text = iValue.empty() ? iPin.defaultValue : iValue;
		if (text.empty() && iPin.optional)
			return "nil";
		switch (iPin.type) {
			case PinType::Exec:
				return "nil";
			case PinType::Table:
				return tableLiteral(iNode, text);
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
				if (text == "0")
					return "0";
				if (const auto value = parseInteger(text); value.has_value() && *value > 0)
					return std::format("scene.find_by_uuid({})", quote(text));
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

	// A value of a typed table: a quoted text, a boolean, a number, or the text itself.
	static auto itemLiteral(const std::string_view iItem) -> std::string {
		if (iItem.size() >= 2 && iItem.front() == '"' && iItem.back() == '"')
			return quote(iItem.substr(1, iItem.size() - 2));
		if (iItem == "true" || iItem == "false")
			return std::string{iItem};
		if (const auto value = parseNumber(std::string{iItem}); value.has_value())
			return numberLiteral(*value);
		return quote(iItem);
	}

	// A table typed as `a, b, key = c`: list items and named fields, `{}` when empty.
	auto tableLiteral(const core::UUID iNode, const std::string_view iText) -> std::string {
		std::vector<std::string> entries;
		for (const auto item: splitItems(iText)) {
			const auto equal = assignmentOf(item);
			if (equal == std::string_view::npos) {
				entries.push_back(itemLiteral(item));
				continue;
			}
			const auto key = trimmedText(item.substr(0, equal));
			if (!fieldName(std::format("\"{}\"", key)).has_value()) {
				error(iNode, std::format("'{}' is not a usable table key: use a Lua name.", key));
				return "nil";
			}
			entries.push_back(std::format("{} = {}", key, itemLiteral(trimmedText(item.substr(equal + 1)))));
		}
		return entries.empty() ? std::string{"{}"} : std::format("{{ {} }}", joined(entries));
	}

	// The default of a list variable: numbers for `numbers`, texts for `strings`.
	auto listDefault(const GraphVariable& iVariable) -> std::optional<std::string> {
		std::vector<std::string> entries;
		for (const auto item: splitItems(iVariable.defaultValue)) {
			if (iVariable.type == ScriptPropertyType::Strings) {
				entries.push_back(item.size() >= 2 && item.front() == '"' && item.back() == '"'
										  ? quote(item.substr(1, item.size() - 2))
										  : quote(item));
				continue;
			}
			const auto value = parseNumber(std::string{item});
			if (!value.has_value()) {
				error(0, std::format("Variable '{}' holds '{}', which is not a number.", iVariable.name, item));
				return std::nullopt;
			}
			entries.push_back(numberLiteral(*value));
		}
		return entries.empty() ? std::string{"{}"} : std::format("{{ {} }}", joined(entries));
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

	// A value read without cost: a name or a slot of a table (`__vs[3]`, `__vs["12:Result"]`).
	static auto isStored(const std::string_view iExpression) -> bool {
		const auto open = iExpression.find('[');
		if (open == std::string_view::npos)
			return std::ranges::all_of(iExpression, isNameChar);
		return std::ranges::all_of(iExpression.substr(0, open), isNameChar) && iExpression.back() == ']' &&
			   closingOf(iExpression, open) == iExpression.size();
	}

	// The arguments a grouped input of a call passes: one per component, the table read once.
	auto groupArguments(const GraphNode& iNode, const PinSpec& iPin) -> std::vector<std::string> {
		const auto components = componentsOf(iPin);
		if (!m_inputLinks.contains({iNode.id, iPin.name})) {
			const auto value = iNode.values.find(iPin.name);
			return groupLiteral(iNode.id, iPin, value == iNode.values.end() ? std::string{} : value->second)
					.value_or(std::vector<std::string>(components.size(), "nil"));
		}
		auto table = bare(inputExpression(iNode, iPin));
		if (!isStored(table)) {
			const auto name = statementLocal(iPin.name);
			line(std::format("local {} = {}", name, table));
			table = name;
		}
		std::vector<std::string> out;
		out.reserve(components.size());
		for (const auto& component: components) out.push_back(std::format("{}.{}", table, component));
		return out;
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
			return read(source->id, output->name);
		const auto& values = evaluatePure(*source, *sourceType, pins);
		const auto index = static_cast<size_t>(std::distance(pins.outputs.begin(), output));
		return index < values.size() ? values[index] : std::string{"nil"};
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

	// The Lua a custom or Lua-function node calls, checked against its definition.
	auto scriptedCallee(const GraphNode& iNode, const NodeType& iType) -> std::optional<std::string> {
		if (iType.kind == NodeKind::CustomCall) {
			const auto* custom = findNodeCustom(m_script, iNode);
			if (custom == nullptr) {
				error(iNode.id, "The node names no custom node of the graph.");
				return std::nullopt;
			}
			if (custom->pure != iType.pure) {
				error(iNode.id,
					  std::format("The custom node '{}' changed between pure and not: place it again.", custom->name));
				return std::nullopt;
			}
			return m_prefix + custom->name;
		}
		const auto module = iNode.values.find("module");
		if (module == iNode.values.end() || !isModuleName(module->second)) {
			error(iNode.id, "The node names no Lua module.");
			return std::nullopt;
		}
		if (findLuaModule(module->second) == nullptr) {
			error(iNode.id, std::format("The Lua module '{}' cannot be read.", module->second));
			return std::nullopt;
		}
		const auto* function = findNodeLuaFunction(iNode);
		if (function == nullptr) {
			error(iNode.id, std::format("The Lua module '{}' has no annotated function '{}'.", module->second,
										iNode.values.contains("function") ? iNode.values.at("function") : ""));
			return std::nullopt;
		}
		if (function->pure != iType.pure) {
			error(iNode.id,
				  std::format("The Lua function '{}' changed between pure and not: place it again.", function->name));
			return std::nullopt;
		}
		return std::format("require({}).{}", quote(module->second), function->name);
	}

	// A single plain output read by one input is written where it is read, in Game output only.
	[[nodiscard]] auto canInline(const GraphNode& iNode, const NodeType& iType, const NodePins& iPins) const -> bool {
		if (m_debug || iPins.outputs.size() != 1 || iPins.outputs.front().group == iPins.outputs.front().name)
			return false;
		if (const auto readers = m_outputReaders.find({iNode.id, iPins.outputs.front().name});
			readers != m_outputReaders.end() && readers->second > 1)
			return false;
		return iType.kind == NodeKind::Operator || iType.kind == NodeKind::GetVariable ||
			   iType.kind == NodeKind::Call || iType.kind == NodeKind::CustomCall || iType.kind == NodeKind::LuaCall;
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
		const bool inlined = canInline(iNode, iType, iPins);
		std::optional<std::string> expression;
		switch (iType.kind) {
			case NodeKind::Call:
				if (inlined)
					expression = std::format("{}({})", iType.lua, callArguments(args));
				else
					return m_temps[iNode.id] =
								   evaluateCall(iNode, std::format("{}({})", iType.lua, callArguments(args)), iPins);
				break;
			case NodeKind::CustomCall:
			case NodeKind::LuaCall:
				{
					const auto callee = scriptedCallee(iNode, iType);
					if (!callee.has_value())
						return none;
					const auto call = std::format("{}({})", *callee, joined(bareAll(args)));
					if (!inlined)
						return m_temps[iNode.id] = evaluateCall(iNode, call, iPins);
					expression = call;
				}
				break;
			case NodeKind::Operator:
				{
					const auto lua = withoutTrailingNils(iType.lua, args, iPins);
					expression = substitute(lua, storeRepeated(lua, args));
				}
				break;
			case NodeKind::GetVariable:
				if (m_library)
					error(iNode.id, "A library function cannot read the variables of its graph.");
				expression = variableName(iNode).value_or("nil");
				break;
			case NodeKind::Event:
			case NodeKind::CustomEvent:
			case NodeKind::Branch:
			case NodeKind::Sequence:
			case NodeKind::ForLoop:
			case NodeKind::ForEach:
			case NodeKind::Statement:
			case NodeKind::SetVariable:
			case NodeKind::FunctionEntry:
			case NodeKind::FunctionReturn:
			case NodeKind::FunctionCall:
				error(iNode.id, "The node cannot be evaluated as a value.");
				return none;
		}
		if (inlined)
			return m_temps[iNode.id] = {*expression};
		std::vector<std::string> temps;
		temps.reserve(iPins.outputs.size());
		for (const auto& output: iPins.outputs) temps.push_back(statementLocal(output.name));
		markNode(iNode.id);
		line(std::format("local {} = {}", joined(temps), bare(*expression)));
		probe(iNode.id, iPins, temps);
		return m_temps[iNode.id] = std::move(temps);
	}

	// A pure call into statement locals, a grouped output assembled into its table.
	auto evaluateCall(const GraphNode& iNode, const std::string& iCall, const NodePins& iPins)
			-> std::vector<std::string> {
		std::vector<std::string> values;
		std::vector<std::string> results;
		std::vector<std::pair<std::string, std::vector<std::string>>> groups;
		for (const auto& pin: iPins.outputs) {
			if (pin.group == pin.name) {
				std::vector<std::string> parts;
				for (const auto& component: componentsOf(pin)) parts.push_back(statementLocal(component));
				for (const auto& part: parts) results.push_back(part);
				values.push_back(statementLocal(pin.name));
				groups.emplace_back(values.back(), std::move(parts));
			} else {
				values.push_back(statementLocal(pin.name));
				results.push_back(values.back());
			}
		}
		markNode(iNode.id);
		line(std::format("local {} = {}", joined(results), iCall));
		for (size_t i = 0, group = 0; i < iPins.outputs.size(); ++i)
			if (iPins.outputs[i].group == iPins.outputs[i].name) {
				line(std::format("local {} = {}", groups[group].first,
								 componentTable(iPins.outputs[i].type, groups[group].second)));
				++group;
			}
		probe(iNode.id, iPins, values);
		return values;
	}

	void probe(const core::UUID iNode, const NodePins& iPins, const std::vector<std::string>& iValues) {
		if (!m_debug)
			return;
		for (size_t i = 0; i < iPins.outputs.size() && i < iValues.size(); ++i)
			line(std::format("__vs[{}] = {}", quote(getProbeKey(iNode, iPins.outputs[i].name)), iValues[i]));
	}

	// A binding counts its arguments: unset optional parameters at the end are left out, not passed as nil.
	static auto callArguments(std::vector<std::string> iArgs) -> std::string {
		while (!iArgs.empty() && iArgs.back() == "nil") iArgs.pop_back();
		return joined(bareAll(std::move(iArgs)));
	}

	static auto bareAll(std::vector<std::string> iArgs) -> std::vector<std::string> {
		for (auto& arg: iArgs) arg = bare(std::move(arg));
		return iArgs;
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

	// An argument a template reads several times is computed once, in a statement local.
	auto storeRepeated(const std::string& iTemplate, std::vector<std::string> iArgs) -> std::vector<std::string> {
		for (size_t i = 0; i < iArgs.size() && i < 10; ++i) {
			const auto placeholder = std::format("{{{}}}", i);
			size_t count = 0;
			for (auto at = iTemplate.find(placeholder); at != std::string::npos;
				 at = iTemplate.find(placeholder, at + 1))
				++count;
			if (count < 2 || isStored(iArgs[i]) || isLiteral(iArgs[i]))
				continue;
			const auto name = statementLocal("value");
			line(std::format("local {} = {}", name, bare(iArgs[i])));
			iArgs[i] = name;
		}
		return iArgs;
	}

	static auto isLiteral(const std::string_view iExpression) -> bool {
		return !iExpression.empty() && (std::isdigit(static_cast<unsigned char>(iExpression.front())) != 0 ||
										iExpression == "true" || iExpression == "false" || iExpression == "nil" ||
										(iExpression.front() == '"' && isAtomic(iExpression)));
	}

	// The template without the optional inputs left empty at the end of its call or table: `f(a, nil)` is `f(a)`.
	static auto withoutTrailingNils(std::string iTemplate, const std::vector<std::string>& iArgs, const NodePins& iPins)
			-> std::string {
		std::vector<const PinSpec*> inputs;
		for (const auto& pin: iPins.inputs)
			if (pin.type != PinType::Exec)
				inputs.push_back(&pin);
		for (size_t i = std::min(iArgs.size(), inputs.size()); i-- > 0;) {
			if (iArgs[i] != "nil" || !inputs[i]->optional)
				break;
			bool trimmed = false;
			for (const std::string_view suffix: {")", " }"}) {
				const auto tail = std::format(", {{{}}}{}", i, suffix);
				if (iTemplate.ends_with(tail)) {
					iTemplate.erase(iTemplate.size() - tail.size(), tail.size() - suffix.size());
					trimmed = true;
					break;
				}
			}
			if (!trimmed)
				break;
		}
		if (iTemplate == "{ {0} }" && !iArgs.empty() && iArgs.front() == "nil")
			return "{}";
		return iTemplate;
	}

	static auto substitute(const std::string& iTemplate, const std::vector<std::string>& iArgs) -> std::string {
		std::string out;
		for (size_t i = 0; i < iTemplate.size(); ++i) {
			if (iTemplate[i] == '[' && i + 4 < iTemplate.size() && iTemplate[i + 1] == '{' && iTemplate[i + 3] == '}' &&
				iTemplate[i + 4] == ']') {
				if (const auto index = static_cast<size_t>(iTemplate[i + 2] - '0'); index < iArgs.size())
					if (const auto name = fieldName(iArgs[index]); name.has_value()) {
						out += '.';
						out += *name;
						i += 4;
						continue;
					}
			}
			if (iTemplate[i] == '{' && i + 2 < iTemplate.size() && iTemplate[i + 2] == '}') {
				if (const auto index = static_cast<size_t>(iTemplate[i + 1] - '0'); index < iArgs.size()) {
					const auto& arg = iArgs[index];
					out += isAtomic(arg) ? arg : "(" + arg + ")";
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

	// The Lua literal of the default of a variable, nil when it does not parse.
	auto variableLiteral(const GraphVariable& iVariable) -> std::optional<std::string> {
		const auto type = getVariablePinType(iVariable.type);
		if (type == PinType::Table)
			return listDefault(iVariable);
		const PinSpec pin{.name = iVariable.name,
						  .type = type,
						  .defaultValue = {},
						  .optional = false,
						  .group = {},
						  .components = {},
						  .field = false};
		if (type == PinType::Entity)
			return iVariable.defaultValue.empty() || iVariable.defaultValue == "0" || iVariable.defaultValue == "self"
						   ? std::string{"0"}
						   : quote(iVariable.defaultValue);
		if (isGroup(type))
			return arrayLiteral(pin, iVariable.defaultValue);
		return literal(0, pin, iVariable.defaultValue);
	}

	// Exposed variables are the script properties; the others are globals set when the chunk runs.
	void emitProperties() {
		std::set<std::string> names;
		for (const auto& variable: m_script.variables)
			if (!isValidName(variable.name) || !names.insert(variable.name).second)
				error(0, std::format("Variable '{}' needs a unique Lua name.", variable.name));
		const auto valid = [&names](const GraphVariable& iVariable) -> bool { return names.contains(iVariable.name); };
		if (std::ranges::any_of(m_script.variables, &GraphVariable::exposed)) {
			line("properties = {");
			++m_indent;
			for (const auto& variable: m_script.variables) {
				if (!variable.exposed || !valid(variable))
					continue;
				const auto value = variableLiteral(variable);
				if (!value.has_value())
					continue;
				std::string bounds;
				for (const auto& [key, bound]:
					 {std::pair{"min", variable.min}, std::pair{"max", variable.max}, std::pair{"step", variable.step}})
					if (bound.has_value())
						bounds += std::format(", {} = {}", key, *bound);
				line(std::format("{{ name = {}, type = {}, default = {}{} }},", quote(variable.name),
								 quote(ScriptEngine::propertyTypeName(variable.type)), *value, bounds));
			}
			--m_indent;
			line("}");
		}
		for (const auto& variable: m_script.variables) {
			if (variable.exposed || !valid(variable))
				continue;
			if (const auto value = variableLiteral(variable); value.has_value())
				line(std::format("{} = {}", variable.name, *value));
		}
	}

	// The custom nodes, as local functions their calls see: the inputs as parameters, the body as written.
	void emitCustomNodes() {
		const auto saved = std::exchange(m_part, Part::Custom);
		for (const auto& custom: m_script.customNodes) {
			if (!isValidName(custom.name)) {
				error(0, std::format("'{}' is not a usable custom node name.", custom.name));
				continue;
			}
			std::set<std::string> params;
			std::vector<std::string> names;
			for (const auto& input: custom.inputs) {
				if (!isValidName(input.name) || !params.insert(input.name).second)
					error(0, std::format("Custom node '{}': input '{}' needs a unique Lua name.", custom.name,
										 input.name));
				names.push_back(input.name);
			}
			line(std::format("local function {}{}({})", m_prefix, custom.name, joined(names)));
			++m_indent;
			for (const auto range: std::views::split(custom.code, '\n')) {
				const std::string_view text{range.begin(), range.end()};
				if (text.find_first_not_of(" \t\r") == std::string_view::npos)
					continue;
				line(text);
			}
			--m_indent;
			line("end");
		}
		m_part = saved;
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
		openScope();
		std::vector<std::string> params;
		for (size_t i = 1; i < iType.outputs.size(); ++i) params.push_back(allocate(iType.outputs[i].name));
		openBody(std::format("function {}({})", function, joined(params)));
		for (size_t i = 1; i < iType.outputs.size(); ++i) bindParameter(iNode.id, iType.outputs[i].name, params[i - 1]);
		emitChain(iNode, "Then");
		closeBody();
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
		if (std::ranges::contains(m_stops, static_cast<uint64_t>(target->id)))
			return;
		if (m_execStack.contains(target->id)) {
			error(target->id, "The execution loops back into this node: use a For Loop.");
			return;
		}
		if (m_function == nullptr && m_execInDegree[target->id] > 1) {
			line(std::format("{}()", joinFunction(*target, *type)));
			return;
		}
		m_execStack.insert(target->id);
		if (out().text.size() <= g_maxSourceSize)
			emitStatement(*target, *type);
		m_execStack.erase(target->id);
	}

	// A node several execution paths reach, written once as a function each of them calls.
	auto joinFunction(const GraphNode& iNode, const NodeType& iType) -> std::string {
		if (const auto found = m_joins.find(iNode.id); found != m_joins.end())
			return found->second;
		auto name = std::format("__join{}", m_joins.size() + 1);
		m_joins.emplace(iNode.id, name);
		m_declarations.push_back(name);
		auto savedScope = std::exchange(m_scope, {});
		auto savedTemps = std::exchange(m_temps, {});
		const auto savedLocals = std::exchange(m_statementLocals, false);
		const auto savedPart = std::exchange(m_part, Part::Joins);
		const auto savedIndent = std::exchange(m_indent, 0);
		openScope();
		openBody(std::format("function {}()", name));
		m_execStack.insert(iNode.id);
		emitStatement(iNode, iType);
		m_execStack.erase(iNode.id);
		closeBody();
		m_indent = savedIndent;
		m_part = savedPart;
		m_statementLocals = savedLocals;
		m_temps = std::move(savedTemps);
		m_scope = std::move(savedScope);
		return name;
	}

	void emitStatement(const GraphNode& iNode, const NodeType& iType) {
		auto savedTemps = std::exchange(m_temps, {});
		const auto savedLocals = std::exchange(m_statementLocals, false);
		const auto pins = getNodePins(m_script, iNode);
		if (m_debug)
			line("do");
		const auto start = out().text.size();
		++m_indent;
		if (m_debug) {
			markNode(iNode.id);
			line(std::format("__vs.at = {}", static_cast<uint64_t>(iNode.id)));
		}
		const auto next = emitBody(iNode, iType, pins);
		--m_indent;
		if (m_debug) {
			line("end");
		} else if (m_statementLocals) {
			out().text.insert(start, std::string(m_indent, '\t') + "do\n");
			++out().lines;
			line("end");
		} else {
			dedent(start);
		}
		m_statementLocals = savedLocals;
		m_temps = std::move(savedTemps);
		if (!next.empty())
			emitChain(iNode, next);
	}

	// Writes what a node does and gives the execution output to follow after it, empty when it continues by itself.
	auto emitBody(const GraphNode& iNode, const NodeType& iType, const NodePins& iPins) -> std::string {
		switch (iType.kind) {
			case NodeKind::Call:
				emitCall(iNode, std::format("{}({})", iType.lua, callArguments(arguments(iNode, iPins))), iPins);
				return "Then";
			case NodeKind::CustomCall:
			case NodeKind::LuaCall:
				if (const auto callee = scriptedCallee(iNode, iType); callee.has_value())
					emitCall(iNode, std::format("{}({})", *callee, joined(bareAll(arguments(iNode, iPins)))), iPins);
				return "Then";
			case NodeKind::FunctionReturn:
				emitReturn(iNode, iPins);
				return {};
			case NodeKind::FunctionCall:
				return emitFunctionCall(iNode, iPins) ? std::string{} : std::string{"Then"};
			case NodeKind::SetVariable:
				emitSetVariable(iNode, iPins);
				return "Then";
			case NodeKind::Branch:
				emitBranch(iNode, iPins);
				return {};
			case NodeKind::Sequence:
				for (const auto& pin: iPins.outputs) emitChain(iNode, pin.name);
				return {};
			case NodeKind::ForLoop:
				emitLoop(iNode, iPins);
				return "Completed";
			case NodeKind::ForEach:
				emitForEach(iNode, iPins);
				return "Completed";
			case NodeKind::Statement:
				{
					const auto args = arguments(iNode, iPins);
					const auto lua = withoutTrailingNils(iType.lua, args, iPins);
					auto stored = storeRepeated(lua, args);
					// A statement starting with `(` would continue the call of the line before it.
					const auto statement = unwrapPrefix(substitute(lua, stored));
					if (statement.starts_with('(') && lua.starts_with("{0}") && !stored.empty()) {
						line("do");
						++m_indent;
						line(std::format("local __target = {}", bare(std::exchange(stored.front(), "__target"))));
						line(bare(substitute(lua, stored)));
						--m_indent;
						line("end");
					} else {
						line(statement);
					}
				}
				return "Then";
			case NodeKind::Event:
			case NodeKind::CustomEvent:
			case NodeKind::Operator:
			case NodeKind::GetVariable:
			case NodeKind::FunctionEntry:
				error(iNode.id, "The node has no execution input.");
				return {};
		}
		return {};
	}

	// Takes one tab off every line written since an offset: the body of a statement that needs no `do` block.
	void dedent(const size_t iStart) {
		auto& text = out().text;
		for (size_t position = iStart; position < text.size();) {
			if (text[position] == '\t')
				text.erase(position, 1);
			const auto end = text.find('\n', position);
			if (end == std::string::npos)
				break;
			position = end + 1;
		}
	}

	void emitSetVariable(const GraphNode& iNode, const NodePins& iPins) {
		if (m_library)
			error(iNode.id, "A library function cannot change the variables of its graph.");
		const auto name = variableName(iNode);
		if (!name.has_value())
			return;
		const auto value = arguments(iNode, iPins);
		line(std::format("{} = {}", *name, value.empty() ? "nil" : bare(value.front())));
		if (const auto result = target(iNode.id, "Value"); result.has_value())
			line(std::format("{} = {}", *result, *name));
	}

	// Where the execution leaves a node for the join of two paths: a loop by Completed, a sequence or a macro by none.
	auto successors(const uint64_t iNode) -> std::vector<uint64_t> {
		const auto* node = m_script.findNode(core::UUID{iNode});
		const auto* type = node == nullptr ? nullptr : findNodeType(node->type);
		if (type == nullptr)
			return {};
		std::vector<std::string> pins{"Then"};
		switch (type->kind) {
			case NodeKind::Branch:
				pins = {"True", "False"};
				break;
			case NodeKind::ForLoop:
			case NodeKind::ForEach:
				pins = {"Completed"};
				break;
			case NodeKind::Sequence:
			case NodeKind::FunctionReturn:
				return {};
			case NodeKind::FunctionCall:
				if (const auto* function = findNodeFunction(m_script, *node); function != nullptr && function->macro)
					return {};
				break;
			case NodeKind::Event:
			case NodeKind::CustomEvent:
			case NodeKind::Call:
			case NodeKind::Operator:
			case NodeKind::Statement:
			case NodeKind::GetVariable:
			case NodeKind::SetVariable:
			case NodeKind::FunctionEntry:
			case NodeKind::CustomCall:
			case NodeKind::LuaCall:
				break;
		}
		std::vector<uint64_t> out;
		for (const auto& pin: pins)
			if (const auto link = m_execLinks.find({iNode, pin}); link != m_execLinks.end())
				out.push_back(static_cast<uint64_t>(link->second->toNode));
		return out;
	}

	// The nodes every execution path from a node runs, the node included.
	auto postDominators(const uint64_t iNode) -> const std::set<uint64_t>& {
		if (const auto found = m_postDominators.find(iNode); found != m_postDominators.end())
			return found->second;
		static const std::set<uint64_t> none;
		if (!m_postDominatorsPending.insert(iNode).second)
			return none;
		std::optional<std::set<uint64_t>> common;
		for (const auto next: successors(iNode)) {
			const auto& after = postDominators(next);
			if (!common.has_value()) {
				common = after;
				continue;
			}
			std::set<uint64_t> both;
			std::ranges::set_intersection(*common, after, std::inserter(both, both.begin()));
			common = std::move(both);
		}
		auto result = common.value_or(std::set<uint64_t>{});
		result.insert(iNode);
		m_postDominatorsPending.erase(iNode);
		return m_postDominators[iNode] = std::move(result);
	}

	// Where the True and False paths of a branch meet again: the nearest node both run.
	auto mergeOf(const GraphNode& iBranch) -> std::optional<uint64_t> {
		const auto onTrue = m_execLinks.find({iBranch.id, "True"});
		const auto onFalse = m_execLinks.find({iBranch.id, "False"});
		if (onTrue == m_execLinks.end() || onFalse == m_execLinks.end())
			return std::nullopt;
		const auto& left = postDominators(static_cast<uint64_t>(onTrue->second->toNode));
		const auto& right = postDominators(static_cast<uint64_t>(onFalse->second->toNode));
		std::optional<uint64_t> merge;
		size_t depth = 0;
		for (const auto node: left)
			if (right.contains(node) && postDominators(node).size() > depth) {
				merge = node;
				depth = postDominators(node).size();
			}
		return merge;
	}

	// Whether every execution link into the merge of a branch comes from the branch or the paths before the merge.
	auto isOwnMerge(const GraphNode& iBranch, const uint64_t iMerge) -> bool {
		std::set<uint64_t> region{static_cast<uint64_t>(iBranch.id)};
		std::vector<uint64_t> pending{static_cast<uint64_t>(iBranch.id)};
		while (!pending.empty()) {
			const auto node = pending.back();
			pending.pop_back();
			for (const auto next: successors(node))
				if (next != iMerge && region.insert(next).second)
					pending.push_back(next);
		}
		size_t inside = 0;
		for (const auto& [key, link]: m_execLinks)
			if (static_cast<uint64_t>(link->toNode) == iMerge && region.contains(key.first))
				++inside;
		return static_cast<int>(inside) == m_execInDegree[iMerge];
	}

	void emitBranch(const GraphNode& iNode, const NodePins& iPins) {
		const auto condition = bare(arguments(iNode, iPins).front());
		const auto merge = mergeOf(iNode);
		const bool outerStop = merge.has_value() && std::ranges::contains(m_stops, *merge);
		const bool inlineMerge =
				merge.has_value() && !outerStop && (m_function != nullptr || isOwnMerge(iNode, *merge));
		const auto runs = [&](const std::string& iPin) -> bool {
			const auto link = m_execLinks.find({iNode.id, iPin});
			return link != m_execLinks.end() &&
				   (!(outerStop || inlineMerge) || static_cast<uint64_t>(link->second->toNode) != *merge);
		};
		const bool onTrue = runs("True");
		const bool onFalse = runs("False");
		if (outerStop || inlineMerge)
			m_stops.push_back(*merge);
		if (onTrue || onFalse) {
			if (onTrue)
				line(std::format("if {} then", condition));
			else
				line(std::format("if not {} then", isAtomic(condition) ? condition : "(" + condition + ")"));
			++m_indent;
			emitChain(iNode, onTrue ? "True" : "False");
			--m_indent;
			if (onTrue && onFalse) {
				line("else");
				++m_indent;
				emitChain(iNode, "False");
				--m_indent;
			}
			line("end");
		}
		if (outerStop || inlineMerge)
			m_stops.pop_back();
		if (inlineMerge)
			emitNode(*merge);
	}

	// A node the chain reaches by the join of a branch, written once after the branch.
	void emitNode(const uint64_t iNode) {
		const auto* target = m_script.findNode(core::UUID{iNode});
		const auto* type = target == nullptr ? nullptr : findNodeType(target->type);
		if (type == nullptr)
			return;
		if (m_execStack.contains(target->id)) {
			error(target->id, "The execution loops back into this node: use a For Loop.");
			return;
		}
		m_execStack.insert(target->id);
		if (out().text.size() <= g_maxSourceSize)
			emitStatement(*target, *type);
		m_execStack.erase(target->id);
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
			const CompilerSetup setup{.mode = CompileMode::Game,
									  .usage = nullptr,
									  .prefix = std::format("__l{}_", index),
									  .slotTable = std::format("__vsl{}", index),
									  .library = true};
			Compiler analysis{*graph, setup};
			std::ignore = analysis.runLibrary();
			auto second = setup;
			second.usage = &analysis.getUsage();
			Compiler compiler{*graph, second};
			auto code = compiler.runLibrary();
			if (!code) {
				error(node.id, std::format("The library '{}' does not compile: {}", library->second,
										   code.error().front().message));
				continue;
			}
			out().text += *code;
			out().lines += static_cast<int>(std::ranges::count(*code, '\n'));
		}
	}

	// The functions of the graph, as locals declared first so that they may call each other.
	void emitFunctions() {
		const auto saved = std::exchange(m_part, Part::Functions);
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
		std::set<std::string> names;
		for (const auto& variable: m_script.variables) names.insert(variable.name);
		for (const auto& type: getNodeTypes())
			if (type.kind == NodeKind::Event)
				names.insert(type.lua);
		for (const auto& node: m_script.nodes)
			if (const auto name = node.values.find("function");
				node.type == "event.custom" && name != node.values.end())
				names.insert(name->second);
		for (const auto& custom: m_script.customNodes)
			if (!names.insert(custom.name).second)
				error(0, std::format("'{}' names two things of the graph: give each its own name.", custom.name));
		for (const auto& function: m_script.functions) {
			if (!isValidName(function.name))
				error(0, std::format("'{}' is not a usable function name.", function.name));
			else if (!names.insert(function.name).second)
				error(0, std::format("'{}' names two things of the graph: give each its own name.", function.name));
			if (function.exported && function.macro)
				error(0, std::format("The macro '{}' cannot be exported: only a function returns to the engine.",
									 function.name));
			if (!function.exported || !m_prefix.empty())
				m_declarations.push_back(m_prefix + function.name);
		}
		for (const auto& function: m_script.functions) {
			const auto entry = entries.find(function.name);
			openScope();
			std::vector<std::string> params;
			params.reserve(function.inputs.size());
			for (const auto& input: function.inputs) params.push_back(allocate(input.name));
			openBody(std::format("function {}{}({})", m_prefix, function.name, joined(params)));
			if (entry != entries.end()) {
				m_function = &function;
				for (size_t i = 0; i < function.inputs.size(); ++i)
					bindParameter(entry->second->id, function.inputs[i].name, params[i]);
				emitChain(*entry->second, "Then");
				m_function = nullptr;
			}
			closeBody();
		}
		m_part = saved;
	}

	void emitReturn(const GraphNode& iNode, const NodePins& iPins) {
		const auto* function = findNodeFunction(m_script, iNode);
		if (function == nullptr || function != m_function) {
			error(iNode.id, "The Return node runs outside its function.");
			return;
		}
		auto values = bareAll(arguments(iNode, iPins));
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

	// Results of a call: their storage, a placeholder local for those no node reads, the trailing ones left out.
	auto resultTargets(const core::UUID iNode, const std::vector<std::string>& iPins,
					   std::vector<std::string>& ioLocals) -> std::vector<std::string> {
		std::vector<std::string> results;
		results.reserve(iPins.size());
		for (const auto& pin: iPins) results.push_back(target(iNode, pin).value_or("_"));
		while (!results.empty() && results.back() == "_") results.pop_back();
		if (!std::ranges::contains(results, std::string{"_"}))
			return results;
		const auto placeholder = statementLocal("_");
		ioLocals.push_back(placeholder);
		for (auto& result: results)
			if (result == "_")
				result = placeholder;
		return results;
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
		const auto call = std::format("{}{}({})", prefix, function->name, joined(bareAll(arguments(iNode, iPins))));
		std::vector<std::string> pins;
		pins.reserve(function->outputs.size());
		for (const auto& output: function->outputs) pins.push_back(output.name);
		std::vector<std::string> locals;
		if (function->exits.size() <= 1) {
			auto results = resultTargets(iNode.id, pins, locals);
			if (!locals.empty())
				line(std::format("local {}", joined(locals)));
			line(results.empty() ? call : std::format("{} = {}", joined(results), call));
			return false;
		}
		const auto exit = statementLocal("exit");
		auto results = resultTargets(iNode.id, pins, locals);
		locals.insert(locals.begin(), exit);
		results.insert(results.begin(), exit);
		line(std::format("local {}", joined(locals)));
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

	// An execution call: its results stored, a grouped output assembled into its table from its components.
	void emitCall(const GraphNode& iNode, const std::string& iCall, const NodePins& iPins) {
		struct Group {
			PinType type;
			std::string target;
			std::vector<std::string> parts;
		};
		std::vector<std::string> locals;
		std::vector<std::string> results;
		std::vector<Group> groups;
		for (const auto& pin: iPins.outputs) {
			if (pin.type == PinType::Exec)
				continue;
			const auto stored = target(iNode.id, pin.name);
			if (pin.group != pin.name) {
				results.push_back(stored.value_or("_"));
				continue;
			}
			Group group{.type = pin.type, .target = stored.value_or(""), .parts = {}};
			for (const auto& component: componentsOf(pin)) {
				group.parts.push_back(stored.has_value() ? statementLocal(component) : std::string{"_"});
				if (stored.has_value())
					locals.push_back(group.parts.back());
				results.push_back(group.parts.back());
			}
			if (stored.has_value())
				groups.push_back(std::move(group));
		}
		while (!results.empty() && results.back() == "_") results.pop_back();
		if (std::ranges::contains(results, std::string{"_"})) {
			const auto placeholder = statementLocal("_");
			for (auto& result: results)
				if (result == "_")
					result = placeholder;
			locals.insert(locals.begin(), placeholder);
		}
		if (!locals.empty())
			line(std::format("local {}", joined(locals)));
		line(results.empty() ? iCall : std::format("{} = {}", joined(results), iCall));
		for (const auto& group: groups)
			line(std::format("{} = {}", group.target, componentTable(group.type, group.parts)));
	}

	void emitLoop(const GraphNode& iNode, const NodePins& iPins) {
		const auto bounds = bareAll(arguments(iNode, iPins));
		const auto index = allocate("i");
		line(std::format("for {} = {}, {} do", index, bounds[0], bounds[1]));
		++m_indent;
		if (const auto stored = target(iNode.id, "Index"); stored.has_value())
			line(std::format("{} = {}", *stored, index));
		emitChain(iNode, "Body");
		--m_indent;
		line("end");
	}

	// `for i, item in ipairs(list)`: the loop variables are the outputs, copied out when read after the loop.
	void emitForEach(const GraphNode& iNode, const NodePins& iPins) {
		const auto list = bare(arguments(iNode, iPins).front());
		const std::array<std::pair<std::string, std::string_view>, 2> outputs{{{"Index", "i"}, {"Item", "item"}}};
		std::vector<std::string> names;
		names.reserve(outputs.size());
		for (const auto& [pin, base]: outputs)
			names.push_back(m_debug || isRead({iNode.id, pin}) ? allocate(base) : std::string{"_"});
		auto declared = names;
		while (declared.size() > 1 && declared.back() == "_") declared.pop_back();
		line(std::format("for {} in ipairs({}) do", joined(declared), list));
		++m_indent;
		for (size_t i = 0; i < outputs.size(); ++i) {
			const PinKey key{iNode.id, outputs[i].first};
			if (names[i] == "_")
				continue;
			if (m_debug || (m_usage != nullptr && m_usage->escaped.contains(key))) {
				if (const auto stored = target(iNode.id, key.second); stored.has_value())
					line(std::format("{} = {}", *stored, names[i]));
			} else {
				bindParameter(iNode.id, key.second, names[i]);
			}
		}
		emitChain(iNode, "Body");
		--m_indent;
		line("end");
		for (const auto& [pin, base]: outputs) {
			const PinKey key{iNode.id, pin};
			if (const auto local = m_scope.locals.find(key);
				local != m_scope.locals.end() && std::ranges::contains(names, local->second))
				m_scope.locals.erase(local);
			m_closedLoops.insert(key);
		}
	}

	// The parts joined in order, the node lines shifted by the lines of the parts before theirs.
	auto assemble() -> std::string {
		{
			const auto saved = std::exchange(m_part, Part::Declarations);
			if (m_debug && !m_library) {
				line("local __vs = {}");
				line("__owl_probes = __vs");
			} else if (m_usesSlotTable) {
				line(std::format("local {} = {{}}", m_slotTable));
			}
			if (!m_declarations.empty())
				line(std::format("local {}", joined(m_declarations)));
			m_part = saved;
		}
		std::array<int, g_partCount> offsets{};
		std::string lua;
		int lines = 0;
		for (size_t i = 0; i < g_partCount; ++i) {
			offsets[i] = lines;
			lua += m_parts[i].text;
			lines += m_parts[i].lines;
		}
		for (const auto& [node, marks]: m_partLines)
			for (const auto& [part, number]: marks)
				m_nodeLines[node].push_back(number + offsets[static_cast<size_t>(part)]);
		return m_debug ? lua : tidyReturns(lua);
	}
};
// NOLINTEND(misc-no-recursion)

// Two passes: the first finds the function that reads each value, the second keeps in locals what stays in one.
auto compileGraph(const VisualScript& iScript, const CompileMode iMode, std::map<core::UUID, std::vector<int>>* oLines)
		-> expected<std::string, std::vector<CompileDiagnostic>> {
	Compiler analysis{iScript, {.mode = iMode, .usage = nullptr, .prefix = {}, .slotTable = "__vs", .library = false}};
	std::ignore = analysis.run();
	Compiler compiler{
			iScript,
			{.mode = iMode, .usage = &analysis.getUsage(), .prefix = {}, .slotTable = "__vs", .library = false}};
	auto lua = compiler.run();
	if (lua && oLines != nullptr)
		*oLines = std::move(compiler.getNodeLines());
	return lua;
}

}// namespace

auto getProbeKey(const core::UUID iNode, const std::string_view iPin) -> std::string {
	return std::format("{}:{}", static_cast<uint64_t>(iNode), iPin);
}

auto compile(const VisualScript& iScript, const CompileMode iMode)
		-> expected<std::string, std::vector<CompileDiagnostic>> {
	return compileGraph(iScript, iMode, nullptr);
}

auto compileForDebug(const VisualScript& iScript) -> expected<DebugBuild, std::vector<CompileDiagnostic>> {
	std::map<core::UUID, std::vector<int>> lines;
	auto lua = compileGraph(iScript, CompileMode::Debug, &lines);
	if (!lua)
		return unexpected{std::move(lua.error())};
	return DebugBuild{.lua = std::move(*lua), .nodeLines = std::move(lines)};
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
