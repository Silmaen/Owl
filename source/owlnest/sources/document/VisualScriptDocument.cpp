/**
 * @file VisualScriptDocument.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VisualScriptDocument.h"

#include "EditorLayer.h"
#include "codeEditor/LanguageDefinitions.h"
#include "external/imgui_text_edit.h"

#include <app/Application.h>
#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/NodeStyle.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>
#include <script/ScriptTypeChecker.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

namespace vs = script::visual;

// One checker for every graph: building it loads the declarations of the engine once.
auto typeChecker() -> script::ScriptTypeChecker& {
	static script::ScriptTypeChecker s_checker;
	return s_checker;
}

class ModifyVisualScriptCommand final : public UndoCommand<vs::VisualScript> {
public:
	ModifyVisualScriptCommand(vs::VisualScript iBefore, vs::VisualScript iAfter, std::string iDescription,
							  const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(vs::VisualScript& ioTarget) override { ioTarget = m_before; }

	void redo(vs::VisualScript& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyVisualScriptCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	vs::VisualScript m_before;
	vs::VisualScript m_after;
	std::string m_description;
	bool m_merge;
};

auto valueOf(const vs::GraphNode& iNode, const std::string& iKey) -> std::string {
	const auto it = iNode.values.find(iKey);
	return it == iNode.values.end() ? std::string{} : it->second;
}

auto containsNoCase(const std::string_view iText, const std::string_view iNeedle) -> bool {
	return std::ranges::search(iText, iNeedle, [](const char iLeft, const char iRight) -> bool {
			   return std::tolower(static_cast<unsigned char>(iLeft)) ==
					  std::tolower(static_cast<unsigned char>(iRight));
		   }).begin() != iText.end();
}

auto isSelected(const std::vector<core::UUID>& iSelection, const core::UUID iNode) -> bool {
	return std::ranges::find(iSelection, iNode) != iSelection.end();
}

constexpr std::array<script::ScriptPropertyType, 10> g_variableTypes{
		script::ScriptPropertyType::Float,  script::ScriptPropertyType::Int,    script::ScriptPropertyType::String,
		script::ScriptPropertyType::Bool,   script::ScriptPropertyType::Entity, script::ScriptPropertyType::Vec2,
		script::ScriptPropertyType::Vec3,   script::ScriptPropertyType::Color,  script::ScriptPropertyType::Numbers,
		script::ScriptPropertyType::Strings};

// Palette groups in reading order; the binding tables follow.
constexpr std::array<std::string_view, 11> g_categoryOrder{
		"Events", "Flow", "Variables", "Constant", "Math", "Vector", "Color", "Compare", "Logic", "Text", "Table"};

auto categoryRank(const std::string& iCategory) -> size_t {
	const auto* const it = std::ranges::find(g_categoryOrder, iCategory);
	return static_cast<size_t>(std::distance(g_categoryOrder.begin(), it));
}

auto isLinked(const vs::VisualScript& iScript, const core::UUID iNode, const std::string& iPin, const bool iOutput)
		-> bool {
	return std::ranges::any_of(iScript.links, [&](const vs::GraphLink& iLink) -> bool {
		return iOutput ? iLink.fromNode == iNode && iLink.fromPin == iPin
					   : iLink.toNode == iNode && iLink.toPin == iPin;
	});
}

// The pin of iPins that takes a link from iFrom (or gives one to it), the same type first.
auto findCompatiblePin(const std::vector<vs::PinSpec>& iPins, const VisualScriptPinRef& iFrom) -> const vs::PinSpec* {
	const vs::PinSpec* found = nullptr;
	for (const auto& pin: iPins) {
		if (iFrom.output ? !vs::canConnect(iFrom.type, pin.type) : !vs::canConnect(pin.type, iFrom.type))
			continue;
		if (pin.type == iFrom.type)
			return &pin;
		if (found == nullptr)
			found = &pin;
	}
	return found;
}

// Pins a grouped pin of the node stands for, whether it is split or not.
auto componentNames(const vs::VisualScript& iScript, const vs::GraphNode& iNode, const std::string& iGroup)
		-> std::vector<std::string> {
	std::vector<std::string> out;
	const auto pins = vs::getNodePins(iScript, iNode);
	for (const auto* side: {&pins.inputs, &pins.outputs})
		for (const auto& pin: *side) {
			if (pin.group != iGroup)
				continue;
			if (pin.name == iGroup)
				return pin.components;
			out.push_back(pin.name);
		}
	return out;
}

// An entry of a typed combo: its text and the type giving its glyph and colour.
struct TypedItem {
	// Text of the entry.
	std::string label;
	// Type of the entry.
	vs::PinType type{vs::PinType::Any};
};

void typedLabel(const TypedItem& iItem) {
	ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(getPinColor(iItem.type)));
	ImGui::TextUnformatted(getPinGlyph(iItem.type));
	ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);
	ImGui::TextUnformatted(iItem.label.c_str());
	ImGui::PopStyleColor();
}

// Combo of the inspector style whose entries show the glyph and colour of their type; returns the entry picked.
auto typedCombo(const char* iId, const std::vector<TypedItem>& iItems, const std::optional<size_t> iCurrent)
		-> std::optional<size_t> {
	std::optional<size_t> picked;
	if (gui::widgets::beginCombo(iId, "", ImGuiComboFlags_CustomPreview)) {
		for (size_t i = 0; i < iItems.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			const bool selected = iCurrent == i;
			const ImVec2 start = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable("##entry", selected, ImGuiSelectableFlags_None, {0.f, ImGui::GetFrameHeight()}))
				picked = i;
			if (selected)
				ImGui::SetItemDefaultFocus();
			const ImVec2 end = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos({start.x + ImGui::GetStyle().FramePadding.x,
									   start.y + (ImGui::GetFrameHeight() - ImGui::GetTextLineHeight()) * 0.5f});
			typedLabel(iItems[i]);
			ImGui::SetCursorScreenPos(end);
			ImGui::Dummy({0.f, 0.f});
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginComboPreview()) {
		if (iCurrent.has_value() && *iCurrent < iItems.size())
			typedLabel(iItems[*iCurrent]);
		ImGui::EndComboPreview();
	}
	return picked;
}

// Edits a value typed as `a, b, c` with a colour or vector field; returns the new text when it changed.
auto componentField(const char* iId, const vs::PinType iType, const size_t iCount, const std::string& iValue)
		-> std::optional<std::string> {
	std::array<float, 4> values{};
	values.fill(iType == vs::PinType::Color ? 1.f : 0.f);
	size_t start = 0;
	for (size_t i = 0; i < iCount && start <= iValue.size(); ++i) {
		const auto end = std::min(iValue.find(',', start), iValue.size());
		try {
			values[i] = std::stof(iValue.substr(start, end - start));
		} catch (const std::exception&) { break; }
		start = end + 1;
	}
	bool changed = false;
	if (iType == vs::PinType::Color)
		changed = iCount < 4 ? ImGui::ColorEdit3(iId, values.data()) : ImGui::ColorEdit4(iId, values.data());
	else
		changed = gui::widgets::vectorField(iId, std::span{values}.first(std::min<size_t>(iCount, 3)), 0.05f);
	if (!changed)
		return std::nullopt;
	std::string out;
	for (size_t i = 0; i < iCount; ++i) out += std::format("{}{:.3g}", i == 0 ? "" : ", ", values[i]);
	return out;
}

// Numbers of a transform or a matrix typed as `a, b; c, d`: as many as asked, the identity where the text has none.
auto parseCompound(const std::string& iValue, const bool iMatrix) -> std::vector<float> {
	std::vector<float> values = iMatrix ? std::vector<float>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}
										: std::vector<float>{0, 0, 0, 0, 0, 0, 1, 1, 1};
	size_t index = 0;
	std::string part;
	for (size_t i = 0; i <= iValue.size() && index < values.size(); ++i) {
		if (i < iValue.size() && iValue[i] != ',' && iValue[i] != ';' && iValue[i] != ' ') {
			part += iValue[i];
			continue;
		}
		if (part.empty())
			continue;
		float value = 0.f;
		if (std::from_chars(part.data(), part.data() + part.size(), value).ec == std::errc{})
			values[index] = value;
		++index;
		part.clear();
	}
	return values;
}

// Edits a transform (position, rotation shown in degrees, scale) or a matrix, row by row; the new text when it changed.
auto compoundField(const char* iId, const bool iMatrix, const std::string& iValue) -> std::optional<std::string> {
	auto values = parseCompound(iValue, iMatrix);
	const size_t width = iMatrix ? 4 : 3;
	bool changed = false;
	ImGui::PushID(iId);
	for (size_t row = 0; row < values.size() / width; ++row) {
		ImGui::PushID(static_cast<int>(row));
		auto cells = std::span{values}.subspan(row * width, width);
		const bool degrees = !iMatrix && row == 1;
		std::array<float, 4> shown{};
		for (size_t i = 0; i < width; ++i) shown[i] = degrees ? cells[i] * 57.29578f : cells[i];
		if (!iMatrix)
			ImGui::TextDisabled("%s", std::array{"Position", "Rotation (deg)", "Scale"}[row]);
		if (gui::widgets::vectorField("##row", std::span{shown}.first(width), degrees ? 1.f : 0.05f)) {
			for (size_t i = 0; i < width; ++i) cells[i] = degrees ? shown[i] / 57.29578f : shown[i];
			changed = true;
		}
		ImGui::PopID();
	}
	if (iMatrix && ImGui::SmallButton("Identity")) {
		values = parseCompound({}, true);
		changed = true;
	}
	ImGui::PopID();
	if (!changed)
		return std::nullopt;
	std::string out;
	for (size_t i = 0; i < values.size(); ++i) {
		const char* separator = i % width == 0 ? "; " : ", ";
		out += std::format("{}{:.6g}", i == 0 ? "" : separator, values[i]);
	}
	return out;
}

// Items of a list typed as `a, b, "c, d"`: the commas outside quotes separate them, each item trimmed.
auto splitListItems(const std::string& iText) -> std::vector<std::string> {
	std::vector<std::string> items;
	std::string item;
	bool quoted = false;
	const auto flush = [&]() -> void {
		const auto first = item.find_first_not_of(' ');
		items.push_back(first == std::string::npos ? std::string{}
												   : item.substr(first, item.find_last_not_of(' ') - first + 1));
		item.clear();
	};
	for (const char character: iText) {
		if (character == '"')
			quoted = !quoted;
		if (character == ',' && !quoted) {
			flush();
			continue;
		}
		item += character;
	}
	if (!iText.empty())
		flush();
	return items;
}

// Edits a list one item per row: change, move up or down, remove, add at the end; the new text when it changed.
auto listField(const char* iId, const std::string& iValue) -> std::optional<std::string> {
	auto items = splitListItems(iValue);
	bool changed = false;
	std::optional<size_t> removed;
	std::optional<size_t> raised;
	ImGui::PushID(iId);
	for (size_t i = 0; i < items.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%zu", i + 1);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-3.f * ImGui::GetFrameHeight());
		changed |= ImGui::InputText("##item", &items[i]);
		ImGui::SameLine();
		ImGui::BeginDisabled(i == 0);
		if (ImGui::SmallButton(gui::icon::arrowUp))
			raised = i;
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::SmallButton(gui::icon::minus))
			removed = i;
		ImGui::PopID();
	}
	if (gui::widgets::button("##addItem", {.glyph = gui::icon::plus, .label = "Add Item", .width = -1.f})) {
		items.emplace_back(items.empty() ? "0" : items.back());
		changed = true;
	}
	ImGui::PopID();
	if (raised.has_value()) {
		std::swap(items[*raised], items[*raised - 1]);
		changed = true;
	}
	if (removed.has_value()) {
		items.erase(items.begin() + static_cast<std::ptrdiff_t>(*removed));
		changed = true;
	}
	if (!changed)
		return std::nullopt;
	std::string out;
	for (const auto& item: items) out += (out.empty() ? "" : ", ") + item;
	return out;
}

// Value field of a typed input; returns the new text when it changed.
auto valueField(const char* iId, const vs::PinType iType, const size_t iCount, const std::string& iValue,
				const std::string& iDefault) -> std::optional<std::string> {
	switch (iType) {
		case vs::PinType::Boolean:
			{
				bool checked = (iValue.empty() ? iDefault : iValue) == "true";
				if (gui::widgets::toggle(iId, &checked))
					return checked ? "true" : "false";
				return std::nullopt;
			}
		case vs::PinType::Color:
		case vs::PinType::Vector2:
		case vs::PinType::Vector3:
			return componentField(iId, iType, iCount, iValue);
		case vs::PinType::Transform:
		case vs::PinType::Matrix:
			return compoundField(iId, iType == vs::PinType::Matrix, iValue);
		case vs::PinType::Exec:
			return std::nullopt;
		case vs::PinType::Table:
		case vs::PinType::Integer:
		case vs::PinType::Number:
		case vs::PinType::String:
		case vs::PinType::Entity:
		case vs::PinType::Any:
			break;
	}
	// A list is edited item by item; a table with fields (`key = value`) stays a text.
	if (iType == vs::PinType::Table && iValue.find('=') == std::string::npos)
		return listField(iId, iValue);
	auto text = iValue;
	const auto hint = iType == vs::PinType::Entity ? std::string{"self"} : iDefault;
	if (ImGui::InputTextWithHint(iId, hint.c_str(), &text))
		return text;
	return std::nullopt;
}

// A bound typed in a field: nullopt when the text is not a number.
auto parseBound(const std::string& iText) -> std::optional<float> {
	float value = 0.f;
	const auto [end, error] = std::from_chars(iText.data(), iText.data() + iText.size(), value);
	if (error != std::errc{} || end != iText.data() + iText.size())
		return std::nullopt;
	return value;
}

}// namespace


VisualScriptDocument::VisualScriptDocument() { m_savedText = vs::serialize(m_script); }

VisualScriptDocument::~VisualScriptDocument() = default;

auto VisualScriptDocument::starterScript() -> vs::VisualScript {
	vs::VisualScript script;
	script.nodes.push_back({.id = core::UUID{}, .type = "event.on_create", .position = {0.f, 0.f}, .values = {}});
	script.nodes.push_back({.id = core::UUID{}, .type = "event.on_update", .position = {0.f, 160.f}, .values = {}});
	return script;
}

auto VisualScriptDocument::title() const -> std::string {
	return m_path.empty() ? std::string{"Untitled"} : m_path.filename().string();
}

auto VisualScriptDocument::isDirty() const -> bool { return vs::serialize(m_script) != m_savedText; }

auto VisualScriptDocument::recoverySnapshot() const -> std::optional<std::string> { return vs::serialize(m_script); }

auto VisualScriptDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto script = vs::deserialize(iSnapshot, title());
	if (!script) {
		OWL_WARN("Recovery: Cannot restore the autosave of visual script '{}'.", title())
		return false;
	}
	m_script = std::move(*script);
	m_undo.clear();
	m_compileStale = true;
	m_view.requestFrameAll();
	return true;
}

auto VisualScriptDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	const auto text = vs::serialize(m_script);
	if (!platform::writeFileAtomic(iPath, text)) {
		OWL_ERROR("VisualScriptDocument: Cannot write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_savedText = text;
	m_undo.markSaved();
	vs::clearLibraryCache();
	return true;
}

auto VisualScriptDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto script = vs::deserialize(platform::fileToString(iPath), iPath.string());
	if (!script) {
		OWL_WARN("VisualScriptDocument: '{}' is not a visual script.", iPath.string())
		return false;
	}
	m_script = std::move(*script);
	m_path = iPath;
	m_savedText = vs::serialize(m_script);
	m_undo.clear();
	m_undo.markSaved();
	m_compileStale = true;
	m_view.requestFrameAll();
	return true;
}

void VisualScriptDocument::onAttach(EditorLayer* iEditor) { NodeGraphDocument::onAttach(iEditor); }

void VisualScriptDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_script);
	m_compileStale = true;
}

void VisualScriptDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_script);
	m_compileStale = true;
}

auto VisualScriptDocument::commit(vs::VisualScript iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_script)
		return false;
	auto before = std::move(m_script);
	m_script = iNext;
	m_undo.push(mkUniq<ModifyVisualScriptCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	m_compileStale = true;
	return true;
}

void VisualScriptDocument::compileIfStale() {
	if (!m_compileStale)
		return;
	m_compileStale = false;
	m_checked = false;
	auto compiled = vs::compile(m_script);
	m_diagnostics = compiled ? std::vector<vs::CompileDiagnostic>{} : compiled.error();
	if (!compiled)
		return;
	m_compiledLua = std::move(*compiled);
	if (auto debug = vs::compileForDebug(m_script); debug)
		m_nodeLines = std::move(debug->nodeLines);
	std::erase_if(m_breakpoints,
				  [this](const core::UUID iNode) -> bool { return m_script.findNode(iNode) == nullptr; });
	syncBreakpoints();
}

void VisualScriptDocument::syncBreakpoints() const {
	if (m_path.empty())
		return;
	std::vector<int> lines;
	for (const auto node: m_breakpoints)
		if (const auto found = m_nodeLines.find(node); found != m_nodeLines.end())
			lines.insert(lines.end(), found->second.begin(), found->second.end());
	script::ScriptDebugger::setBreakpoints(m_path.string(), lines);
}

void VisualScriptDocument::toggleBreakpoint(const core::UUID iNode) {
	if (m_script.findNode(iNode) == nullptr)
		return;
	if (m_breakpoints.erase(iNode) == 0)
		m_breakpoints.insert(iNode);
	compileIfStale();
	syncBreakpoints();
	auto view = m_view.getDebugView();
	view.breakpoints = m_breakpoints;
	m_view.setDebugView(std::move(view));
}

auto VisualScriptDocument::findNodeAtLine(const int iLine) -> std::optional<core::UUID> {
	compileIfStale();
	for (const auto& [node, lines]: m_nodeLines)
		if (std::ranges::contains(lines, iLine))
			return node;
	return std::nullopt;
}

void VisualScriptDocument::revealLine(const int iLine) {
	if (const auto node = findNodeAtLine(iLine); node.has_value()) {
		m_view.setSelection({*node});
		m_view.requestFocus(*node);
	}
}

void VisualScriptDocument::setDebugState(const std::vector<script::WatchValue>& iValues,
										 const std::optional<int> iStoppedLine) {
	VisualScriptDebugView view{.breakpoints = m_breakpoints, .stopped = std::nullopt, .values = {}, .entries = {}};
	for (const auto& value: iValues) {
		view.values.emplace(value.name, value.value);
		if (value.entries.empty())
			continue;
		auto& entries = view.entries[value.name];
		for (const auto& entry: value.entries) entries.emplace_back(entry.name, entry.value);
	}
	if (iStoppedLine.has_value())
		view.stopped = findNodeAtLine(*iStoppedLine);
	m_view.setDebugView(std::move(view));
}

auto VisualScriptDocument::addNode(const std::string& iType, const std::array<float, 2> iPosition,
								   const std::optional<VisualScriptPinRef>& iLinkFrom) -> core::UUID {
	const auto separator = iType.find('|');
	const auto typeId = iType.substr(0, separator);
	const auto* type = vs::findNodeType(typeId);
	if (type == nullptr) {
		OWL_WARN("VisualScriptDocument: Unknown node type '{}'.", iType)
		return core::UUID{0};
	}
	auto next = m_script;
	vs::GraphNode node{.id = core::UUID{}, .type = typeId, .position = iPosition, .values = {}};
	if (type->kind == vs::NodeKind::GetVariable || type->kind == vs::NodeKind::SetVariable) {
		if (separator != std::string::npos)
			node.values["variable"] = iType.substr(separator + 1);
		else if (!m_script.variables.empty())
			node.values["variable"] = m_script.variables.front().name;
	} else if (type->kind == vs::NodeKind::CustomEvent) {
		node.values["function"] = "my_event";
	} else if (type->kind == vs::NodeKind::FunctionEntry || type->kind == vs::NodeKind::FunctionReturn ||
			   type->kind == vs::NodeKind::FunctionCall) {
		const auto rest = separator == std::string::npos ? std::string{} : iType.substr(separator + 1);
		const auto split = rest.find('|');
		node.values["function"] = rest.substr(0, split);
		if (split != std::string::npos)
			node.values["library"] = rest.substr(split + 1);
		if (const auto* function = vs::findNodeFunction(next, node);
			function != nullptr && function->exits.size() > 1 && type->kind == vs::NodeKind::FunctionReturn)
			node.values["exit"] = function->exits.front();
	} else if (type->kind == vs::NodeKind::CustomCall) {
		if (separator != std::string::npos)
			node.values["custom"] = iType.substr(separator + 1);
	} else if (type->kind == vs::NodeKind::LuaCall && separator != std::string::npos) {
		const auto rest = iType.substr(separator + 1);
		const auto split = rest.find('|');
		node.values["module"] = rest.substr(0, split);
		if (split != std::string::npos)
			node.values["function"] = rest.substr(split + 1);
	}
	const auto id = node.id;
	if (iLinkFrom.has_value()) {
		const auto pins = vs::getNodePins(next, node);
		if (const auto* pin = findCompatiblePin(iLinkFrom->output ? pins.inputs : pins.outputs, *iLinkFrom);
			pin != nullptr) {
			const VisualScriptPinRef created{.node = id,
											 .pin = pin->name,
											 .output = !iLinkFrom->output,
											 .type = pin->type};
			const auto& from = iLinkFrom->output ? *iLinkFrom : created;
			const auto& to = iLinkFrom->output ? created : *iLinkFrom;
			std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
				if (from.type == vs::PinType::Exec)
					return iLink.fromNode == from.node && iLink.fromPin == from.pin;
				return iLink.toNode == to.node && iLink.toPin == to.pin;
			});
			next.links.push_back(
					{.fromNode = from.node, .fromPin = from.pin, .toNode = to.node, .toPin = to.pin, .waypoints = {}});
		}
	}
	next.nodes.push_back(std::move(node));
	commit(std::move(next), "Add Node");
	m_view.setSelection({id});
	return id;
}

void VisualScriptDocument::deleteNodes(const std::span<const core::UUID> iNodes) {
	auto next = m_script;
	for (const auto id: iNodes) next.removeNode(id);
	commit(std::move(next), iNodes.size() > 1 ? "Delete Nodes" : "Delete Node");
}

void VisualScriptDocument::setPinSplit(const core::UUID iNode, const std::string& iGroup, const bool iSplit) {
	auto next = m_script;
	auto* node = next.findNode(iNode);
	if (node == nullptr)
		return;
	const auto key = std::string{vs::g_SplitPrefix} + iGroup;
	const auto components = componentNames(m_script, *m_script.findNode(iNode), iGroup);
	if (iSplit) {
		node->values[key] = "true";
		if (const auto it = node->values.find(iGroup); it != node->values.end()) {
			size_t start = 0;
			for (const auto& component: components) {
				const auto end = std::min(it->second.find(',', start), it->second.size());
				if (start < it->second.size()) {
					auto part = it->second.substr(start, end - start);
					std::erase(part, ' ');
					node->values[component] = part;
				}
				start = end + 1;
			}
			node->values.erase(iGroup);
		}
	} else {
		node->values.erase(key);
		std::string joined;
		bool complete = true;
		for (const auto& component: components) {
			const auto it = node->values.find(component);
			complete = complete && it != node->values.end();
			if (it != node->values.end()) {
				joined += (joined.empty() ? "" : ", ") + it->second;
				node->values.erase(it);
			}
		}
		if (complete)
			node->values[iGroup] = joined;
	}
	std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
		const auto touches = [&](const std::string& iPin) -> bool {
			return iSplit ? iPin == iGroup : std::ranges::find(components, iPin) != components.end();
		};
		return (iLink.fromNode == iNode && touches(iLink.fromPin)) || (iLink.toNode == iNode && touches(iLink.toPin));
	});
	commit(std::move(next), iSplit ? "Split Pin" : "Recombine Pin");
}

auto VisualScriptDocument::copyNodes(const std::span<const core::UUID> iNodes) const -> std::string {
	vs::VisualScript part;
	const std::set<uint64_t> ids(iNodes.begin(), iNodes.end());
	for (const auto& node: m_script.nodes) {
		if (!ids.contains(node.id))
			continue;
		part.nodes.push_back(node);
		if (const auto name = node.values.find("variable"); name != node.values.end())
			if (const auto* variable = m_script.findVariable(name->second);
				variable != nullptr && part.findVariable(variable->name) == nullptr)
				part.variables.push_back(*variable);
	}
	for (const auto& link: m_script.links)
		if (ids.contains(link.fromNode) && ids.contains(link.toNode))
			part.links.push_back(link);
	return vs::serialize(part);
}

auto VisualScriptDocument::pasteNodes(const std::string& iText, const std::array<float, 2> iOffset)
		-> std::vector<core::UUID> {
	auto part = vs::deserialize(iText, "clipboard");
	if (!part || part->nodes.empty())
		return {};
	auto next = m_script;
	std::unordered_map<uint64_t, core::UUID> fresh;
	std::vector<core::UUID> pasted;
	for (auto node: part->nodes) {
		const core::UUID id{};
		fresh[node.id] = id;
		node.id = id;
		node.position = {node.position[0] + iOffset[0], node.position[1] + iOffset[1]};
		pasted.push_back(id);
		next.nodes.push_back(std::move(node));
	}
	for (auto link: part->links) {
		link.fromNode = fresh[link.fromNode];
		link.toNode = fresh[link.toNode];
		next.links.push_back(std::move(link));
	}
	for (const auto& variable: part->variables)
		if (next.findVariable(variable.name) == nullptr)
			next.variables.push_back(variable);
	commit(std::move(next), "Paste Nodes");
	m_view.setSelection(pasted);
	return pasted;
}

void VisualScriptDocument::onImGuiRender() {
	compileIfStale();
	const auto winTitle = documentWindowTitle(DocumentType::NodeGraph, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
	if (isDirty())
		flags |= ImGuiWindowFlags_UnsavedDocument;
	if (const auto dockspaceId = ImGui::GetID("OwlDockSpace");
		const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockspaceId))
		ImGui::SetNextWindowDockID(centralNode->ID, ImGuiCond_FirstUseEver);
	const bool wantFocus = consumeFocusRequest();
	if (wantFocus)
		ImGui::SetNextWindowFocus();
	const bool open = ImGui::Begin(winTitle.c_str(), &m_pOpen, flags);
	if (mp_editorLayer != nullptr)
		mp_editorLayer->renderDocumentTabMenu(*this);
	if (wantFocus)
		ImGui::SetWindowFocus();
	if (open) {
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		renderToolbar();
		if (m_viewMode == VisualScriptViewMode::Code) {
			renderLua();
			renderContextMenu();
			ImGui::End();
			return;
		}
		if (m_viewMode == VisualScriptViewMode::Split) {
			const float width = std::max(ImGui::GetContentRegionAvail().x * 0.55f, 200.f);
			ImGui::BeginChild("##vsGraphPane", {width, 0.f}, ImGuiChildFlags_ResizeX);
		}
		m_view.setSnapToGrid(isGraphSnapOn(mp_editorLayer));
		auto shown = m_diagnostics;
		if (m_checked)
			shown.insert(shown.end(), m_checkResults.begin(), m_checkResults.end());
		const auto result = m_view.render(m_script, shown);
		if (m_viewMode == VisualScriptViewMode::Split) {
			ImGui::EndChild();
			ImGui::SameLine();
			if (ImGui::BeginChild("##vsCodePane"))
				renderLua();
			ImGui::EndChild();
		}
		if (result.edit.has_value())
			commit(result.edit->script, result.edit->description, result.edit->merge);
		if (result.droppedType.has_value())
			std::ignore = addNode(*result.droppedType, result.mouse);
		if (result.contextMenu || result.linkDropped.has_value()) {
			m_contextNode = result.contextNode;
			m_contextGroup = result.contextGroup;
			m_contextPin = result.contextPin;
			m_pendingLink = result.linkDropped;
			m_paletteTarget = result.mouse;
			m_paletteFilter.clear();
			m_openContextMenu = true;
		}
		if (focused && !ImGui::GetIO().WantTextInput)
			handleShortcuts(result.deleteUsed);
		renderContextMenu();
	}
	ImGui::End();
}

void VisualScriptDocument::renderToolbar() {
	const auto separator = []() -> void {
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
	};
	if (gui::widgets::iconButton("##vsFrame", {.glyph = gui::icon::focus,
											   .tooltip = "Frame All",
											   .shortcut = "Home",
											   .description = "Show every node.",
											   .disabled = m_viewMode == VisualScriptViewMode::Code}))
		m_view.requestFrameAll();
	ImGui::SameLine();
	graphSnapButton(mp_editorLayer);
	separator();
	constexpr std::array modes{
			std::tuple{VisualScriptViewMode::Graph, gui::icon::workflow, "Graph", "The nodes only."},
			std::tuple{VisualScriptViewMode::Split, gui::icon::columns3, "Split",
					   "The nodes and the generated Lua side by side."},
			std::tuple{VisualScriptViewMode::Code, gui::icon::code, "Lua", "The generated Lua only, read-only."}};
	for (const auto& [mode, glyph, label, tip]: modes) {
		ImGui::PushID(label);
		if (gui::widgets::iconButton(
					"##mode", {.glyph = glyph, .tooltip = label, .description = tip, .selected = m_viewMode == mode}))
			m_viewMode = mode;
		ImGui::PopID();
		ImGui::SameLine(0.f, 2.f);
	}
	separator();
	if (m_diagnostics.empty()) {
		gui::widgets::badge(std::format("{} Compiles to Lua", gui::icon::circleCheck).c_str(),
							gui::widgets::BadgeKind::Success);
	} else if (gui::widgets::button(
					   "##vsErrors",
					   {.glyph = gui::icon::triangleAlert,
						.label = std::format("{} error{}", m_diagnostics.size(), m_diagnostics.size() > 1 ? "s" : "")
										 .c_str(),
						.tooltip = "Compilation errors",
						.description = "Click to list them; click one to centre its node."})) {
		ImGui::OpenPopup("##vsErrorList");
	}
	ImGui::SameLine();
	if (gui::widgets::button("##vsCheck", {.glyph = gui::icon::check,
										   .label = "Check",
										   .tooltip = "Check the graph",
										   .description = "Type-check the generated Lua against the engine API; "
														  "click a problem to centre its node."})) {
		check();
		ImGui::OpenPopup("##vsCheckList");
	}
	if (ImGui::BeginPopup("##vsCheckList")) {
		gui::widgets::menuHeader("Check");
		if (m_checkResults.empty())
			gui::widgets::badge(std::format("{} No problem found", gui::icon::circleCheck).c_str(),
								gui::widgets::BadgeKind::Success);
		for (size_t i = 0; i < m_checkResults.size(); ++i) {
			const auto& found = m_checkResults[i];
			ImGui::PushID(static_cast<int>(i));
			if (gui::widgets::menuItem({.glyph = gui::icon::triangleAlert, .label = found.message.c_str()}) &&
				found.node != 0) {
				m_view.setSelection({found.node});
				m_view.requestFocus(found.node);
			}
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	if (gui::widgets::button("##vsGroupEvents",
							 {.glyph = gui::icon::layoutGrid,
							  .label = "Group by Events",
							  .tooltip = "Group by Events",
							  .description = "Frame each event and function outside a group with the nodes it runs."}))
		std::ignore = groupEvents();
	if (ImGui::BeginPopup("##vsErrorList")) {
		gui::widgets::menuHeader("Compilation errors");
		for (size_t i = 0; i < m_diagnostics.size(); ++i) {
			const auto& diagnostic = m_diagnostics[i];
			ImGui::PushID(static_cast<int>(i));
			if (gui::widgets::menuItem({.glyph = gui::icon::triangleAlert, .label = diagnostic.message.c_str()}) &&
				diagnostic.node != 0) {
				m_view.setSelection({diagnostic.node});
				m_view.requestFocus(diagnostic.node);
			}
			ImGui::PopID();
		}
		ImGui::EndPopup();
	}
}

void VisualScriptDocument::handleShortcuts(const bool iDeleteUsed) {
	const auto selection = m_view.getSelection();
	const bool ctrl = ImGui::GetIO().KeyCtrl;
	if (!iDeleteUsed && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !selection.empty()) {
		deleteNodes(selection);
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
		m_view.requestFrameAll();
	if (ImGui::IsKeyPressed(ImGuiKey_F9, false))
		for (const auto node: selection) toggleBreakpoint(node);
	if (!ctrl && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Q, false) && !selection.empty())
		std::ignore = straightenNodes(selection);
	if (!ctrl)
		return;
	if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
		std::vector<core::UUID> all;
		all.reserve(m_script.nodes.size());
		for (const auto& node: m_script.nodes) all.push_back(node.id);
		m_view.setSelection(std::move(all));
	} else if (ImGui::IsKeyPressed(ImGuiKey_C, false) && !selection.empty()) {
		ImGui::SetClipboardText(copyNodes(selection).c_str());
	} else if (ImGui::IsKeyPressed(ImGuiKey_X, false) && !selection.empty()) {
		ImGui::SetClipboardText(copyNodes(selection).c_str());
		deleteNodes(selection);
	} else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
		if (const char* text = ImGui::GetClipboardText(); text != nullptr) {
			const float shift = 30.f * static_cast<float>(++m_pasteCount);
			std::ignore = pasteNodes(text, {shift, shift});
		}
	} else if (ImGui::IsKeyPressed(ImGuiKey_D, false) && !selection.empty()) {
		std::ignore = pasteNodes(copyNodes(selection), {30.f, 30.f});
	} else if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !selection.empty()) {
		std::ignore = groupNodes(selection);
	}
}

void VisualScriptDocument::renderContextMenu() {
	if (m_openContextMenu) {
		ImGui::OpenPopup("##visualScriptMenu");
		m_openContextMenu = false;
	}
	if (!ImGui::BeginPopup("##visualScriptMenu")) {
		m_pendingLink.reset();
		return;
	}
	if (m_contextPin.has_value()) {
		const auto& pin = *m_contextPin;
		const auto* node = m_script.findNode(pin.node);
		const auto pins = node == nullptr ? vs::NodePins{} : vs::getNodePins(m_script, *node);
		const auto& side = pin.output ? pins.outputs : pins.inputs;
		const auto spec = std::ranges::find(side, pin.pin, &vs::PinSpec::name);
		gui::widgets::menuHeader(pin.pin.c_str(), "Pin");
		if (spec != side.end() && !spec->group.empty()) {
			if (spec->group == spec->name) {
				if (gui::widgets::menuItem({.glyph = gui::icon::split, .label = "Split Pin"}))
					setPinSplit(pin.node, spec->group, true);
			} else if (gui::widgets::menuItem({.glyph = gui::icon::merge,
											   .label = std::format("Recombine as {}", spec->group).c_str()})) {
				setPinSplit(pin.node, spec->group, false);
			}
		}
		if (isLinked(m_script, pin.node, pin.pin, pin.output) &&
			gui::widgets::menuItem({.glyph = gui::icon::unlink, .label = "Break Links", .shortcut = "Alt+Click"})) {
			auto next = m_script;
			std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
				return pin.output ? iLink.fromNode == pin.node && iLink.fromPin == pin.pin
								  : iLink.toNode == pin.node && iLink.toPin == pin.pin;
			});
			commit(std::move(next), "Break Links");
		}
		ImGui::EndPopup();
		return;
	}
	if (const auto group = m_contextGroup.has_value()
								   ? std::ranges::find(m_script.groups, *m_contextGroup, &vs::GraphGroup::id)
								   : m_script.groups.end();
		group != m_script.groups.end()) {
		auto edited = *group;
		gui::widgets::menuHeader(group->title.empty() ? "Group" : group->title.c_str(), "Group");
		ImGui::SetNextItemWidth(220.f);
		if (ImGui::InputText("##groupTitle", &edited.title))
			setGroup(edited);
		if (const auto chosen = gui::widgets::linkColorSwatches(group->color); chosen.has_value()) {
			edited.color = *chosen;
			setGroup(edited);
		}
		if (gui::widgets::menuItem({.glyph = gui::icon::squareDashed, .label = "Ungroup"}))
			removeGroup(group->id, false);
		if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete with Its Nodes", .danger = true}))
			removeGroup(group->id, true);
		ImGui::EndPopup();
		return;
	}
	const auto* node = m_contextNode.has_value() ? m_script.findNode(*m_contextNode) : nullptr;
	if (node == nullptr) {
		renderNodeList(true);
		ImGui::EndPopup();
		return;
	}
	auto selection = m_view.getSelection();
	if (!isSelected(selection, node->id))
		selection = {node->id};
	const auto* type = vs::findNodeType(node->type);
	gui::widgets::menuHeader(type == nullptr ? node->type.c_str() : getNodeTitle(*node, *type).c_str(), "Node");
	if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
		std::ignore = pasteNodes(copyNodes(selection), {30.f, 30.f});
	if (gui::widgets::menuItem({.glyph = gui::icon::clipboardCopy, .label = "Copy", .shortcut = "Ctrl+C"}))
		ImGui::SetClipboardText(copyNodes(selection).c_str());
	if (gui::widgets::menuItem({.glyph = gui::icon::circleDot,
								.label = m_breakpoints.contains(node->id) ? "Remove Breakpoint" : "Add Breakpoint",
								.shortcut = "F9"}))
		for (const auto id: selection) toggleBreakpoint(id);
	if (gui::widgets::menuItem({.glyph = gui::icon::minus, .label = "Straighten Links", .shortcut = "Q"}))
		std::ignore = straightenNodes(selection);
	if (gui::widgets::menuItem({.glyph = gui::icon::squareDashed, .label = "Group", .shortcut = "Ctrl+G"}))
		std::ignore = groupNodes(selection);
	if (gui::widgets::menuItem({.glyph = gui::icon::squareFunction, .label = "Set as Function"}))
		std::ignore = collapseToFunction(selection);
	if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Delete", .danger = true}))
		deleteNodes(selection);
	ImGui::EndPopup();
}

void VisualScriptDocument::renderNodeSearch(const std::span<const vs::NodeType* const> iHeads,
											const PaletteEntry& iEntry,
											const std::function<void(const vs::NodeType&)>& iItem,
											const std::function<bool(const vs::NodeType&)>& iAccepts) {
	const auto& types = vs::getNodeTypes();
	const auto categoryLabel = [](const std::string& iCategory) -> std::string {
		return std::format("{}  {}", getCategoryGlyph(iCategory), iCategory);
	};
	// The matches stay under their category: a search for `vector` lists every node of the category.
	const auto matches = [&](const vs::NodeType& iType) -> bool {
		return containsNoCase(iType.title, m_paletteFilter) || containsNoCase(iType.description, m_paletteFilter) ||
			   containsNoCase(iType.category, m_paletteFilter);
	};
	std::vector<std::tuple<std::string, std::string, vs::NodeType>> functions;
	renderFunctionEntries([&](const std::string& iKey, const std::string& iLabel, const vs::NodeType& iType) -> void {
		if (containsNoCase(iLabel, m_paletteFilter))
			functions.emplace_back(iKey, iLabel, iType);
	});
	if (!functions.empty()) {
		ImGui::SeparatorText(categoryLabel("Functions").c_str());
		for (const auto& [key, label, type]: functions) iEntry(key, label, type);
	}
	for (const auto* head: iHeads) {
		const auto inCategory = [&](const vs::NodeType& iType) -> bool {
			return iType.category == head->category && iAccepts(iType) && matches(iType);
		};
		if (std::ranges::none_of(types, inCategory))
			continue;
		ImGui::SeparatorText(categoryLabel(head->category).c_str());
		for (const auto& type: types)
			if (inCategory(type))
				iItem(type);
	}
}

void VisualScriptDocument::renderNodeList(const bool iPopup) {
	if (iPopup) {
		gui::widgets::menuHeader(m_pendingLink.has_value() ? "Add Linked Node" : "Add Node", "Palette");
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
			// Read once per opening: the system clipboard is slow to query and Wayland reports every empty read.
			const char* text = ImGui::GetClipboardText();
			m_paletteClipboard = text != nullptr && std::string_view{text}.contains("VisualScript:") ? text : "";
		}
		ImGui::SetNextItemWidth(320.f);
		ImGui::InputTextWithHint("##paletteFilter", "Search nodes", &m_paletteFilter);
		if (!m_pendingLink.has_value() && !m_paletteClipboard.empty() &&
			gui::widgets::menuItem({.glyph = gui::icon::clipboardPaste, .label = "Paste Here", .shortcut = "Ctrl+V"})) {
			if (const auto part = vs::deserialize(m_paletteClipboard, "clipboard"); part && !part->nodes.empty()) {
				const auto& first = part->nodes.front().position;
				std::ignore =
						pasteNodes(m_paletteClipboard, {m_paletteTarget[0] - first[0], m_paletteTarget[1] - first[1]});
			}
			ImGui::CloseCurrentPopup();
			return;
		}
	} else {
		std::ignore = gui::widgets::searchField("##librarySearch", m_paletteFilter, "Search nodes");
	}
	const bool child = iPopup ? ImGui::BeginChild("##paletteList", {320.f, 380.f})
							  : ImGui::BeginChild("##libraryList", {0.f, 0.f});
	if (!child) {
		ImGui::EndChild();
		return;
	}
	const auto accepts = [this](const vs::NodeType& iType) -> bool {
		if (!m_pendingLink.has_value())
			return true;
		const vs::GraphNode probe{.id = core::UUID{0}, .type = iType.id, .position = {0.f, 0.f}, .values = {}};
		const auto pins = vs::getNodePins(m_script, probe);
		return findCompatiblePin(m_pendingLink->output ? pins.inputs : pins.outputs, *m_pendingLink) != nullptr;
	};
	const auto keyedItem = [&](const std::string& iKey, const std::string& iLabel, const vs::NodeType& iType) -> void {
		ImGui::PushID(iKey.c_str());
		const vs::GraphNode probe{.id = core::UUID{0}, .type = iType.id, .position = {0.f, 0.f}, .values = {}};
		const auto color = getNodeColor(m_script, probe, iType);
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(gui::Theme::mix(color, {1.f, 1.f, 1.f, 1.f}, 0.45f)));
		ImGui::TextUnformatted(getNodeGlyph(iType));
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Selectable(iLabel.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
			if (iPopup) {
				std::ignore = addNode(iKey, m_paletteTarget, m_pendingLink);
				m_pendingLink.reset();
				ImGui::CloseCurrentPopup();
			} else {
				const auto center = m_view.getViewCenter();
				std::ignore = addNode(iKey, {center[0] - 60.f, center[1] - 30.f});
			}
		}
		if (!iPopup && ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(g_VisualScriptNodePayload, iKey.data(), iKey.size());
			ImGui::Text("%s %s", getNodeGlyph(iType), iLabel.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered() && !iType.description.empty())
			ImGui::SetTooltip("%s\n%s", iType.title.c_str(), iType.description.c_str());
		ImGui::PopID();
	};
	const auto item = [&](const vs::NodeType& iType) -> void {
		const vs::GraphNode probe{.id = core::UUID{0}, .type = iType.id, .position = {0.f, 0.f}, .values = {}};
		keyedItem(iType.id, iType.kind == vs::NodeKind::Call ? getNodeTitle(probe, iType) : iType.title, iType);
	};
	const auto isFunctionNode = [](const vs::NodeType& iType) -> bool {
		return iType.category == "Functions" || iType.category == "Custom" || iType.category == "Lua";
	};
	refreshLibraries();
	const auto& types = vs::getNodeTypes();
	std::vector<const vs::NodeType*> heads;
	for (const auto& type: types)
		if (!isFunctionNode(type) && std::ranges::none_of(heads, [&](const vs::NodeType* iHead) -> bool {
				return iHead->category == type.category;
			}))
			heads.push_back(&type);
	std::ranges::stable_sort(heads, {},
							 [](const vs::NodeType* iHead) -> size_t { return categoryRank(iHead->category); });
	const auto categoryLabel = [](const std::string& iCategory) -> std::string {
		return std::format("{}  {}", getCategoryGlyph(iCategory), iCategory);
	};
	if (!m_paletteFilter.empty()) {
		renderNodeSearch(heads, keyedItem, item, accepts);
		ImGui::EndChild();
		return;
	}
	if (!m_pendingLink.has_value() &&
		(!m_script.functions.empty() || !m_script.customNodes.empty() || !m_libraries.empty() ||
		 !m_luaModules.empty()) &&
		ImGui::TreeNodeEx("Functions", ImGuiTreeNodeFlags_SpanAvailWidth, "%s  Functions", gui::icon::component)) {
		renderFunctionEntries(keyedItem);
		ImGui::TreePop();
	}
	for (const auto* head: heads) {
		if (m_pendingLink.has_value() && std::ranges::none_of(types, [&](const vs::NodeType& iType) -> bool {
				return iType.category == head->category && accepts(iType);
			}))
			continue;
		const auto label = categoryLabel(head->category);
		const auto flags = ImGuiTreeNodeFlags_SpanAvailWidth |
						   (m_pendingLink.has_value() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
		if (!ImGui::TreeNodeEx(head->category.c_str(), flags, "%s", label.c_str()))
			continue;
		for (const auto& type: types)
			if (type.category == head->category && accepts(type))
				item(type);
		ImGui::TreePop();
	}
	ImGui::EndChild();
}

void VisualScriptDocument::renderHierarchyPanel() { renderNodeList(false); }

void VisualScriptDocument::renderPropertiesPanel() {
	compileIfStale();
	if (!ImGui::BeginTabBar("##vsTabs"))
		return;
	if (ImGui::BeginTabItem(std::format("{} Variables###variables", gui::icon::variable).c_str())) {
		renderVariables();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Functions###functions", gui::icon::component).c_str())) {
		renderFunctions();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Custom###custom", gui::icon::code).c_str())) {
		renderCustomNodes();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Node###node", gui::icon::squareFunction).c_str())) {
		renderSelectedNode();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void VisualScriptDocument::drawVariableBounds(const size_t iIndex) {
	const auto& variable = m_script.variables[iIndex];
	const std::array<std::pair<const char*, std::optional<float> vs::GraphVariable::*>, 3> bounds{
			{{"Min", &vs::GraphVariable::min}, {"Max", &vs::GraphVariable::max}, {"Step", &vs::GraphVariable::step}}};
	for (const auto& [label, member]: bounds) {
		const auto& bound = variable.*member;
		gui::widgets::propertyLabel(label, "Bound of the inspector field; empty for none.");
		const auto text = bound.has_value() ? std::format("{}", *bound) : std::string{};
		const auto value = valueField(std::format("##{}", label).c_str(), vs::PinType::Number, 1, text, {});
		if (gui::widgets::propertyReset(bound.has_value()) || value.has_value()) {
			auto next = m_script;
			auto& edited = next.variables[iIndex].*member;
			edited.reset();
			if (value.has_value() && !value->empty())
				edited = parseBound(*value);
			commit(std::move(next), "Edit Variable Bounds", true);
		}
	}
}

void VisualScriptDocument::renderVariables() {
	std::optional<size_t> removed;
	for (size_t i = 0; i < m_script.variables.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto& variable = m_script.variables[i];
		const auto pinType = vs::getVariablePinType(variable.type);
		const auto section = gui::widgets::sectionHeader(variable.name.c_str(), getPinGlyph(pinType), true, false,
														 getPinColor(pinType));
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format("var.get|{}", variable.name);
			ImGui::SetDragDropPayload(g_VisualScriptNodePayload, payload.data(), payload.size());
			ImGui::Text("%s Get %s", gui::icon::variable, variable.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##variableMenu");
		if (ImGui::BeginPopup("##variableMenu")) {
			gui::widgets::menuHeader(variable.name.c_str(), "Variable");
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				removed = i;
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##variable")) {
				auto name = variable.name;
				gui::widgets::propertyLabel("Name", "Lua name of the property; renaming updates its nodes.");
				if (ImGui::InputText("##name", &name)) {
					auto next = m_script;
					for (auto& node: next.nodes)
						if (const auto it = node.values.find("variable");
							it != node.values.end() && it->second == variable.name)
							it->second = name;
					next.variables[i].name = name;
					commit(std::move(next), "Rename Variable", true);
				}
				if (!vs::isValidName(name) && ImGui::IsItemHovered())
					ImGui::SetTooltip("Not a usable Lua name.");
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::propertyLabel("Type");
				std::vector<TypedItem> types;
				std::optional<size_t> currentType;
				for (const auto type: g_variableTypes) {
					if (type == variable.type)
						currentType = types.size();
					types.push_back({.label = std::string{script::ScriptEngine::propertyTypeName(type)},
									 .type = vs::getVariablePinType(type)});
				}
				if (const auto picked = typedCombo("##type", types, currentType);
					picked.has_value() && g_variableTypes[*picked] != variable.type) {
					auto next = m_script;
					next.variables[i].type = g_variableTypes[*picked];
					next.variables[i].defaultValue.clear();
					commit(std::move(next), "Change Variable Type");
				}
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::propertyLabel("Default", "Value of the property until the inspector sets another.");
				const auto value = valueField("##default", pinType, vs::getPinComponents(pinType).size(),
											  variable.defaultValue, {});
				if (gui::widgets::propertyReset(!variable.defaultValue.empty()) || value.has_value()) {
					auto next = m_script;
					next.variables[i].defaultValue = value.value_or(std::string{});
					commit(std::move(next), "Edit Variable Default", true);
				}
				if (pinType == vs::PinType::Number || pinType == vs::PinType::Integer)
					drawVariableBounds(i);
				if (auto exposed = variable.exposed; gui::widgets::propertyBool(
							"Exposed", exposed, true,
							"A property the inspector shows; otherwise internal state of the script.")) {
					auto next = m_script;
					next.variables[i].exposed = exposed;
					commit(std::move(next), "Expose Variable");
				}
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (removed.has_value()) {
		auto next = m_script;
		next.variables.erase(next.variables.begin() + static_cast<std::ptrdiff_t>(*removed));
		commit(std::move(next), "Remove Variable");
	}
	if (m_script.variables.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::variable, "No variable",
											   "Variables are script properties, set per entity in the inspector.");
	ImGui::Spacing();
	if (gui::widgets::button("##addVariable",
							 {.glyph = gui::icon::plus,
							  .label = "Add Variable",
							  .tooltip = "Add Variable",
							  .description = "A script property; drag its header onto the graph to read it.",
							  .width = -1.f})) {
		auto next = m_script;
		std::string name = "variable";
		for (uint32_t counter = 2; next.findVariable(name) != nullptr; ++counter)
			name = std::format("variable_{}", counter);
		next.variables.push_back({.name = name,
								  .type = script::ScriptPropertyType::Float,
								  .defaultValue = "0",
								  .min = {},
								  .max = {},
								  .step = {},
								  .exposed = true});
		commit(std::move(next), "Add Variable");
	}
}

void VisualScriptDocument::renderSelectedNode() {
	const auto& selection = m_view.getSelection();
	if (selection.empty()) {
		std::ignore = gui::widgets::emptyState(gui::icon::mousePointer2, "No node selected",
											   "Click a node in the graph to edit its values here.");
		return;
	}
	if (selection.size() > 1) {
		std::ignore =
				gui::widgets::emptyState(gui::icon::boxes, std::format("{} nodes selected", selection.size()).c_str(),
										 "Select one node to edit its values.");
		return;
	}
	const auto* node = m_script.findNode(selection.front());
	const auto* type = node == nullptr ? nullptr : vs::findNodeType(node->type);
	if (type == nullptr) {
		std::ignore = gui::widgets::emptyState(gui::icon::triangleAlert, "Unknown node type");
		return;
	}
	renderNodeHeader(*node, *type);
	renderGroupedPins(*node);
	renderNodeInputs(*node);
}

void VisualScriptDocument::setNodeValue(const core::UUID iNode, const std::string& iKey, const std::string& iValue) {
	auto next = m_script;
	if (auto* edited = next.findNode(iNode); edited != nullptr) {
		if (iValue.empty())
			edited->values.erase(iKey);
		else
			edited->values[iKey] = iValue;
	}
	commit(std::move(next), "Edit Node Value", true);
}

void VisualScriptDocument::setListItemCount(const core::UUID iNode, const size_t iCount) {
	auto next = m_script;
	auto* node = next.findNode(iNode);
	if (node == nullptr)
		return;
	node->values["count"] = std::to_string(std::clamp<size_t>(iCount, 1, vs::g_MaxListItems));
	const auto pins = vs::getNodePins(next, *node);
	const auto kept = [&pins](const std::string& iPin, const bool iOutput) -> bool {
		return std::ranges::any_of(iOutput ? pins.outputs : pins.inputs,
								   [&iPin](const vs::PinSpec& iSpec) -> bool { return iSpec.name == iPin; });
	};
	std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
		return (iLink.toNode == iNode && !kept(iLink.toPin, false)) ||
			   (iLink.fromNode == iNode && !kept(iLink.fromPin, true));
	});
	std::erase_if(node->values, [&kept](const auto& iValue) -> bool {
		return iValue.first.size() == 1 && std::isupper(static_cast<unsigned char>(iValue.first.front())) != 0 &&
			   !kept(iValue.first, false);
	});
	commit(std::move(next), "Change List Items", true);
}

void VisualScriptDocument::renderNodeHeader(const vs::GraphNode& iNode, const vs::NodeType& iType) {
	const auto* node = &iNode;
	const auto* type = &iType;
	const auto id = iNode.id;
	const auto setValue = [this, id](const std::string& iKey, const std::string& iValue) -> void {
		setNodeValue(id, iKey, iValue);
	};
	if (gui::widgets::sectionHeader(getNodeTitle(*node, *type).c_str(), getNodeIcon(*type), false).open) {
		ImGui::PushTextWrapPos();
		ImGui::TextDisabled("%s", type->title.c_str());
		ImGui::TextUnformatted(type->description.c_str());
		ImGui::PopTextWrapPos();
		for (const auto& diagnostic: m_diagnostics)
			if (diagnostic.node == id)
				gui::widgets::notice(diagnostic.message.c_str(), gui::widgets::BadgeKind::Error);
		if (type->kind == vs::NodeKind::FunctionReturn) {
			const auto* function = vs::findNodeFunction(m_script, *node);
			if (function != nullptr && function->exits.size() > 1 && gui::widgets::beginProperties("##exit")) {
				gui::widgets::propertyLabel("Exit", "Execution output of the macro call this return continues by.");
				const auto current = valueOf(*node, "exit");
				if (gui::widgets::beginCombo("##exitCombo", current.c_str())) {
					for (const auto& exit: function->exits)
						if (ImGui::Selectable(exit.c_str(), exit == current))
							setValue("exit", exit);
					ImGui::EndCombo();
				}
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
		}
		if (const auto library = node->values.find("library");
			library != node->values.end() && !library->second.empty())
			ImGui::TextDisabled("%s Library %s", gui::icon::bookOpen, library->second.c_str());
		if ((node->type == "table.make_list" || node->type == "table.break_list") &&
			gui::widgets::beginProperties("##items")) {
			gui::widgets::propertyLabel("Items", "Number of item pins of the node.");
			auto count = static_cast<int>(vs::getListItemCount(*node));
			if (ImGui::SliderInt("##count", &count, 1, static_cast<int>(vs::g_MaxListItems)))
				setListItemCount(id, static_cast<size_t>(count));
			std::ignore = gui::widgets::propertyReset(false);
			gui::widgets::endProperties();
		}
		const bool variableNode = type->kind == vs::NodeKind::GetVariable || type->kind == vs::NodeKind::SetVariable;
		if ((variableNode || type->kind == vs::NodeKind::CustomEvent) && gui::widgets::beginProperties("##settings")) {
			if (variableNode) {
				const auto current = valueOf(*node, "variable");
				gui::widgets::propertyLabel("Variable");
				std::vector<TypedItem> variables;
				std::optional<size_t> currentVariable;
				for (const auto& variable: m_script.variables) {
					if (variable.name == current)
						currentVariable = variables.size();
					variables.push_back({.label = variable.name, .type = vs::getVariablePinType(variable.type)});
				}
				if (const auto picked = typedCombo("##variable", variables, currentVariable); picked.has_value())
					setValue("variable", m_script.variables[*picked].name);
				std::ignore = gui::widgets::propertyReset(false);
			} else {
				auto function = valueOf(*node, "function");
				gui::widgets::propertyLabel("Function", "Lua name the engine calls, e.g. a trigger CallbackName.");
				if (ImGui::InputText("##function", &function))
					setValue("function", function);
				std::ignore = gui::widgets::propertyReset(false);
			}
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
}

void VisualScriptDocument::renderGroupedPins(const vs::GraphNode& iNode) {
	const auto* node = &iNode;
	const auto id = iNode.id;
	const auto pins = vs::getNodePins(m_script, *node);
	std::set<std::string> groups;
	for (const auto& pin: pins.inputs)
		if (!pin.group.empty())
			groups.insert(pin.group);
	for (const auto& pin: pins.outputs)
		if (!pin.group.empty())
			groups.insert(pin.group);
	if (groups.empty() || !gui::widgets::sectionHeader("Grouped Pins", "vs_node_pins", false).open)
		return;
	{
		if (gui::widgets::beginProperties("##groups")) {
			for (const auto& group: groups) {
				const bool split = valueOf(*node, std::string{vs::g_SplitPrefix} + group) == "true";
				gui::widgets::propertyLabel(group.c_str(), "A vector or a colour, or its components as numbers.");
				if (gui::widgets::button("##split", {.glyph = split ? gui::icon::merge : gui::icon::split,
													 .label = split ? "Recombine" : "Split",
													 .width = -1.f}))
					setPinSplit(id, group, !split);
				std::ignore = gui::widgets::propertyReset(false);
			}
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
}

void VisualScriptDocument::renderNodeInputs(const vs::GraphNode& iNode) {
	const auto* node = &iNode;
	const auto id = iNode.id;
	const auto pins = vs::getNodePins(m_script, iNode);
	if (std::ranges::none_of(pins.inputs,
							 [](const vs::PinSpec& iPin) -> bool { return iPin.type != vs::PinType::Exec; }))
		return;
	if (!gui::widgets::sectionHeader("Inputs", "vs_node_inputs", false).open)
		return;
	if (gui::widgets::beginProperties("##inputs")) {
		for (const auto& pin: pins.inputs) {
			if (pin.type == vs::PinType::Exec)
				continue;
			const auto tip = std::format("{}{}", getPinTypeName(pin.type), pin.optional ? ", optional" : "");
			gui::widgets::propertyLabel(pin.name.c_str(), tip.c_str());
			const auto current = valueOf(*node, pin.name);
			if (isLinked(m_script, id, pin.name, false)) {
				ImGui::AlignTextToFramePadding();
				ImGui::TextDisabled("%s linked", gui::icon::link);
				std::ignore = gui::widgets::propertyReset(false);
				continue;
			}
			const auto all = vs::getPinComponents(pin.type).size();
			const auto count =
					pin.group == pin.name && !pin.components.empty() ? std::min(pin.components.size(), all) : all;
			const auto value = valueField("##value", pin.type, count, current, pin.defaultValue);
			if (gui::widgets::propertyReset(!current.empty()))
				setNodeValue(id, pin.name, {});
			else if (value.has_value())
				setNodeValue(id, pin.name, *value);
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

auto VisualScriptDocument::ensureCustomEvent(const std::string& iFunction) -> core::UUID {
	for (const auto& node: m_script.nodes) {
		if (node.type == "event.custom" && valueOf(node, "function") == iFunction) {
			m_view.setSelection({node.id});
			m_view.requestFocus(node.id);
			return node.id;
		}
	}
	const auto center = m_view.getViewCenter();
	const auto id = addNode("event.custom", {center[0] - 80.f, center[1] - 30.f});
	setNodeValue(id, "function", iFunction);
	m_view.requestFocus(id);
	return id;
}

auto VisualScriptDocument::prepareSceneGraph(const std::filesystem::path& iSceneFile)
		-> std::optional<std::pair<std::filesystem::path, std::string>> {
	if (!app::Application::instanced())
		return std::nullopt;
	for (const auto& [title, directory]: app::Application::get().getAssetDirectories()) {
		const auto relative = iSceneFile.lexically_relative(directory);
		if (relative.empty() || *relative.begin() == "..")
			continue;
		const std::string_view suffix = iSceneFile.extension() == ".owlhud" ? "_hud" : "";
		const auto asset =
				std::format("scripts/{}{}{}", iSceneFile.stem().string(), suffix, vs::g_VisualScriptExtension);
		const auto file = directory / asset;
		std::error_code error;
		std::filesystem::create_directories(file.parent_path(), error);
		if (!exists(file) && !platform::writeFileAtomic(file, vs::serialize(starterScript()))) {
			OWL_WARN("VisualScriptDocument: Cannot write '{}'.", file.string())
			return std::nullopt;
		}
		return std::pair{file, asset};
	}
	OWL_WARN("VisualScriptDocument: '{}' is in no asset folder, save it in the project first.", iSceneFile.string())
	return std::nullopt;
}

auto VisualScriptDocument::addFunction(const bool iMacro) -> std::string {
	auto next = m_script;
	std::string name = iMacro ? "macro" : "function";
	const std::string base = name;
	for (uint32_t counter = 2; next.findFunction(name) != nullptr; ++counter)
		name = std::format("{}_{}", base, counter);
	next.functions.push_back(
			{.name = name,
			 .inputs = {},
			 .outputs = {},
			 .macro = iMacro,
			 .exits = iMacro ? std::vector<std::string>{"Then", "Else"} : std::vector<std::string>{"Then"},
			 .exported = false,
			 .category = {},
			 .pure = false});
	const auto center = m_view.getViewCenter();
	const vs::GraphNode entry{.id = core::UUID{},
							  .type = "func.entry",
							  .position = {center[0] - 220.f, center[1]},
							  .values = {{"function", name}}};
	vs::GraphNode ret{.id = core::UUID{},
					  .type = "func.return",
					  .position = {center[0] + 80.f, center[1]},
					  .values = {{"function", name}}};
	if (iMacro)
		ret.values["exit"] = "Then";
	next.links.push_back({.fromNode = entry.id, .fromPin = "Then", .toNode = ret.id, .toPin = "Exec", .waypoints = {}});
	next.nodes.push_back(entry);
	next.nodes.push_back(std::move(ret));
	commit(std::move(next), iMacro ? "Add Macro" : "Add Function");
	m_view.setSelection({entry.id});
	return name;
}

void VisualScriptDocument::removeFunction(const std::string& iName) {
	auto next = m_script;
	std::erase_if(next.functions, [&](const vs::GraphFunction& iFunction) -> bool { return iFunction.name == iName; });
	std::vector<core::UUID> doomed;
	for (const auto& node: next.nodes)
		if (node.type.starts_with("func.") && valueOf(node, "function") == iName && valueOf(node, "library").empty())
			doomed.push_back(node.id);
	for (const auto id: doomed) next.removeNode(id);
	commit(std::move(next), "Remove Function");
}

void VisualScriptDocument::renameFunction(const std::string& iName, const std::string& iNewName) {
	if (iName == iNewName || m_script.findFunction(iNewName) != nullptr)
		return;
	auto next = m_script;
	for (auto& function: next.functions)
		if (function.name == iName)
			function.name = iNewName;
	for (auto& node: next.nodes)
		if (node.type.starts_with("func.") && valueOf(node, "function") == iName && valueOf(node, "library").empty())
			node.values["function"] = iNewName;
	commit(std::move(next), "Rename Function", true);
}

void VisualScriptDocument::setFunction(const std::string& iName, vs::GraphFunction iFunction, const bool iMerge) {
	auto next = m_script;
	const auto found = std::ranges::find(next.functions, iName, &vs::GraphFunction::name);
	if (found == next.functions.end())
		return;
	iFunction.name = iName;
	if (iFunction.exits.empty())
		iFunction.exits = {"Then"};
	// A pin renamed in place keeps its links and the returns their exit.
	std::map<std::string, std::string> inputs;
	std::map<std::string, std::string> outputs;
	const auto renames = [](const auto& iBefore, const auto& iAfter, auto&& iKey,
							std::map<std::string, std::string>& oMap) -> void {
		if (iBefore.size() != iAfter.size())
			return;
		for (size_t i = 0; i < iBefore.size(); ++i)
			if (iKey(iBefore[i]) != iKey(iAfter[i]))
				oMap[iKey(iBefore[i])] = iKey(iAfter[i]);
	};
	const auto parameterName = [](const vs::GraphParameter& iParameter) -> std::string { return iParameter.name; };
	const auto exitName = [](const std::string& iExit) -> std::string { return iExit; };
	renames(found->inputs, iFunction.inputs, parameterName, inputs);
	renames(found->outputs, iFunction.outputs, parameterName, outputs);
	renames(found->exits, iFunction.exits, exitName, outputs);
	for (auto& link: next.links) {
		const auto* from = next.findNode(link.fromNode);
		const auto* to = next.findNode(link.toNode);
		const auto named = [&iName](const vs::GraphNode* iNode, const char* iType) -> bool {
			return iNode != nullptr && iNode->type == iType && valueOf(*iNode, "function") == iName &&
				   valueOf(*iNode, "library").empty();
		};
		if (((named(from, "func.call") || named(from, "func.pure")) && outputs.contains(link.fromPin)))
			link.fromPin = outputs.at(link.fromPin);
		if ((named(from, "func.entry") && inputs.contains(link.fromPin)))
			link.fromPin = inputs.at(link.fromPin);
		if (((named(to, "func.call") || named(to, "func.pure")) && inputs.contains(link.toPin)))
			link.toPin = inputs.at(link.toPin);
		if ((named(to, "func.return") && outputs.contains(link.toPin)))
			link.toPin = outputs.at(link.toPin);
	}
	for (auto& node: next.nodes)
		if (node.type == "func.return" && valueOf(node, "function") == iName && outputs.contains(valueOf(node, "exit")))
			node.values["exit"] = outputs.at(valueOf(node, "exit"));
	*found = std::move(iFunction);
	std::erase_if(next.links, [&next](const vs::GraphLink& iLink) -> bool {
		for (const auto& [nodeId, pinName, output]:
			 {std::tuple{iLink.fromNode, iLink.fromPin, true}, std::tuple{iLink.toNode, iLink.toPin, false}}) {
			const auto* node = next.findNode(nodeId);
			if (node == nullptr || !node->type.starts_with("func."))
				continue;
			const auto pins = vs::getNodePins(next, *node);
			const auto& side = output ? pins.outputs : pins.inputs;
			if (std::ranges::find(side, pinName, &vs::PinSpec::name) == side.end())
				return true;
		}
		return false;
	});
	for (auto& node: next.nodes)
		if (node.type == "func.return" && valueOf(node, "function") == iName && found->exits.size() > 1 &&
			!std::ranges::contains(found->exits, valueOf(node, "exit")))
			node.values["exit"] = found->exits.front();
	commit(std::move(next), "Edit Function", iMerge);
}

auto VisualScriptDocument::addCustomNode() -> std::string {
	auto next = m_script;
	std::string name = "custom";
	for (uint32_t counter = 2; next.findCustomNode(name) != nullptr || next.findFunction(name) != nullptr; ++counter)
		name = std::format("custom_{}", counter);
	next.customNodes.push_back({.name = name,
								.title = "Custom",
								.inputs = {{.name = "value", .type = vs::PinType::Number}},
								.outputs = {{.name = "result", .type = vs::PinType::Number}},
								.code = "return value",
								.pure = true});
	const auto center = m_view.getViewCenter();
	const vs::GraphNode call{.id = core::UUID{},
							 .type = "custom.pure",
							 .position = {center[0] - 60.f, center[1] - 30.f},
							 .values = {{"custom", name}}};
	next.nodes.push_back(call);
	commit(std::move(next), "Add Custom Node");
	m_view.setSelection({call.id});
	return name;
}

void VisualScriptDocument::removeCustomNode(const std::string& iName) {
	auto next = m_script;
	std::erase_if(next.customNodes, [&](const vs::CustomNode& iCustom) -> bool { return iCustom.name == iName; });
	std::vector<core::UUID> doomed;
	for (const auto& node: next.nodes)
		if (node.type.starts_with("custom.") && valueOf(node, "custom") == iName)
			doomed.push_back(node.id);
	for (const auto id: doomed) next.removeNode(id);
	commit(std::move(next), "Remove Custom Node");
}

void VisualScriptDocument::renameCustomNode(const std::string& iName, const std::string& iNewName) {
	if (iName == iNewName || m_script.findCustomNode(iNewName) != nullptr || m_script.findFunction(iNewName) != nullptr)
		return;
	auto next = m_script;
	for (auto& custom: next.customNodes)
		if (custom.name == iName)
			custom.name = iNewName;
	for (auto& node: next.nodes)
		if (node.type.starts_with("custom.") && valueOf(node, "custom") == iName)
			node.values["custom"] = iNewName;
	commit(std::move(next), "Rename Custom Node", true);
}

void VisualScriptDocument::setCustomNode(const std::string& iName, vs::CustomNode iCustom, const bool iMerge) {
	auto next = m_script;
	const auto found = std::ranges::find(next.customNodes, iName, &vs::CustomNode::name);
	if (found == next.customNodes.end())
		return;
	iCustom.name = iName;
	const auto renamed = [](const std::vector<vs::GraphParameter>& iBefore,
							const std::vector<vs::GraphParameter>& iAfter) -> std::map<std::string, std::string> {
		std::map<std::string, std::string> out;
		if (iBefore.size() == iAfter.size())
			for (size_t i = 0; i < iBefore.size(); ++i)
				if (iBefore[i].name != iAfter[i].name)
					out[iBefore[i].name] = iAfter[i].name;
		return out;
	};
	const auto inputs = renamed(found->inputs, iCustom.inputs);
	const auto outputs = renamed(found->outputs, iCustom.outputs);
	const auto isCall = [&iName](const vs::GraphNode* iNode) -> bool {
		return iNode != nullptr && iNode->type.starts_with("custom.") && valueOf(*iNode, "custom") == iName;
	};
	for (auto& link: next.links) {
		if (isCall(next.findNode(link.fromNode)) && outputs.contains(link.fromPin))
			link.fromPin = outputs.at(link.fromPin);
		if (isCall(next.findNode(link.toNode)) && inputs.contains(link.toPin))
			link.toPin = inputs.at(link.toPin);
	}
	for (auto& node: next.nodes)
		if (isCall(&node)) {
			node.type = iCustom.pure ? "custom.pure" : "custom.call";
			for (const auto& [before, after]: inputs)
				if (const auto value = node.values.extract(before); !value.empty())
					node.values[after] = value.mapped();
		}
	*found = std::move(iCustom);
	std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
		for (const auto& [nodeId, pinName, output]:
			 {std::tuple{iLink.fromNode, iLink.fromPin, true}, std::tuple{iLink.toNode, iLink.toPin, false}}) {
			const auto* node = next.findNode(nodeId);
			if (!isCall(node))
				continue;
			const auto pins = vs::getNodePins(next, *node);
			const auto& side = output ? pins.outputs : pins.inputs;
			if (std::ranges::find(side, pinName, &vs::PinSpec::name) == side.end())
				return true;
		}
		return false;
	});
	commit(std::move(next), "Edit Custom Node", iMerge);
}

void VisualScriptDocument::renderCustomNodes() {
	std::optional<std::string> removed;
	for (size_t i = 0; i < m_script.customNodes.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto custom = m_script.customNodes[i];
		const auto section = gui::widgets::sectionHeader(
				custom.title.empty() ? custom.name.c_str() : custom.title.c_str(), "vs_node_call", true, false);
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format("{}|{}", custom.pure ? "custom.pure" : "custom.call", custom.name);
			ImGui::SetDragDropPayload(g_VisualScriptNodePayload, payload.data(), payload.size());
			ImGui::Text("%s %s", gui::icon::code, custom.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##customMenu");
		if (ImGui::BeginPopup("##customMenu")) {
			gui::widgets::menuHeader(custom.name.c_str(), "Custom Node");
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				removed = custom.name;
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##custom")) {
				auto name = custom.name;
				gui::widgets::propertyLabel("Name", "Lua name of the function; renaming updates its nodes.");
				if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && vs::isValidName(name))
					renameCustomNode(custom.name, name);
				std::ignore = gui::widgets::propertyReset(false);
				auto edited = custom;
				gui::widgets::propertyLabel("Title", "Title shown on the node.");
				if (ImGui::InputText("##title", &edited.title))
					setCustomNode(custom.name, edited, true);
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::propertyLabel("Pure", "No execution pins: runs where its outputs are read.");
				if (gui::widgets::toggle("##pure", &edited.pure))
					setCustomNode(custom.name, edited);
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			ImGui::PushID("inputs");
			if (auto inputs = renderParameters("Inputs", custom.inputs, "value", "Add Input"); inputs.has_value()) {
				auto edited = custom;
				edited.inputs = std::move(*inputs);
				setCustomNode(custom.name, edited);
			}
			ImGui::PopID();
			ImGui::PushID("outputs");
			if (auto outputs = renderParameters("Outputs", custom.outputs, "result", "Add Output");
				outputs.has_value()) {
				auto edited = custom;
				edited.outputs = std::move(*outputs);
				setCustomNode(custom.name, edited);
			}
			ImGui::PopID();
			if (gui::widgets::subHeader("Lua", true)) {
				auto edited = custom;
				gui::UiLayer::pushTextStyle(gui::TextStyle::Code);
				if (ImGui::InputTextMultiline("##code", &edited.code,
											  {ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 10.f},
											  ImGuiInputTextFlags_AllowTabInput))
					setCustomNode(custom.name, edited, true);
				gui::UiLayer::popTextStyle();
				ImGui::TextDisabled("The inputs are its parameters; return the outputs in order.");
			}
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (removed.has_value())
		removeCustomNode(*removed);
	if (m_script.customNodes.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::code, "No custom node",
											   "A custom node runs Lua you write, with the pins you give it.");
	ImGui::Spacing();
	if (gui::widgets::button("##addCustom", {.glyph = gui::icon::plus,
											 .label = "Add Custom Node",
											 .tooltip = "Add Custom Node",
											 .description = "Typed inputs and outputs, a Lua body.",
											 .width = ImGui::GetContentRegionAvail().x}))
		std::ignore = addCustomNode();
}

auto VisualScriptDocument::renderParameters(const char* iTitle, const std::vector<vs::GraphParameter>& iParameters,
											const std::string& iAddName, const char* iAddLabel)
		-> std::optional<std::vector<vs::GraphParameter>> {
	static constexpr std::array g_parameterTypes{vs::PinType::Number,    vs::PinType::Integer, vs::PinType::Boolean,
												 vs::PinType::String,    vs::PinType::Entity,  vs::PinType::Vector2,
												 vs::PinType::Vector3,   vs::PinType::Color,   vs::PinType::Table,
												 vs::PinType::Transform, vs::PinType::Matrix,  vs::PinType::Any};
	if (!gui::widgets::subHeader(iTitle, true))
		return std::nullopt;
	std::optional<std::vector<vs::GraphParameter>> result;
	std::optional<size_t> erased;
	for (size_t i = 0; i < iParameters.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		auto edited = iParameters;
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
		if (ImGui::InputText("##name", &edited[i].name, ImGuiInputTextFlags_EnterReturnsTrue) &&
			vs::isValidName(edited[i].name))
			result = edited;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.f);
		std::vector<TypedItem> types;
		std::optional<size_t> current;
		for (const auto type: g_parameterTypes) {
			if (type == iParameters[i].type)
				current = types.size();
			types.push_back({.label = std::string{getPinTypeName(type)}, .type = type});
		}
		if (const auto picked = typedCombo("##type", types, current); picked.has_value()) {
			edited[i].type = g_parameterTypes[*picked];
			result = edited;
		}
		ImGui::SameLine();
		if (gui::widgets::iconButton("##remove", {.glyph = gui::icon::x, .tooltip = "Remove"}))
			erased = i;
		ImGui::PopID();
	}
	if (erased.has_value()) {
		auto edited = iParameters;
		edited.erase(edited.begin() + static_cast<std::ptrdiff_t>(*erased));
		result = std::move(edited);
	}
	if (gui::widgets::button("##add", {.glyph = gui::icon::plus, .label = iAddLabel})) {
		auto edited = iParameters;
		std::string name = iAddName;
		for (uint32_t counter = 2; std::ranges::contains(edited, name, &vs::GraphParameter::name); ++counter)
			name = std::format("{}_{}", iAddName, counter);
		edited.push_back({.name = name, .type = vs::PinType::Number});
		result = std::move(edited);
	}
	return result;
}

void VisualScriptDocument::renderFunctionEntries(
		const std::function<void(const std::string&, const std::string&, const vs::NodeType&)>& iItem) const {
	const auto* call = vs::findNodeType("func.call");
	const auto* pure = vs::findNodeType("func.pure");
	const auto* ret = vs::findNodeType("func.return");
	if (call == nullptr || pure == nullptr || ret == nullptr)
		return;
	const auto folder = [](const vs::GraphFunction& iFunction) -> std::string {
		return iFunction.category.empty() ? std::string{} : iFunction.category + " / ";
	};
	for (const auto& function: m_script.functions) {
		const auto& type = function.pure ? *pure : *call;
		iItem(std::format("{}|{}", type.id, function.name), std::format("{}Call {}", folder(function), function.name),
			  type);
		iItem(std::format("func.return|{}", function.name),
			  std::format("{}Return from {}", folder(function), function.name), *ret);
	}
	for (const auto& custom: m_script.customNodes)
		if (const auto* type = vs::findNodeType(custom.pure ? "custom.pure" : "custom.call"); type != nullptr)
			iItem(std::format("{}|{}", type->id, custom.name), custom.title.empty() ? custom.name : custom.title,
				  *type);
	for (const auto& library: m_libraries) {
		const auto file = std::filesystem::path{library.path}.stem().string();
		for (const auto& function: library.functions) {
			const auto& type = function.pure ? *pure : *call;
			iItem(std::format("{}|{}|{}", type.id, function.name, library.path),
				  std::format("{} / {}{}", file, folder(function), function.name), type);
		}
	}
	for (const auto& module: m_luaModules) {
		const auto file = module.module.substr(module.module.rfind('.') + 1);
		for (const auto& function: module.functions)
			if (const auto* type = vs::findNodeType(function.pure ? "lua.pure" : "lua.call"); type != nullptr)
				iItem(std::format("{}|{}|{}", type->id, module.module, function.name),
					  std::format("{} / {}", file, function.name), *type);
	}
}

void VisualScriptDocument::refreshLibraries() {
	if (!app::Application::instanced() || ImGui::GetFrameCount() - m_libraryScanFrame < 180)
		return;
	m_libraryScanFrame = ImGui::GetFrameCount();
	std::vector<LibraryEntry> libraries;
	std::vector<LuaModuleEntry> modules;
	std::error_code error;
	for (const auto& [title, directory]: app::Application::get().getAssetDirectories()) {
		for (const auto& entry: std::filesystem::recursive_directory_iterator(directory, error)) {
			if (entry.is_regular_file() && entry.path().extension() == ".lua") {
				auto functions = vs::parseLuaFunctions(platform::fileToString(entry.path()));
				if (functions.empty())
					continue;
				auto module = entry.path().lexically_relative(directory).replace_extension().generic_string();
				std::ranges::replace(module, '/', '.');
				modules.push_back({.module = std::move(module), .functions = std::move(functions)});
				continue;
			}
			if (!entry.is_regular_file() || entry.path().extension() != vs::g_VisualScriptExtension ||
				std::filesystem::equivalent(entry.path(), m_path, error))
				continue;
			const auto text = platform::fileToString(entry.path());
			const auto graph = vs::deserialize(text, entry.path().string());
			if (!graph || graph->functions.empty())
				continue;
			libraries.push_back({.path = entry.path().lexically_relative(directory).generic_string(),
								 .functions = graph->functions});
		}
	}
	m_libraries = std::move(libraries);
	m_luaModules = std::move(modules);
}

void VisualScriptDocument::setFunctionPure(const std::string& iName, const bool iPure) {
	auto next = m_script;
	const auto found = std::ranges::find(next.functions, iName, &vs::GraphFunction::name);
	if (found == next.functions.end() || found->macro)
		return;
	found->pure = iPure;
	std::set<uint64_t> calls;
	for (auto& node: next.nodes)
		if ((node.type == "func.call" || node.type == "func.pure") && valueOf(node, "function") == iName &&
			valueOf(node, "library").empty()) {
			node.type = iPure ? "func.pure" : "func.call";
			calls.insert(static_cast<uint64_t>(node.id));
		}
	// A pure call has no execution pins: the links of its old ones go.
	std::erase_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
		const auto touches = [&](const core::UUID iNode, const std::string& iPin) -> bool {
			if (!calls.contains(static_cast<uint64_t>(iNode)))
				return false;
			const auto* node = next.findNode(iNode);
			const auto pins = vs::getNodePins(next, *node);
			const auto has = [&iPin](const std::vector<vs::PinSpec>& iPins) -> bool {
				return std::ranges::any_of(iPins,
										   [&iPin](const vs::PinSpec& iSpec) -> bool { return iSpec.name == iPin; });
			};
			return !has(pins.inputs) && !has(pins.outputs);
		};
		return touches(iLink.fromNode, iLink.fromPin) || touches(iLink.toNode, iLink.toPin);
	});
	commit(std::move(next), iPure ? "Make Function Pure" : "Make Function Not Pure");
}

namespace {

// Asset folder holding a file, the first one of the project when none does.
auto assetDirectoryOf(const std::filesystem::path& iFile) -> std::filesystem::path {
	const auto& directories = app::Application::get().getAssetDirectories();
	for (const auto& directory: directories)
		if (const auto relative = iFile.lexically_relative(directory.assetsPath);
			!relative.empty() && *relative.begin() != "..")
			return directory.assetsPath;
	return directories.front().assetsPath;
}

// Width of a text for the layout of nodes: ImGui's when it runs, a fixed advance otherwise (tests).
auto measureText(const std::string_view iText) -> float {
	if (ImGui::GetCurrentContext() == nullptr)
		return 7.f * static_cast<float>(iText.size());
	return ImGui::CalcTextSize(iText.data(), iText.data() + iText.size()).x;
}

// Canvas box around nodes: top-left, bottom-right.
auto boundsOf(const vs::VisualScript& iScript, std::span<const core::UUID> iNodes)
		-> std::optional<std::array<std::array<float, 2>, 2>> {
	std::optional<std::array<std::array<float, 2>, 2>> box;
	for (const auto id: iNodes) {
		const auto* node = iScript.findNode(id);
		if (node == nullptr)
			continue;
		const auto size = layoutNode(iScript, *node, measureText).size;
		const std::array lo = node->position;
		const std::array hi{node->position[0] + size[0], node->position[1] + size[1]};
		if (!box.has_value()) {
			box = {lo, hi};
			continue;
		}
		(*box)[0] = {std::min((*box)[0][0], lo[0]), std::min((*box)[0][1], lo[1])};
		(*box)[1] = {std::max((*box)[1][0], hi[0]), std::max((*box)[1][1], hi[1])};
	}
	return box;
}

// Name of a parameter or result made unique among those already taken.
auto uniqueName(std::string iBase, std::set<std::string>& ioTaken) -> std::string {
	std::ranges::replace(iBase, ' ', '_');
	if (iBase.empty())
		iBase = "value";
	auto name = iBase;
	for (int i = 2; !ioTaken.insert(name).second; ++i) name = std::format("{}_{}", iBase, i);
	return name;
}


// Height of a pin of a node above its top edge, canvas units.
auto pinHeight(const vs::VisualScript& iScript, const vs::GraphNode& iNode, const std::string& iPin, const bool iOutput)
		-> std::optional<float> {
	const auto pins = vs::getNodePins(iScript, iNode);
	const auto layout = layoutNode(iScript, iNode, measureText);
	const auto& side = iOutput ? pins.outputs : pins.inputs;
	const auto& at = iOutput ? layout.outputs : layout.inputs;
	const auto found = std::ranges::find(side, iPin, &vs::PinSpec::name);
	const auto index = static_cast<size_t>(std::distance(side.begin(), found));
	if (found == side.end() || index >= at.size())
		return std::nullopt;
	return at[index][1];
}

auto isExecLink(const vs::VisualScript& iScript, const vs::GraphLink& iLink) -> bool {
	const auto* node = iScript.findNode(iLink.fromNode);
	if (node == nullptr)
		return false;
	const auto outputs = vs::getNodePins(iScript, *node).outputs;
	const auto found = std::ranges::find(outputs, iLink.fromPin, &vs::PinSpec::name);
	return found != outputs.end() && found->type == vs::PinType::Exec;
}

auto isPureNode(const vs::GraphNode& iNode) -> bool {
	const auto* type = vs::findNodeType(iNode.type);
	return type != nullptr && type->pure;
}

auto overlaps(const vs::GraphGroup& iFirst, const vs::GraphGroup& iSecond) -> bool {
	return iFirst.position[0] < iSecond.position[0] + iSecond.size[0] &&
		   iSecond.position[0] < iFirst.position[0] + iFirst.size[0] &&
		   iFirst.position[1] < iSecond.position[1] + iSecond.size[1] &&
		   iSecond.position[1] < iFirst.position[1] + iFirst.size[1];
}

// The nodes an event runs: its execution chain, then the pure nodes feeding it, minus those already taken.
auto collectChain(const vs::VisualScript& iScript, const core::UUID iStart, std::set<core::UUID>& ioTaken)
		-> std::vector<core::UUID> {
	std::vector<core::UUID> chain{iStart};
	ioTaken.insert(iStart);
	for (size_t i = 0; i < chain.size(); ++i) {
		const auto current = chain[i];
		for (const auto& link: iScript.links) {
			const bool forward = link.fromNode == current && isExecLink(iScript, link);
			const auto* source = link.toNode == current ? iScript.findNode(link.fromNode) : nullptr;
			const bool feeding = source != nullptr && isPureNode(*source);
			auto next = core::UUID{0};
			if (forward)
				next = link.toNode;
			else if (feeding)
				next = link.fromNode;
			if (next != 0 && !ioTaken.contains(next)) {
				ioTaken.insert(next);
				chain.push_back(next);
			}
		}
	}
	return chain;
}

}// namespace

auto VisualScriptDocument::groupNodes(const std::span<const core::UUID> iNodes, const std::string& iTitle)
		-> core::UUID {
	constexpr float margin = 24.f;
	constexpr float header = 28.f;
	const auto box = boundsOf(m_script, iNodes);
	if (!box.has_value())
		return core::UUID{0};
	auto next = m_script;
	const core::UUID id{};
	next.groups.push_back(
			{.id = id,
			 .title = iTitle,
			 .position = {(*box)[0][0] - margin, (*box)[0][1] - margin - header},
			 .size = {(*box)[1][0] - (*box)[0][0] + 2.f * margin, (*box)[1][1] - (*box)[0][1] + 2.f * margin + header},
			 .color = {}});
	commit(std::move(next), "Group Nodes");
	return id;
}

void VisualScriptDocument::setGroup(const vs::GraphGroup& iGroup) {
	auto next = m_script;
	const auto found = std::ranges::find(next.groups, iGroup.id, &vs::GraphGroup::id);
	if (found == next.groups.end() || *found == iGroup)
		return;
	*found = iGroup;
	commit(std::move(next), "Edit Group", true);
}

auto VisualScriptDocument::straightenNodes(const std::span<const core::UUID> iNodes) -> size_t {
	auto next = m_script;
	std::vector<vs::GraphNode*> nodes;
	for (const auto id: iNodes)
		if (auto* node = next.findNode(id); node != nullptr)
			nodes.push_back(node);
	std::ranges::sort(nodes, {}, [](const vs::GraphNode* iNode) -> float { return iNode->position[0]; });
	size_t moved = 0;
	for (auto* node: nodes) {
		const auto incoming = [&](const vs::GraphLink& iLink) -> bool { return iLink.toNode == node->id; };
		const auto outgoing = [&](const vs::GraphLink& iLink) -> bool { return iLink.fromNode == node->id; };
		// The execution wire coming in first, else the first value wire coming in, else the first wire leaving.
		auto link = std::ranges::find_if(next.links, [&](const vs::GraphLink& iLink) -> bool {
			return incoming(iLink) && isExecLink(next, iLink);
		});
		if (link == next.links.end())
			link = std::ranges::find_if(next.links, incoming);
		const bool leaving = link == next.links.end();
		if (leaving)
			link = std::ranges::find_if(next.links, outgoing);
		if (link == next.links.end())
			continue;
		const auto* other = next.findNode(leaving ? link->toNode : link->fromNode);
		if (other == nullptr || other == node)
			continue;
		const auto otherPin = pinHeight(next, *other, leaving ? link->toPin : link->fromPin, !leaving);
		const auto ownPin = pinHeight(next, *node, leaving ? link->fromPin : link->toPin, leaving);
		if (!otherPin.has_value() || !ownPin.has_value())
			continue;
		const float target = other->position[1] + *otherPin - *ownPin;
		if (std::abs(node->position[1] - target) < 0.5f)
			continue;
		node->position[1] = target;
		link->waypoints.clear();
		++moved;
	}
	if (moved > 0)
		commit(std::move(next), "Straighten Links");
	return moved;
}

void VisualScriptDocument::check() {
	compileIfStale();
	m_checkResults = vs::checkTypes(m_script, typeChecker());
	m_checked = true;
}

auto VisualScriptDocument::groupEvents() -> size_t {
	constexpr float margin = 24.f;
	constexpr float header = 28.f;
	std::set<core::UUID> taken;
	for (const auto& node: m_script.nodes)
		if (std::ranges::any_of(m_script.groups,
								[&](const vs::GraphGroup& iGroup) -> bool { return iGroup.holds(node); }))
			taken.insert(node.id);
	std::vector<std::pair<vs::GraphGroup, std::vector<core::UUID>>> made;
	for (const auto& node: m_script.nodes) {
		const auto* type = vs::findNodeType(node.type);
		if (type == nullptr || taken.contains(node.id) ||
			(type->kind != vs::NodeKind::Event && type->kind != vs::NodeKind::CustomEvent &&
			 type->kind != vs::NodeKind::FunctionEntry))
			continue;
		made.push_back({{.id = {}, .title = getNodeTitle(node, *type), .position = {}, .size = {}, .color = {}},
						collectChain(m_script, node.id, taken)});
	}
	const auto fit = [&](std::pair<vs::GraphGroup, std::vector<core::UUID>>& ioGroup) -> void {
		const auto box = boundsOf(m_script, ioGroup.second);
		ioGroup.first.position = {(*box)[0][0] - margin, (*box)[0][1] - margin - header};
		ioGroup.first.size = {(*box)[1][0] - (*box)[0][0] + 2.f * margin,
							  (*box)[1][1] - (*box)[0][1] + 2.f * margin + header};
	};
	std::ranges::for_each(made, fit);
	// Chains whose frames cross share one group: two frames over each other would read as nested.
	for (bool merged = true; merged;) {
		merged = false;
		for (size_t i = 0; i < made.size() && !merged; ++i)
			for (size_t j = i + 1; j < made.size() && !merged; ++j) {
				if (!overlaps(made[i].first, made[j].first))
					continue;
				made[i].first.title += " + " + made[j].first.title;
				made[i].second.insert(made[i].second.end(), made[j].second.begin(), made[j].second.end());
				made.erase(made.begin() + static_cast<std::ptrdiff_t>(j));
				fit(made[i]);
				merged = true;
			}
	}
	if (made.empty())
		return 0;
	auto next = m_script;
	const auto colors = gui::widgets::getLinkColors();
	for (size_t i = 0; i < made.size(); ++i) {
		made[i].first.color = colors[(next.groups.size() + i) % colors.size()].name;
		next.groups.push_back(made[i].first);
	}
	commit(std::move(next), "Group by Events");
	return made.size();
}

void VisualScriptDocument::removeGroup(const core::UUID iGroup, const bool iWithNodes) {
	const auto found = std::ranges::find(m_script.groups, iGroup, &vs::GraphGroup::id);
	if (found == m_script.groups.end())
		return;
	auto next = m_script;
	if (iWithNodes)
		for (const auto& node: m_script.nodes)
			if (found->holds(node))
				next.removeNode(node.id);
	std::erase_if(next.groups, [iGroup](const vs::GraphGroup& iEntry) -> bool { return iEntry.id == iGroup; });
	commit(std::move(next), iWithNodes ? "Delete Group" : "Ungroup");
}

auto VisualScriptDocument::collapseToFunction(const std::span<const core::UUID> iNodes) -> std::string {
	const std::set<uint64_t> inside(iNodes.begin(), iNodes.end());
	const auto isInside = [&inside](const core::UUID iNode) -> bool {
		return inside.contains(static_cast<uint64_t>(iNode));
	};
	bool pure = true;
	for (const auto id: iNodes) {
		const auto* node = m_script.findNode(id);
		const auto* type = node == nullptr ? nullptr : vs::findNodeType(node->type);
		if (type == nullptr || type->kind == vs::NodeKind::Event || type->kind == vs::NodeKind::CustomEvent ||
			type->kind == vs::NodeKind::FunctionEntry || type->kind == vs::NodeKind::FunctionReturn) {
			OWL_WARN("VisualScriptDocument: Events and function entries or returns cannot go into a function.")
			return {};
		}
		pure = pure && type->pure;
	}
	const auto box = boundsOf(m_script, iNodes);
	if (!box.has_value())
		return {};
	const auto pinType = [this](const core::UUID iNode, const std::string& iPin, const bool iOutput) -> vs::PinType {
		const auto* node = m_script.findNode(iNode);
		if (node == nullptr)
			return vs::PinType::Any;
		const auto pins = vs::getNodePins(m_script, *node);
		const auto& side = iOutput ? pins.outputs : pins.inputs;
		const auto found = std::ranges::find(side, iPin, &vs::PinSpec::name);
		return found == side.end() ? vs::PinType::Any : found->type;
	};
	using Pin = std::pair<uint64_t, std::string>;
	std::set<Pin> execTargets;
	std::set<Pin> execSources;
	for (const auto& link: m_script.links) {
		if (pinType(link.fromNode, link.fromPin, true) != vs::PinType::Exec)
			continue;
		if (!isInside(link.fromNode) && isInside(link.toNode))
			execTargets.emplace(static_cast<uint64_t>(link.toNode), link.toPin);
		if (isInside(link.fromNode) && !isInside(link.toNode))
			execSources.emplace(static_cast<uint64_t>(link.fromNode), link.fromPin);
	}
	if ((!pure && execTargets.size() != 1) || execSources.size() > 1) {
		OWL_WARN("VisualScriptDocument: A function needs one execution entry and one exit at most.")
		return {};
	}
	std::string name = "new_function";
	for (int i = 2; m_script.findFunction(name) != nullptr || !vs::isValidName(name); ++i)
		name = std::format("new_function_{}", i);

	auto next = m_script;
	vs::GraphFunction function{.name = name,
							   .inputs = {},
							   .outputs = {},
							   .macro = false,
							   .exits = {"Then"},
							   .exported = false,
							   .category = {},
							   .pure = pure};
	float bottom = (*box)[1][1];
	for (const auto& node: m_script.nodes)
		bottom = std::max(bottom, node.position[1] + layoutNode(m_script, node, measureText).size[1]);
	const std::array shift{0.f, bottom + 300.f - (*box)[0][1]};
	const vs::GraphNode entry{.id = core::UUID{},
							  .type = "func.entry",
							  .position = {(*box)[0][0] - 260.f, (*box)[0][1] + shift[1]},
							  .values = {{"function", name}}};
	const vs::GraphNode ret{.id = core::UUID{},
							.type = "func.return",
							.position = {(*box)[1][0] + 80.f, (*box)[0][1] + shift[1]},
							.values = {{"function", name}}};
	const vs::GraphNode call{.id = core::UUID{},
							 .type = pure ? "func.pure" : "func.call",
							 .position = (*box)[0],
							 .values = {{"function", name}}};
	std::set<std::string> parameterNames;
	std::set<std::string> resultNames;
	std::map<Pin, std::string> parameters;
	std::map<Pin, std::string> results;
	std::vector<vs::GraphLink> links;
	const auto add = [&links](const core::UUID iFrom, const std::string& iFromPin, const core::UUID iTo,
							  const std::string& iToPin) -> void {
		const vs::GraphLink link{.fromNode = iFrom,
								 .fromPin = iFromPin,
								 .toNode = iTo,
								 .toPin = iToPin,
								 .waypoints = {}};
		if (std::ranges::find(links, link) == links.end())
			links.push_back(link);
	};
	for (const auto& link: m_script.links) {
		const bool fromInside = isInside(link.fromNode);
		const bool toInside = isInside(link.toNode);
		const Pin source{static_cast<uint64_t>(link.fromNode), link.fromPin};
		const bool exec = pinType(link.fromNode, link.fromPin, true) == vs::PinType::Exec;
		if (fromInside == toInside) {
			auto kept = link;
			if (fromInside)
				for (auto& [x, y]: kept.waypoints) y += shift[1];
			links.push_back(std::move(kept));
		} else if (exec && toInside) {
			add(link.fromNode, link.fromPin, call.id, "Exec");
			add(entry.id, "Then", link.toNode, link.toPin);
		} else if (exec) {
			add(call.id, "Then", link.toNode, link.toPin);
			add(link.fromNode, link.fromPin, ret.id, "Exec");
		} else if (toInside) {
			auto [it, added] = parameters.try_emplace(source);
			if (added) {
				it->second = uniqueName(link.toPin, parameterNames);
				function.inputs.push_back({.name = it->second, .type = pinType(link.fromNode, link.fromPin, true)});
			}
			add(link.fromNode, link.fromPin, call.id, it->second);
			add(entry.id, it->second, link.toNode, link.toPin);
		} else {
			auto [it, added] = results.try_emplace(source);
			if (added) {
				it->second = uniqueName(link.fromPin, resultNames);
				function.outputs.push_back({.name = it->second, .type = pinType(link.fromNode, link.fromPin, true)});
			}
			add(link.fromNode, link.fromPin, ret.id, it->second);
			add(call.id, it->second, link.toNode, link.toPin);
		}
	}
	if (pure)
		add(entry.id, "Then", ret.id, "Exec");
	next.links = std::move(links);
	for (auto& node: next.nodes)
		if (isInside(node.id))
			node.position[1] += shift[1];
	next.nodes.push_back(entry);
	next.nodes.push_back(ret);
	next.nodes.push_back(call);
	next.functions.push_back(std::move(function));
	next.groups.push_back({.id = core::UUID{},
						   .title = name,
						   .position = {entry.position[0] - 24.f, entry.position[1] - 52.f},
						   .size = {ret.position[0] + 240.f - entry.position[0], (*box)[1][1] - (*box)[0][1] + 100.f},
						   .color = {}});
	commit(std::move(next), "Set as Function");
	m_view.setSelection({call.id});
	return name;
}

auto VisualScriptDocument::moveFunctionToLibrary(const std::string& iName, const std::filesystem::path& iAssetDirectory,
												 const std::string& iLibrary) -> bool {
	const auto* function = m_script.findFunction(iName);
	if (function == nullptr || function->macro || !vs::isVisualScriptPath(iLibrary)) {
		OWL_WARN("VisualScriptDocument: '{}' cannot move to the library '{}'.", iName, iLibrary)
		return false;
	}
	const auto file = iAssetDirectory / iLibrary;
	vs::VisualScript library;
	if (std::filesystem::exists(file)) {
		auto read = vs::deserialize(platform::fileToString(file), file.string());
		if (!read || read->findFunction(iName) != nullptr) {
			OWL_WARN("VisualScriptDocument: The library '{}' cannot take the function '{}'.", iLibrary, iName)
			return false;
		}
		library = std::move(*read);
	}
	// The body: what the entry reaches, and the pure nodes that feed it.
	std::set<uint64_t> body;
	std::vector<core::UUID> pending;
	for (const auto& node: m_script.nodes)
		if (node.type == "func.entry" && valueOf(node, "function") == iName)
			pending.push_back(node.id);
	while (!pending.empty()) {
		const auto id = pending.back();
		pending.pop_back();
		if (!body.insert(static_cast<uint64_t>(id)).second)
			continue;
		for (const auto& link: m_script.links) {
			if (link.fromNode == id)
				pending.push_back(link.toNode);
			if (link.toNode == id)
				if (const auto* from = m_script.findNode(link.fromNode); from != nullptr)
					if (const auto* type = vs::findNodeType(from->type); type != nullptr && type->pure)
						pending.push_back(link.fromNode);
		}
	}
	const auto inBody = [&body](const core::UUID iNode) -> bool { return body.contains(static_cast<uint64_t>(iNode)); };
	auto next = m_script;
	library.functions.push_back(*function);
	for (const auto& node: m_script.nodes)
		if (inBody(node.id))
			library.nodes.push_back(node);
	for (const auto& link: m_script.links)
		if (inBody(link.fromNode) && inBody(link.toNode))
			library.links.push_back(link);
	std::erase_if(next.nodes, [&inBody](const vs::GraphNode& iNode) -> bool { return inBody(iNode.id); });
	std::erase_if(next.links, [&inBody](const vs::GraphLink& iLink) -> bool {
		return inBody(iLink.fromNode) || inBody(iLink.toNode);
	});
	std::erase_if(next.functions, [&iName](const vs::GraphFunction& iEntry) -> bool { return iEntry.name == iName; });
	for (auto& node: next.nodes)
		if ((node.type == "func.call" || node.type == "func.pure") && valueOf(node, "function") == iName &&
			valueOf(node, "library").empty())
			node.values["library"] = iLibrary;
	std::error_code error;
	std::filesystem::create_directories(file.parent_path(), error);
	if (!platform::writeFileAtomic(file, vs::serialize(library))) {
		OWL_ERROR("VisualScriptDocument: Cannot write the library '{}'.", file.string())
		return false;
	}
	vs::clearLibraryCache();
	m_libraryScanFrame = -1000;
	commit(std::move(next), "Move Function to Library");
	return true;
}

void VisualScriptDocument::renderFunctions() {
	std::optional<std::string> removed;
	std::vector<size_t> order(m_script.functions.size());
	std::iota(order.begin(), order.end(), size_t{0});
	std::ranges::stable_sort(order, {}, [this](const size_t iIndex) -> const std::string& {
		return m_script.functions[iIndex].category;
	});
	std::optional<std::string> folder;
	bool folderOpen = true;
	for (const size_t i: order) {
		if (const auto& category = m_script.functions[i].category; folder != category) {
			folder = category;
			folderOpen = category.empty() || gui::widgets::subHeader(category.c_str(), true);
		}
		if (!folderOpen)
			continue;
		ImGui::PushID(static_cast<int>(i));
		const auto function = m_script.functions[i];
		const auto section = gui::widgets::sectionHeader(function.name.c_str(),
														 function.macro ? "vs_node_flow" : "vs_node_call", true, false);
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format("{}|{}", function.pure ? "func.pure" : "func.call", function.name);
			ImGui::SetDragDropPayload(g_VisualScriptNodePayload, payload.data(), payload.size());
			ImGui::Text("%s Call %s", gui::icon::component, function.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##functionMenu");
		if (ImGui::BeginPopup("##functionMenu")) {
			gui::widgets::menuHeader(function.name.c_str(), function.macro ? "Macro" : "Function");
			if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Go to Entry"}))
				for (const auto& node: m_script.nodes)
					if (node.type == "func.entry" && valueOf(node, "function") == function.name) {
						m_view.setSelection({node.id});
						m_view.requestFocus(node.id);
					}
			if (!function.macro && app::Application::instanced() &&
				!app::Application::get().getAssetDirectories().empty() &&
				ImGui::BeginMenu(std::format("{} Move to Library", gui::icon::bookOpen).c_str())) {
				if (m_libraryTarget.empty())
					m_libraryTarget = std::format("scripts/lib/{}.owlvs", m_path.stem().string() + "_lib");
				ImGui::SetNextItemWidth(260.f);
				ImGui::InputText("##libraryPath", &m_libraryTarget);
				ImGui::TextDisabled("Asset path of the library graph; any graph calls its functions.");
				if (gui::widgets::button("##moveLibrary",
										 {.glyph = gui::icon::bookOpen, .label = "Move", .width = -1.f}) &&
					moveFunctionToLibrary(function.name, assetDirectoryOf(m_path), m_libraryTarget))
					ImGui::CloseCurrentPopup();
				ImGui::EndMenu();
			}
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				removed = function.name;
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##function")) {
				auto name = function.name;
				gui::widgets::propertyLabel("Name", "Name of the function; renaming updates its nodes.");
				if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && vs::isValidName(name))
					renameFunction(function.name, name);
				std::ignore = gui::widgets::propertyReset(false);
				if (auto exported = function.exported;
					!function.macro &&
					gui::widgets::propertyBool("Exported", exported, false,
											   "A global function the engine calls by its name and reads the "
											   "results of: a dialogue condition, a HUD callback.")) {
					auto edited = function;
					edited.exported = exported;
					setFunction(function.name, edited);
				}
				if (auto pure = function.pure;
					!function.macro && gui::widgets::propertyBool("Pure", pure, false,
																  "Called without execution pins, where its results "
																  "are read: its body changes nothing."))
					setFunctionPure(function.name, pure);
				auto category = function.category;
				gui::widgets::propertyLabel("Category", "Folder of the function in this panel and in the palette.");
				if (ImGui::InputText("##category", &category, ImGuiInputTextFlags_EnterReturnsTrue)) {
					auto edited = function;
					edited.category = category;
					setFunction(function.name, edited);
				}
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			renderSignature(function);
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (removed.has_value())
		removeFunction(*removed);
	if (m_script.functions.empty())
		std::ignore =
				gui::widgets::emptyState(gui::icon::component, "No function",
										 "A function or a macro groups nodes reused by several events or graphs.");
	ImGui::Spacing();
	const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (gui::widgets::button("##addFunction", {.glyph = gui::icon::plus,
											   .label = "Add Function",
											   .tooltip = "Add Function",
											   .description = "Parameters in, results out, one execution output.",
											   .width = half}))
		std::ignore = addFunction(false);
	ImGui::SameLine();
	if (gui::widgets::button("##addMacro", {.glyph = gui::icon::plus,
											.label = "Add Macro",
											.tooltip = "Add Macro",
											.description = "Like a function, with several execution outputs.",
											.width = half}))
		std::ignore = addFunction(true);
}

void VisualScriptDocument::renderSignature(const vs::GraphFunction& iFunction) {
	static constexpr std::array g_parameterTypes{vs::PinType::Number,    vs::PinType::Integer, vs::PinType::Boolean,
												 vs::PinType::String,    vs::PinType::Entity,  vs::PinType::Vector2,
												 vs::PinType::Vector3,   vs::PinType::Color,   vs::PinType::Table,
												 vs::PinType::Transform, vs::PinType::Matrix,  vs::PinType::Any};
	const auto list = [&](const char* iTitle, const bool iOutputs) -> void {
		if (!gui::widgets::subHeader(iTitle, true))
			return;
		const auto& parameters = iOutputs ? iFunction.outputs : iFunction.inputs;
		std::optional<size_t> erased;
		for (size_t i = 0; i < parameters.size(); ++i) {
			ImGui::PushID(static_cast<int>(i));
			auto edited = iFunction;
			auto& parameter = (iOutputs ? edited.outputs : edited.inputs)[i];
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
			if (ImGui::InputText("##name", &parameter.name, ImGuiInputTextFlags_EnterReturnsTrue) &&
				vs::isValidName(parameter.name))
				setFunction(iFunction.name, edited);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.f);
			std::vector<TypedItem> types;
			std::optional<size_t> current;
			for (const auto type: g_parameterTypes) {
				if (type == parameters[i].type)
					current = types.size();
				types.push_back({.label = std::string{getPinTypeName(type)}, .type = type});
			}
			if (const auto picked = typedCombo("##type", types, current); picked.has_value()) {
				parameter.type = g_parameterTypes[*picked];
				setFunction(iFunction.name, edited);
			}
			ImGui::SameLine();
			if (gui::widgets::iconButton("##remove", {.glyph = gui::icon::x, .tooltip = "Remove"}))
				erased = i;
			ImGui::PopID();
		}
		if (erased.has_value()) {
			auto edited = iFunction;
			auto& side = iOutputs ? edited.outputs : edited.inputs;
			side.erase(side.begin() + static_cast<std::ptrdiff_t>(*erased));
			setFunction(iFunction.name, edited);
		}
		if (gui::widgets::button(iOutputs ? "##addOutput" : "##addInput",
								 {.glyph = gui::icon::plus, .label = iOutputs ? "Add Result" : "Add Parameter"})) {
			auto edited = iFunction;
			auto& side = iOutputs ? edited.outputs : edited.inputs;
			std::string name = iOutputs ? "result" : "value";
			const std::string base = name;
			for (uint32_t counter = 2; std::ranges::contains(side, name, &vs::GraphParameter::name); ++counter)
				name = std::format("{}_{}", base, counter);
			side.push_back({.name = name, .type = vs::PinType::Number});
			setFunction(iFunction.name, edited);
		}
	};
	ImGui::PushID("inputs");
	list("Parameters", false);
	ImGui::PopID();
	ImGui::PushID("outputs");
	list("Results", true);
	ImGui::PopID();
	if (!iFunction.macro || !gui::widgets::subHeader("Exits", true))
		return;
	std::optional<size_t> erased;
	for (size_t i = 0; i < iFunction.exits.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		auto edited = iFunction;
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.f);
		if (ImGui::InputText("##exit", &edited.exits[i], ImGuiInputTextFlags_EnterReturnsTrue) &&
			!edited.exits[i].empty())
			setFunction(iFunction.name, edited);
		ImGui::SameLine();
		if (iFunction.exits.size() > 1 &&
			gui::widgets::iconButton("##remove", {.glyph = gui::icon::x, .tooltip = "Remove"}))
			erased = i;
		ImGui::PopID();
	}
	if (erased.has_value()) {
		auto edited = iFunction;
		edited.exits.erase(edited.exits.begin() + static_cast<std::ptrdiff_t>(*erased));
		setFunction(iFunction.name, edited);
	}
	if (gui::widgets::button("##addExit", {.glyph = gui::icon::plus, .label = "Add Exit"})) {
		auto edited = iFunction;
		edited.exits.push_back(std::format("Exit{}", edited.exits.size() + 1));
		setFunction(iFunction.name, edited);
	}
}

void VisualScriptDocument::renderLua() {
	if (!m_diagnostics.empty())
		ImGui::TextColored(gui::vec(gui::UiLayer::getTheme().error), "%s The graph does not compile: last valid Lua.",
						   gui::icon::triangleAlert);
	if (!mp_luaView) {
		mp_luaView = mkUniq<TextEditor>();
		mp_luaView->SetReadOnlyEnabled(true);
		mp_luaView->SetShowWhitespacesEnabled(false);
		mp_luaView->SetShowLineNumbersEnabled(true);
		codeEditor::applyLanguage(*mp_luaView, codeEditor::Language::Lua);
		m_luaViewText.clear();
	}
	if (m_luaViewText != m_compiledLua) {
		m_luaViewText = m_compiledLua;
		mp_luaView->SetText(m_luaViewText);
	}
	gui::UiLayer::pushTextStyle(gui::TextStyle::Code);
	mp_luaView->Render("##vsLua", ImGui::GetContentRegionAvail());
	gui::UiLayer::popTextStyle();
}

}// namespace owl::nest
