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

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <platform/AtomicFile.h>
#include <platform/FileUtils.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

namespace vs = script::visual;

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

constexpr std::array<script::ScriptPropertyType, 8> g_variableTypes{
		script::ScriptPropertyType::Float, script::ScriptPropertyType::Int,    script::ScriptPropertyType::String,
		script::ScriptPropertyType::Bool,  script::ScriptPropertyType::Entity, script::ScriptPropertyType::Vec2,
		script::ScriptPropertyType::Vec3,  script::ScriptPropertyType::Color};

// Palette groups in reading order; the binding tables follow.
constexpr std::array<std::string_view, 10> g_categoryOrder{"Events", "Flow",  "Variables", "Constant", "Math",
														   "Vector", "Color", "Compare",   "Logic",    "Text"};

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
		case vs::PinType::Exec:
		case vs::PinType::Table:
			return std::nullopt;
		case vs::PinType::Integer:
		case vs::PinType::Number:
		case vs::PinType::String:
		case vs::PinType::Entity:
		case vs::PinType::Any:
			break;
	}
	auto text = iValue;
	const auto hint = iType == vs::PinType::Entity ? std::string{"self"} : iDefault;
	if (ImGui::InputTextWithHint(iId, hint.c_str(), &text))
		return text;
	return std::nullopt;
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
	return true;
}

auto VisualScriptDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto script = vs::deserialize(platform::fileToString(iPath), iPath.string());
	if (!script) {
		OWL_WARN("VisualScriptDocument: {} is not a visual script.", iPath.string())
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
	auto compiled = vs::compile(m_script);
	m_diagnostics = compiled ? std::vector<vs::CompileDiagnostic>{} : compiled.error();
	if (compiled)
		m_compiledLua = std::move(*compiled);
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
			next.links.push_back({.fromNode = from.node, .fromPin = from.pin, .toNode = to.node, .toPin = to.pin});
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
		const auto result = m_view.render(m_script, m_diagnostics);
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
	if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Delete", .danger = true}))
		deleteNodes(selection);
	ImGui::EndPopup();
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
	const auto item = [&](const vs::NodeType& iType) -> void {
		ImGui::PushID(iType.id.c_str());
		const vs::GraphNode probe{.id = core::UUID{0}, .type = iType.id, .position = {0.f, 0.f}, .values = {}};
		const auto color = getNodeColor(m_script, probe, iType);
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(gui::Theme::mix(color, {1.f, 1.f, 1.f, 1.f}, 0.45f)));
		ImGui::TextUnformatted(getNodeGlyph(iType));
		ImGui::PopStyleColor();
		ImGui::SameLine();
		const auto label = iType.kind == vs::NodeKind::Call ? getNodeTitle(probe, iType) : iType.title;
		if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
			if (iPopup) {
				std::ignore = addNode(iType.id, m_paletteTarget, m_pendingLink);
				m_pendingLink.reset();
				ImGui::CloseCurrentPopup();
			} else {
				const auto center = m_view.getViewCenter();
				std::ignore = addNode(iType.id, {center[0] - 60.f, center[1] - 30.f});
			}
		}
		if (!iPopup && ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(g_VisualScriptNodePayload, iType.id.data(), iType.id.size());
			ImGui::Text("%s %s", getNodeGlyph(iType), label.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered() && !iType.description.empty())
			ImGui::SetTooltip("%s\n%s", iType.title.c_str(), iType.description.c_str());
		ImGui::PopID();
	};
	const auto& types = vs::getNodeTypes();
	if (!m_paletteFilter.empty()) {
		for (const auto& type: types)
			if (accepts(type) &&
				(containsNoCase(type.title, m_paletteFilter) || containsNoCase(type.description, m_paletteFilter)))
				item(type);
		ImGui::EndChild();
		return;
	}
	std::vector<const vs::NodeType*> heads;
	for (const auto& type: types)
		if (std::ranges::none_of(heads,
								 [&](const vs::NodeType* iHead) -> bool { return iHead->category == type.category; }))
			heads.push_back(&type);
	std::ranges::stable_sort(heads, {},
							 [](const vs::NodeType* iHead) -> size_t { return categoryRank(iHead->category); });
	for (const auto* head: heads) {
		if (m_pendingLink.has_value() && std::ranges::none_of(types, [&](const vs::NodeType& iType) -> bool {
				return iType.category == head->category && accepts(iType);
			}))
			continue;
		const auto label = std::format(
				"{}  {}", head->kind == vs::NodeKind::Call ? gui::icon::squareFunction : getNodeGlyph(*head),
				head->category);
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
	if (ImGui::BeginTabItem(std::format("{} Variables", gui::icon::variable).c_str())) {
		renderVariables();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Node", gui::icon::squareFunction).c_str())) {
		renderSelectedNode();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
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
		next.variables.push_back({.name = name, .type = script::ScriptPropertyType::Float, .defaultValue = "0"});
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
