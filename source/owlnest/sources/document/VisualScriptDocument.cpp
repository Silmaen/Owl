/**
 * @file VisualScriptDocument.cpp
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "VisualScriptDocument.h"

#include "EditorLayer.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/Menu.h>
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
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

namespace vs = script::visual;

constexpr float g_sidePanelWidth = 320.f;

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

auto pinId(const core::UUID iNode, const bool iOutput, const std::string& iName) -> core::UUID {
	const auto hash = std::hash<std::string>{}(std::format("{}:{}:{}", static_cast<uint64_t>(iNode), iOutput, iName));
	return core::UUID{hash == 0 ? 1 : static_cast<uint64_t>(hash)};
}

auto pinTypeName(const vs::PinType iType) -> std::string_view {
	switch (iType) {
		case vs::PinType::Exec:
			return "exec";
		case vs::PinType::Boolean:
			return "boolean";
		case vs::PinType::Integer:
			return "integer";
		case vs::PinType::Number:
			return "number";
		case vs::PinType::String:
			return "string";
		case vs::PinType::Table:
			return "table";
		case vs::PinType::Entity:
			return "entity";
		case vs::PinType::Any:
			return "any";
	}
	return "any";
}

auto pinColor(const vs::PinType iType) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	switch (iType) {
		case vs::PinType::Exec:
			return theme.text;
		case vs::PinType::Boolean:
			return theme.error;
		case vs::PinType::Integer:
			return theme.info;
		case vs::PinType::Number:
			return theme.success;
		case vs::PinType::String:
			return theme.accent;
		case vs::PinType::Entity:
			return theme.play;
		case vs::PinType::Table:
		case vs::PinType::Any:
			return gui::Theme::withAlpha(theme.text, 0.6f);
	}
	return theme.text;
}

auto titleColor(const vs::NodeType& iType) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	switch (iType.kind) {
		case vs::NodeKind::Event:
		case vs::NodeKind::CustomEvent:
			return theme.warning;
		case vs::NodeKind::Call:
			return iType.pure ? theme.success : theme.info;
		case vs::NodeKind::GetVariable:
		case vs::NodeKind::SetVariable:
			return theme.accent;
		case vs::NodeKind::Operator:
			return theme.success;
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			return theme.text;
	}
	return theme.text;
}

auto valueOf(const vs::GraphNode& iNode, const std::string& iKey) -> std::string {
	const auto it = iNode.values.find(iKey);
	return it == iNode.values.end() ? std::string{} : it->second;
}

auto nodeTitle(const vs::GraphNode& iNode, const vs::NodeType& iType) -> std::string {
	switch (iType.kind) {
		case vs::NodeKind::GetVariable:
			return std::format("Get {}", valueOf(iNode, "variable"));
		case vs::NodeKind::SetVariable:
			return std::format("Set {}", valueOf(iNode, "variable"));
		case vs::NodeKind::CustomEvent:
			return std::format("Event {}", valueOf(iNode, "function"));
		case vs::NodeKind::Event:
		case vs::NodeKind::Call:
		case vs::NodeKind::Operator:
		case vs::NodeKind::Branch:
		case vs::NodeKind::Sequence:
		case vs::NodeKind::ForLoop:
			break;
	}
	return iType.title;
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

constexpr std::array<script::ScriptPropertyType, 5> g_variableTypes{
		script::ScriptPropertyType::Float, script::ScriptPropertyType::Int, script::ScriptPropertyType::String,
		script::ScriptPropertyType::Bool, script::ScriptPropertyType::Entity};

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
	m_canvasStale = true;
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
	m_canvasStale = true;
	return true;
}

void VisualScriptDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_script);
	m_canvasStale = true;
}

void VisualScriptDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_script);
	m_canvasStale = true;
}

auto VisualScriptDocument::commit(vs::VisualScript iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_script)
		return false;
	auto before = std::move(m_script);
	m_script = iNext;
	m_undo.push(mkUniq<ModifyVisualScriptCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	return true;
}

auto VisualScriptDocument::addNode(const std::string& iType, const std::array<float, 2> iPosition) -> core::UUID {
	if (vs::findNodeType(iType) == nullptr) {
		OWL_WARN("VisualScriptDocument: Unknown node type '{}'.", iType)
		return core::UUID{0};
	}
	auto next = m_script;
	vs::GraphNode node{.id = core::UUID{}, .type = iType, .position = iPosition, .values = {}};
	if (const auto* type = vs::findNodeType(iType);
		type->kind == vs::NodeKind::GetVariable || type->kind == vs::NodeKind::SetVariable) {
		if (!m_script.variables.empty())
			node.values["variable"] = m_script.variables.front().name;
	} else if (type->kind == vs::NodeKind::CustomEvent) {
		node.values["function"] = "my_event";
	}
	const auto id = node.id;
	next.nodes.push_back(std::move(node));
	commit(std::move(next), "Add Node");
	m_canvasStale = true;
	m_pendingSelection = std::vector{id};
	return id;
}

void VisualScriptDocument::deleteNodes(const std::span<const core::UUID> iNodes) {
	auto next = m_script;
	for (const auto id: iNodes) next.removeNode(id);
	if (commit(std::move(next), iNodes.size() > 1 ? "Delete Nodes" : "Delete Node"))
		m_canvasStale = true;
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
	m_canvasStale = true;
	m_pendingSelection = pasted;
	return pasted;
}

void VisualScriptDocument::onCanvasReady() {
	m_canvas.setLinkValidator(
			[this](const core::UUID iFrom, const core::UUID iTo) -> bool { return isLinkAllowed(iFrom, iTo); });
	m_canvas.setOnLinkCreated([this](core::UUID, const core::UUID iFrom, const core::UUID iTo) -> void {
		const auto& from = m_pins.at(iFrom);
		const auto& to = m_pins.at(iTo);
		auto next = m_script;
		next.links.push_back({.fromNode = from.node, .fromPin = from.name, .toNode = to.node, .toPin = to.name});
		commit(std::move(next), "Add Link");
		m_canvasStale = true;
	});
	m_canvas.setOnLinkDeleted([this](const core::UUID iLink) -> void {
		const auto it = m_links.find(iLink);
		if (it == m_links.end())
			return;
		auto next = m_script;
		std::erase(next.links, it->second);
		commit(std::move(next), "Remove Link");
		m_canvasStale = true;
	});
	m_canvas.setOnNodeMoved([this](const core::UUID iNode, const math::vec2f iPosition) -> void {
		auto next = m_script;
		if (auto* node = next.findNode(iNode); node != nullptr)
			node->position = {iPosition.x(), iPosition.y()};
		commit(std::move(next), "Move Nodes", true);
	});
	m_canvas.setOnContextMenu([this](const std::optional<core::UUID> iNode) -> void {
		m_contextNode = iNode;
		const auto mouse = m_canvas.getMouseCanvasPosition();
		m_paletteTarget = {mouse.x(), mouse.y()};
		m_paletteFilter.clear();
		m_openContextMenu = true;
	});
}

auto VisualScriptDocument::isLinkAllowed(const core::UUID iFrom, const core::UUID iTo) const -> bool {
	const auto from = m_pins.find(iFrom);
	const auto to = m_pins.find(iTo);
	if (from == m_pins.end() || to == m_pins.end() || !from->second.output || to->second.output ||
		from->second.node == to->second.node || !vs::canConnect(from->second.type, to->second.type))
		return false;
	const bool exec = from->second.type == vs::PinType::Exec;
	return std::ranges::none_of(m_script.links, [&](const vs::GraphLink& iLink) -> bool {
		if (exec)
			return iLink.fromNode == from->second.node && iLink.fromPin == from->second.name;
		return iLink.toNode == to->second.node && iLink.toPin == to->second.name;
	});
}

void VisualScriptDocument::rebuildCanvas() {
	const auto selection = m_canvas.selection();
	const auto compiled = vs::compile(m_script);
	m_diagnostics = compiled ? std::vector<vs::CompileDiagnostic>{} : compiled.error();
	if (compiled)
		m_compiledLua = *compiled;
	std::set<uint64_t> faulty;
	for (const auto& diagnostic: m_diagnostics) faulty.insert(diagnostic.node);
	m_canvas.clear();
	m_pins.clear();
	m_links.clear();
	const auto& theme = gui::UiLayer::getTheme();
	for (const auto& node: m_script.nodes) {
		const auto* type = vs::findNodeType(node.type);
		const auto pins = vs::getNodePins(m_script, node);
		gui::widgets::Node canvasNode{
				.id = node.id,
				.title = type == nullptr ? std::format("Unknown {}", node.type) : nodeTitle(node, *type),
				.position = {node.position[0], node.position[1]},
				.inputs = {},
				.outputs = {},
				.customData = {},
				.titleColor = type == nullptr || faulty.contains(node.id) ? theme.error : titleColor(*type)};
		const auto addPins = [&](const std::vector<vs::PinSpec>& iSpecs, const bool iOutput) -> void {
			for (const auto& spec: iSpecs) {
				const auto id = pinId(node.id, iOutput, spec.name);
				m_pins[id] = {.node = node.id, .name = spec.name, .output = iOutput, .type = spec.type};
				auto label = spec.name;
				if (const auto value = valueOf(node, spec.name); !iOutput && !value.empty())
					label = std::format("{} = {}", spec.name, value.size() > 12 ? value.substr(0, 12) + "..." : value);
				gui::widgets::NodePin pin{.id = id,
										  .label = label,
										  .typeTag = std::string{pinTypeName(spec.type)},
										  .kind = iOutput ? gui::widgets::PinKind::Output
														  : gui::widgets::PinKind::Input,
										  .labelColor = pinColor(spec.type)};
				(iOutput ? canvasNode.outputs : canvasNode.inputs).push_back(std::move(pin));
			}
		};
		addPins(pins.inputs, false);
		addPins(pins.outputs, true);
		m_canvas.addNode(std::move(canvasNode));
	}
	for (const auto& link: m_script.links) {
		const auto from = pinId(link.fromNode, true, link.fromPin);
		const auto to = pinId(link.toNode, false, link.toPin);
		if (!m_pins.contains(from) || !m_pins.contains(to))
			continue;
		m_links[m_canvas.addLink(from, to)] = link;
	}
	m_canvas.setSelection(m_pendingSelection.value_or(selection));
	m_pendingSelection.reset();
	m_canvasStale = false;
}

void VisualScriptDocument::onImGuiRender() {
	if (m_canvasStale)
		rebuildCanvas();
	const auto winTitle = documentWindowTitle(DocumentType::NodeGraph, title(), static_cast<uint64_t>(id()));
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
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
		const float canvasWidth = std::max(ImGui::GetContentRegionAvail().x - g_sidePanelWidth, 120.f);
		if (ImGui::BeginChild("##vsCanvas", {canvasWidth, 0.f}, ImGuiChildFlags_None,
							  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			m_canvas.onRender();
			if (focused && !ImGui::GetIO().WantTextInput)
				handleShortcuts();
			renderContextMenu();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("##vsSide", {0.f, 0.f}, ImGuiChildFlags_Borders))
			renderSidePanel();
		ImGui::EndChild();
	}
	ImGui::End();
	if (m_canvasStale)
		rebuildCanvas();
}

void VisualScriptDocument::handleShortcuts() {
	const auto selection = m_canvas.selection();
	const bool ctrl = ImGui::GetIO().KeyCtrl;
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !selection.empty()) {
		deleteNodes(selection);
		return;
	}
	if (!ctrl)
		return;
	if (ImGui::IsKeyPressed(ImGuiKey_A, false)) {
		std::vector<core::UUID> all;
		all.reserve(m_script.nodes.size());
		for (const auto& node: m_script.nodes) all.push_back(node.id);
		m_canvas.setSelection(all);
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
	if (!ImGui::BeginPopup("##visualScriptMenu"))
		return;
	const auto* node = m_contextNode.has_value() ? m_script.findNode(*m_contextNode) : nullptr;
	if (node == nullptr) {
		renderPalette();
		ImGui::EndPopup();
		return;
	}
	auto selection = m_canvas.selection();
	if (!isSelected(selection, node->id))
		selection = {node->id};
	const auto* type = vs::findNodeType(node->type);
	gui::widgets::menuHeader(type == nullptr ? node->type.c_str() : type->title.c_str(), "Node");
	if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
		std::ignore = pasteNodes(copyNodes(selection), {30.f, 30.f});
	if (gui::widgets::menuItem({.glyph = gui::icon::clipboardCopy, .label = "Copy", .shortcut = "Ctrl+C"}))
		ImGui::SetClipboardText(copyNodes(selection).c_str());
	if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Delete", .danger = true}))
		deleteNodes(selection);
	ImGui::EndPopup();
}

void VisualScriptDocument::renderPalette() {
	gui::widgets::menuHeader("Add Node", "Palette");
	if (ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(300.f);
	ImGui::InputTextWithHint("##paletteFilter", "Search nodes", &m_paletteFilter);
	if (const char* text = ImGui::GetClipboardText();
		text != nullptr && std::string_view{text}.contains("VisualScript:") &&
		gui::widgets::menuItem({.glyph = gui::icon::clipboardPaste, .label = "Paste Here", .shortcut = "Ctrl+V"})) {
		if (const auto part = vs::deserialize(text, "clipboard"); part && !part->nodes.empty()) {
			const auto& first = part->nodes.front().position;
			std::ignore = pasteNodes(text, {m_paletteTarget[0] - first[0], m_paletteTarget[1] - first[1]});
		}
		ImGui::CloseCurrentPopup();
		return;
	}
	if (!ImGui::BeginChild("##paletteList", {300.f, 360.f})) {
		ImGui::EndChild();
		return;
	}
	const auto pick = [this](const vs::NodeType& iType) -> void {
		if (ImGui::Selectable(iType.title.c_str())) {
			std::ignore = addNode(iType.id, m_paletteTarget);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::IsItemHovered() && !iType.description.empty())
			ImGui::SetTooltip("%s", iType.description.c_str());
	};
	const auto& types = vs::getNodeTypes();
	if (!m_paletteFilter.empty()) {
		for (const auto& type: types)
			if (containsNoCase(type.title, m_paletteFilter) || containsNoCase(type.description, m_paletteFilter)) {
				ImGui::PushID(type.id.c_str());
				pick(type);
				ImGui::PopID();
			}
		ImGui::EndChild();
		return;
	}
	std::vector<std::string> categories;
	for (const auto& type: types)
		if (std::ranges::find(categories, type.category) == categories.end())
			categories.push_back(type.category);
	for (const auto& category: categories) {
		if (!ImGui::TreeNodeEx(category.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
			continue;
		for (const auto& type: types)
			if (type.category == category) {
				ImGui::PushID(type.id.c_str());
				pick(type);
				ImGui::PopID();
			}
		ImGui::TreePop();
	}
	ImGui::EndChild();
}

void VisualScriptDocument::renderSidePanel() {
	renderVariables();
	ImGui::Spacing();
	renderSelectedNode();
	ImGui::Spacing();
	renderCompilation();
}

void VisualScriptDocument::renderVariables() {
	ImGui::SeparatorText("Variables");
	ImGui::TextDisabled("Script properties, shown in the inspector.");
	std::optional<size_t> removed;
	for (size_t i = 0; i < m_script.variables.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		const auto& variable = m_script.variables[i];
		auto name = variable.name;
		ImGui::SetNextItemWidth(110.f);
		if (ImGui::InputText("##name", &name)) {
			auto next = m_script;
			for (auto& node: next.nodes)
				if (const auto it = node.values.find("variable");
					it != node.values.end() && it->second == variable.name)
					it->second = name;
			next.variables[i].name = name;
			commit(std::move(next), "Rename Variable", true);
			m_canvasStale = true;
		}
		if (!vs::isValidName(name) && ImGui::IsItemHovered())
			ImGui::SetTooltip("Not a usable Lua name.");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70.f);
		if (ImGui::BeginCombo("##type", std::string{script::ScriptEngine::propertyTypeName(variable.type)}.c_str())) {
			for (const auto type: g_variableTypes)
				if (ImGui::Selectable(std::string{script::ScriptEngine::propertyTypeName(type)}.c_str(),
									  type == variable.type)) {
					auto next = m_script;
					next.variables[i].type = type;
					commit(std::move(next), "Change Variable Type");
					m_canvasStale = true;
				}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		auto value = variable.defaultValue;
		ImGui::SetNextItemWidth(70.f);
		if (ImGui::InputTextWithHint("##default", "default", &value)) {
			auto next = m_script;
			next.variables[i].defaultValue = value;
			commit(std::move(next), "Edit Variable Default", true);
			m_canvasStale = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton(gui::icon::trash2))
			removed = i;
		ImGui::PopID();
	}
	if (removed.has_value()) {
		auto next = m_script;
		next.variables.erase(next.variables.begin() + static_cast<std::ptrdiff_t>(*removed));
		commit(std::move(next), "Remove Variable");
		m_canvasStale = true;
	}
	if (ImGui::Button(std::format("{} Add Variable", gui::icon::plus).c_str())) {
		auto next = m_script;
		std::string name = "variable";
		for (uint32_t counter = 2; next.findVariable(name) != nullptr; ++counter)
			name = std::format("variable_{}", counter);
		next.variables.push_back({.name = name, .type = script::ScriptPropertyType::Float, .defaultValue = "0"});
		commit(std::move(next), "Add Variable");
		m_canvasStale = true;
	}
}

void VisualScriptDocument::renderSelectedNode() {
	ImGui::SeparatorText("Node");
	const auto selection = m_canvas.selection();
	if (selection.empty()) {
		ImGui::TextDisabled("Select a node to edit its values.");
		return;
	}
	if (selection.size() > 1) {
		ImGui::TextDisabled("%zu nodes selected.", selection.size());
		return;
	}
	const auto* node = m_script.findNode(selection.front());
	const auto* type = node == nullptr ? nullptr : vs::findNodeType(node->type);
	if (type == nullptr) {
		ImGui::TextDisabled("Unknown node type.");
		return;
	}
	ImGui::TextUnformatted(nodeTitle(*node, *type).c_str());
	ImGui::PushTextWrapPos();
	ImGui::TextDisabled("%s", type->description.c_str());
	ImGui::PopTextWrapPos();
	const auto setValue = [this, id = node->id](const std::string& iKey, const std::string& iValue) -> void {
		auto next = m_script;
		if (auto* edited = next.findNode(id); edited != nullptr) {
			if (iValue.empty())
				edited->values.erase(iKey);
			else
				edited->values[iKey] = iValue;
		}
		commit(std::move(next), "Edit Node Value", true);
		m_canvasStale = true;
	};
	if (type->kind == vs::NodeKind::GetVariable || type->kind == vs::NodeKind::SetVariable) {
		const auto current = valueOf(*node, "variable");
		if (ImGui::BeginCombo("Variable", current.c_str())) {
			for (const auto& variable: m_script.variables)
				if (ImGui::Selectable(variable.name.c_str(), variable.name == current))
					setValue("variable", variable.name);
			ImGui::EndCombo();
		}
	}
	if (type->kind == vs::NodeKind::CustomEvent) {
		auto function = valueOf(*node, "function");
		if (ImGui::InputText("Function", &function))
			setValue("function", function);
	}
	const auto pins = vs::getNodePins(m_script, *node);
	for (const auto& pin: pins.inputs) {
		if (pin.type == vs::PinType::Exec)
			continue;
		ImGui::PushID(pin.name.c_str());
		const bool linked = std::ranges::any_of(m_script.links, [&](const vs::GraphLink& iLink) -> bool {
			return iLink.toNode == node->id && iLink.toPin == pin.name;
		});
		auto value = valueOf(*node, pin.name);
		if (linked) {
			ImGui::TextDisabled("%s: linked", pin.name.c_str());
		} else if (pin.type == vs::PinType::Boolean) {
			bool checked = (value.empty() ? pin.defaultValue : value) == "true";
			if (ImGui::Checkbox(pin.name.c_str(), &checked))
				setValue(pin.name, checked ? "true" : "false");
		} else {
			const auto hint = pin.type == vs::PinType::Entity ? std::string{"self"} : pin.defaultValue;
			if (ImGui::InputTextWithHint(pin.name.c_str(), hint.c_str(), &value))
				setValue(pin.name, value);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s%s", std::string{pinTypeName(pin.type)}.c_str(), pin.optional ? ", optional" : "");
		ImGui::PopID();
	}
}

void VisualScriptDocument::renderCompilation() {
	ImGui::SeparatorText("Lua");
	const auto& theme = gui::UiLayer::getTheme();
	if (m_diagnostics.empty()) {
		ImGui::TextColored(gui::vec(theme.success), "%s Compiles to Lua", gui::icon::circleCheck);
	} else {
		for (size_t i = 0; i < m_diagnostics.size(); ++i) {
			const auto& diagnostic = m_diagnostics[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(theme.error));
			ImGui::PushTextWrapPos();
			const auto label = std::format("{} {}", gui::icon::triangleAlert, diagnostic.message);
			if (ImGui::Selectable(label.c_str()) && diagnostic.node != 0) {
				const std::array selection{diagnostic.node};
				m_canvas.setSelection(selection);
			}
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
			ImGui::PopID();
		}
	}
	ImGui::Checkbox("Show generated Lua", &m_showLua);
	if (m_showLua)
		ImGui::InputTextMultiline("##lua", &m_compiledLua, {-1.f, -1.f}, ImGuiInputTextFlags_ReadOnly);
}

}// namespace owl::nest
