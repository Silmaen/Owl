/**
 * @file MaterialDocumentGraph.cpp
 * @author Silmaen
 * @date 09/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MaterialDocument.h"

#include "EditorLayer.h"
#include "NodeGraphDocument.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/utils.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Toggle.h>
#include <gui/widgets/TreeRow.h>
#include <renderer/MaterialLibrary.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest {

namespace {

using renderer::MaterialGraph;
using renderer::MaterialGraphNode;
using renderer::MaterialParameterType;
using renderer::MaterialPinSpec;
using renderer::MaterialPinType;
using renderer::MaterialValueType;

// Drag-and-drop payload of a palette entry dragged onto the canvas: `type` or `type|item`.
constexpr auto g_MaterialNodePayload = "OWL_MAT_NODE";
constexpr float g_thumbnailFooter = 84.f;
constexpr std::array g_pinTypes{MaterialPinType::Float, MaterialPinType::Float2, MaterialPinType::Float3,
								MaterialPinType::Float4};
constexpr std::array g_parameterTypes{MaterialParameterType::Float, MaterialParameterType::Vector,
									  MaterialParameterType::Color, MaterialParameterType::Texture};

auto pinColor(const MaterialPinType iType) -> math::vec4 {
	switch (iType) {
		case MaterialPinType::Float:
			return {0.6f, 0.85f, 0.6f, 1.f};
		case MaterialPinType::Float2:
			return {0.95f, 0.85f, 0.4f, 1.f};
		case MaterialPinType::Float3:
			return {0.7f, 0.6f, 1.f, 1.f};
		case MaterialPinType::Float4:
			return {1.f, 0.55f, 0.75f, 1.f};
		case MaterialPinType::Any:
			return {0.85f, 0.85f, 0.85f, 1.f};
	}
	return {1.f, 1.f, 1.f, 1.f};
}

auto categoryColor(const std::string& iCategory) -> math::vec4 {
	if (iCategory == "Inputs" || iCategory == "Parameters")
		return {0.6f, 0.9f, 0.6f, 1.f};
	if (iCategory == "Textures")
		return {1.f, 0.8f, 0.45f, 1.f};
	if (iCategory == "Effects")
		return {0.85f, 0.65f, 1.f, 1.f};
	if (iCategory == "Surface")
		return {1.f, 0.55f, 0.55f, 1.f};
	if (iCategory == "Graph" || iCategory == "Custom Nodes" || iCategory == "Functions")
		return {0.5f, 0.85f, 0.95f, 1.f};
	return {0.65f, 0.8f, 1.f, 1.f};
}

auto categoryGlyph(const std::string& iCategory) -> const char* {
	if (iCategory == "Inputs")
		return gui::icon::variable;
	if (iCategory == "Parameters")
		return gui::icon::slidersHorizontal;
	if (iCategory == "Maths")
		return gui::icon::sigma;
	if (iCategory == "Vectors")
		return gui::icon::hash;
	if (iCategory == "Textures")
		return gui::icon::image;
	if (iCategory == "Effects")
		return gui::icon::sparkles;
	if (iCategory == "Surface")
		return gui::icon::layers;
	if (iCategory == "Custom Nodes")
		return gui::icon::code;
	if (iCategory == "Functions")
		return gui::icon::squareFunction;
	return gui::icon::workflow;
}

auto parseNumbers(const std::string& iText, const size_t iCount) -> std::array<float, 4> {
	std::array<float, 4> values{0.f, 0.f, 0.f, 0.f};
	if (iText.starts_with('@'))
		return values;
	std::istringstream stream{iText};
	for (size_t index = 0; index < iCount && stream >> values[index]; ++index) {}
	return values;
}

auto formatNumbers(const std::span<const float> iValues) -> std::string {
	std::string text;
	for (const float value: iValues) {
		if (!text.empty())
			text += ' ';
		text += std::format("{}", value);
	}
	return text;
}

auto widthOf(const MaterialPinType iType) -> size_t {
	switch (iType) {
		case MaterialPinType::Float:
			return 1;
		case MaterialPinType::Float2:
			return 2;
		case MaterialPinType::Float3:
			return 3;
		case MaterialPinType::Float4:
			return 4;
		case MaterialPinType::Any:
			return 0;
	}
	return 0;
}

// The text of a node title: the graph item it names, or the title of its type.
auto nodeTitle(const MaterialGraph& iGraph, const MaterialGraphNode& iNode) -> std::string {
	const auto* type = renderer::findMaterialNodeType(iNode.type);
	if (type == nullptr)
		return iNode.type;
	if (iNode.type == "param.get")
		return renderer::getMaterialNodeValue(iNode, "parameter");
	if (iNode.type == "custom") {
		const auto name = renderer::getMaterialNodeValue(iNode, "custom");
		const auto* custom = iGraph.findCustomNode(name);
		return custom != nullptr && !custom->title.empty() ? custom->title : name;
	}
	if (iNode.type == "function.call")
		return renderer::getMaterialNodeValue(iNode, "function");
	if (iNode.type.starts_with("texture.")) {
		if (const auto texture = renderer::getMaterialNodeValue(iNode, "texture"); !texture.empty())
			return std::format("{} ({})", type->title, texture);
	}
	return type->title;
}

auto scoped(commands::CommandArgs iArgs, const std::string& iScope) -> commands::CommandArgs {
	if (!iScope.empty())
		iArgs.set("function", iScope);
	return iArgs;
}

auto idText(const core::UUID iId) -> std::string { return std::to_string(static_cast<uint64_t>(iId)); }

// One entry of the palette.
struct PaletteEntry {
	std::string entry;
	std::string label;
	std::string category;
	std::string description;
};

auto paletteEntries(const MaterialGraph& iGraph, const std::string& iScope) -> std::vector<PaletteEntry> {
	std::vector<PaletteEntry> entries;
	for (const auto& type: renderer::getMaterialNodeTypes()) {
		if (type.id == "param.get" || type.id == "custom" || type.id == "function.call")
			continue;
		const bool functionNode = type.id == "function.input" || type.id == "function.output";
		if ((functionNode && iScope.empty()) || (type.id == "output.surface" && !iScope.empty()))
			continue;
		entries.push_back(
				{.entry = type.id, .label = type.title, .category = type.category, .description = type.description});
	}
	for (const auto& parameter: iGraph.parameters) {
		if (parameter.type == MaterialParameterType::Texture)
			entries.push_back({.entry = std::format("texture.sample|{}", parameter.name),
							   .label = std::format("Sample {}", parameter.name),
							   .category = "Parameters",
							   .description = "The texture parameter read at a uv."});
		else
			entries.push_back({.entry = std::format("param.get|{}", parameter.name),
							   .label = parameter.name,
							   .category = "Parameters",
							   .description = "A value the graph exposes; a child material overrides it."});
	}
	for (const auto& custom: iGraph.customNodes)
		entries.push_back({.entry = std::format("custom|{}", custom.name),
						   .label = custom.title.empty() ? custom.name : custom.title,
						   .category = "Custom Nodes",
						   .description = "A node written in Slang in the Custom tab."});
	for (const auto& function: iGraph.functions) {
		if (function.name == iScope)
			continue;
		entries.push_back({.entry = std::format("function.call|{}", function.name),
						   .label = function.name,
						   .category = "Functions",
						   .description = "A material function of the graph."});
	}
	return entries;
}

auto enumCombo(const char* iId, const std::span<const char* const> iNames, const size_t iCurrent)
		-> std::optional<size_t> {
	std::optional<size_t> picked;
	if (gui::widgets::beginCombo(iId, iNames[iCurrent])) {
		for (size_t index = 0; index < iNames.size(); ++index)
			if (ImGui::Selectable(iNames[index], index == iCurrent))
				picked = index;
		ImGui::EndCombo();
	}
	return picked;
}

// A texture asset path: read-only field, a drop from the Content Browser and a clear button.
auto texturePathField(const char* iId, std::string& ioPath) -> bool {
	bool changed = false;
	const float side = ImGui::GetFrameHeight();
	ImGui::SetNextItemWidth(std::max(side, ImGui::GetContentRegionAvail().x - side - ImGui::GetStyle().ItemSpacing.x));
	std::string shown = ioPath;
	ImGui::InputTextWithHint(iId, "drop an image", &shown, ImGuiInputTextFlags_ReadOnly);
	if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::Texture, dropped)) {
		ioPath = dropped.generic_string();
		changed = true;
	}
	ImGui::SameLine();
	ImGui::PushID(iId);
	if (gui::widgets::iconButton("##clear", {.glyph = gui::icon::x, .tooltip = "Clear", .disabled = ioPath.empty()})) {
		ioPath.clear();
		changed = true;
	}
	ImGui::PopID();
	return changed;
}

// The editor of a value of a parameter; true when it changed.
auto parameterValueField(const char* iId, const MaterialParameterType iType, std::string& ioValue) -> bool {
	switch (iType) {
		case MaterialParameterType::Float:
			{
				auto values = parseNumbers(ioValue, 1);
				if (!ImGui::DragFloat(iId, values.data(), 0.01f))
					return false;
				ioValue = formatNumbers(std::span{values}.first(1));
				return true;
			}
		case MaterialParameterType::Vector:
			{
				auto values = parseNumbers(ioValue, 4);
				if (!gui::widgets::vectorField(iId, values, 0.01f))
					return false;
				ioValue = formatNumbers(values);
				return true;
			}
		case MaterialParameterType::Color:
			{
				auto values = parseNumbers(ioValue, 4);
				if (!ImGui::ColorEdit4(iId, values.data()))
					return false;
				ioValue = formatNumbers(values);
				return true;
			}
		case MaterialParameterType::Texture:
			return texturePathField(iId, ioValue);
	}
	return false;
}

}// namespace

auto MaterialDocument::setScope(const std::string& iFunction) -> bool {
	if (!iFunction.empty() && (!isGraph() || m_asset.graph->findFunction(iFunction) == nullptr)) {
		OWL_WARN("MaterialDocument: No function '{}' in the graph.", iFunction)
		return false;
	}
	if (iFunction == m_scope)
		return true;
	m_scope = iFunction;
	m_selectedNode.reset();
	m_canvasStale = true;
	m_fitFrames = 3;
	return true;
}

void MaterialDocument::syncCanvas() {
	if (!m_canvasStale)
		return;
	rebuildCanvas();
	m_canvasStale = false;
}

void MaterialDocument::selectNode(const std::optional<core::UUID> iNode) {
	const auto* nodes = isGraph() ? commands::getMaterialScopeNodes(*m_asset.graph, m_scope) : nullptr;
	m_selectedNode = iNode && nodes != nullptr && std::ranges::contains(*nodes, *iNode, &MaterialGraphNode::id)
							 ? iNode
							 : std::nullopt;
	m_canvas.setSelection(m_selectedNode ? std::vector<core::UUID>{*m_selectedNode} : std::vector<core::UUID>{});
}

void MaterialDocument::bindCanvas() {
	m_canvas.setLinkColorsEnabled(false);
	m_canvas.setLinkValidator([this](const core::UUID iFrom, const core::UUID iTo) -> bool {
		const auto from = m_pins.find(iFrom);
		const auto to = m_pins.find(iTo);
		return from != m_pins.end() && to != m_pins.end() && from->second.output && !to->second.output &&
			   from->second.node != to->second.node;
	});
	m_canvas.setOnLinkCreated([this](core::UUID, const core::UUID iFrom, const core::UUID iTo) -> void {
		const auto& from = m_pins.at(iFrom);
		const auto& to = m_pins.at(iTo);
		std::ignore = runCommand("material.link", scoped({{"from_node", idText(from.node)},
														  {"from_pin", from.pin},
														  {"to_node", idText(to.node)},
														  {"to_pin", to.pin}},
														 m_scope));
		m_canvasStale = true;
	});
	m_canvas.setOnLinkDeleted([this](core::UUID) -> void {
		m_linkDeleted = true;
		const auto* links = isGraph() ? commands::getMaterialScopeLinks(*m_asset.graph, m_scope) : nullptr;
		if (links == nullptr)
			return;
		std::vector<std::pair<core::UUID, std::string>> removed;
		for (const auto& link: *links) {
			const auto from = m_pinIds.find({link.fromNode, true, link.fromPin});
			const auto to = m_pinIds.find({link.toNode, false, link.toPin});
			if (from == m_pinIds.end() || to == m_pinIds.end() ||
				std::ranges::none_of(m_canvas.links(), [&](const gui::widgets::Link& iLink) -> bool {
					return iLink.fromPin == from->second && iLink.toPin == to->second;
				}))
				removed.emplace_back(link.toNode, link.toPin);
		}
		for (const auto& [node, pin]: removed)
			std::ignore = runCommand("material.unlink", scoped({{"to_node", idText(node)}, {"to_pin", pin}}, m_scope));
		m_canvasStale = true;
	});
	m_canvas.setOnNodeMoved([this](core::UUID, math::vec2f) -> void { m_moving = true; });
	// Material links have no waypoints: the canvas drops the ones a gesture adds.
	m_canvas.setOnLinkWaypointsChanged([this](core::UUID, const std::vector<math::vec2f>&) -> void {
		m_linkDeleted = true;
		m_canvasStale = true;
	});
	m_canvas.setOnNodeSelected([this](const core::UUID iNode) -> void { m_selectedNode = iNode; });
	m_canvas.setOnContextMenu([this](const std::optional<core::UUID> iNode) -> void {
		m_menuNode = iNode;
		const auto mouse = m_canvas.getMouseCanvasPosition();
		m_menuPosition = {mouse.x(), mouse.y()};
		m_openMenu = true;
	});
	m_canvas.setNodeFooterRenderer([this](const core::UUID iNode, const math::vec2f iMin, const math::vec2f iMax,
										  float) -> void { drawThumbnail(iNode, iMin, iMax); });
}

void MaterialDocument::rebuildCanvas() {
	const auto selection = m_canvas.selection();
	m_canvas.clear();
	m_pins.clear();
	m_pinIds.clear();
	if (!isGraph())
		return;
	const auto& graph = *m_asset.graph;
	const auto* function = m_scope.empty() ? nullptr : graph.findFunction(m_scope);
	const auto* nodes = commands::getMaterialScopeNodes(graph, m_scope);
	const auto* links = commands::getMaterialScopeLinks(graph, m_scope);
	if (nodes == nullptr || links == nullptr)
		return;
	std::map<core::UUID, std::string> errors;
	for (const auto& diagnostic: m_program.diagnostics) {
		if (diagnostic.node == core::UUID{0})
			continue;
		auto& text = errors[diagnostic.node];
		text += text.empty() ? diagnostic.message : "\n" + diagnostic.message;
	}
	for (const auto& node: *nodes) {
		const auto* type = renderer::findMaterialNodeType(node.type);
		const auto pins = renderer::getMaterialNodePins(graph, node, function);
		const auto category = type != nullptr ? type->category : std::string{"Graph"};
		const auto error = errors.find(node.id);
		std::string tooltip = type != nullptr ? type->description : std::string{"Unknown node"};
		if (error != errors.end())
			tooltip = error->second;
		gui::widgets::Node canvasNode{
				.id = node.id,
				.title = nodeTitle(graph, node),
				.position = {node.position[0], node.position[1]},
				.inputs = {},
				.outputs = {},
				.customData = {},
				.titleColor = type != nullptr ? math::vec4{1.f, 1.f, 1.f, 1.f} : math::vec4{1.f, 0.3f, 0.3f, 1.f},
				.glyph = categoryGlyph(category),
				.glyphColor = categoryColor(category),
				.faulty = error != errors.end(),
				.tooltip = std::move(tooltip),
				.footerHeight = m_showThumbnails && m_scope.empty() && !pins.outputs.empty() ? g_thumbnailFooter : 0.f};
		for (const auto& [list, output]: {std::pair{&pins.inputs, false}, std::pair{&pins.outputs, true}}) {
			for (const auto& pin: *list) {
				const core::UUID pinId;
				auto& side = output ? canvasNode.outputs : canvasNode.inputs;
				side.push_back({.id = pinId,
								.label = pin.name,
								.typeTag = std::string{commands::getMaterialPinTypeName(pin.type)},
								.kind = output ? gui::widgets::PinKind::Output : gui::widgets::PinKind::Input,
								.labelColor = pinColor(pin.type)});
				m_pins[pinId] = {.node = node.id, .pin = pin.name, .output = output};
				m_pinIds[{node.id, output, pin.name}] = pinId;
			}
		}
		std::ignore = m_canvas.addNode(std::move(canvasNode));
	}
	for (const auto& link: *links) {
		const auto from = m_pinIds.find({link.fromNode, true, link.fromPin});
		const auto to = m_pinIds.find({link.toNode, false, link.toPin});
		if (from != m_pinIds.end() && to != m_pinIds.end())
			std::ignore = m_canvas.addLink(from->second, to->second);
	}
	std::vector<core::UUID> kept;
	for (const auto id: selection)
		if (std::ranges::contains(*nodes, id, &MaterialGraphNode::id))
			kept.push_back(id);
	m_canvas.setSelection(kept);
}

void MaterialDocument::renderToolbar() {
	if (!m_asset.parent.empty())
		return;
	const auto separator = []() -> void {
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
	};
	if (!isGraph()) {
		if (gui::widgets::button("##toGraph", {.glyph = gui::icon::workflow,
											   .label = "Convert to Graph",
											   .tooltip = "Convert to Graph",
											   .description = "Edit the surface as a node graph, starting from these "
															  "values (undoable)."}))
			std::ignore = runCommand("material.convert_to_graph", {});
		return;
	}
	if (gui::widgets::iconButton("##frame", {.glyph = gui::icon::focus,
											 .tooltip = "Frame All",
											 .shortcut = "Home",
											 .description = "Show every node."}))
		m_canvas.requestFitToContent();
	ImGui::SameLine();
	graphSnapButton(mp_editorLayer);
	ImGui::SameLine();
	if (gui::widgets::iconButton("##thumbnails", {.glyph = gui::icon::image,
												  .tooltip = "Node Previews",
												  .description = "Show the output of each node on a small sphere.",
												  .selected = m_showThumbnails}))
		setThumbnails(!m_showThumbnails);
	separator();
	if (!m_scope.empty()) {
		if (gui::widgets::iconButton("##back", {.glyph = gui::icon::arrowLeft,
												.tooltip = "Back to the Graph",
												.description = "Leave the body of the function."}))
			std::ignore = setScope({});
		ImGui::SameLine();
	}
	ImGui::SetNextItemWidth(200.f);
	const auto preview = m_scope.empty() ? std::string{"Graph"} : std::format("Function {}", m_scope);
	if (gui::widgets::beginCombo("##scope", preview.c_str())) {
		if (ImGui::Selectable("Graph", m_scope.empty()))
			std::ignore = setScope({});
		for (const auto& function: m_asset.graph->functions)
			if (ImGui::Selectable(std::format("Function {}", function.name).c_str(), function.name == m_scope))
				std::ignore = setScope(function.name);
		ImGui::EndCombo();
	}
	separator();
	if (m_program.isValid()) {
		gui::widgets::badge(std::format("{} Compiles", gui::icon::circleCheck).c_str(),
							gui::widgets::BadgeKind::Success);
	} else {
		gui::widgets::badge(std::format("{} {} problem{}", gui::icon::triangleAlert, m_program.diagnostics.size(),
										m_program.diagnostics.size() > 1 ? "s" : "")
									.c_str(),
							gui::widgets::BadgeKind::Error);
	}
	separator();
	if (gui::widgets::button("##toParameters",
							 {.glyph = gui::icon::undo2,
							  .label = "Convert to Parameters",
							  .tooltip = "Convert to Parameters",
							  .description = "Drop the graph: the values draw the material again (undoable)."}))
		std::ignore = runCommand("material.convert_to_parameters", {});
}

void MaterialDocument::renderGraphView(const bool iFocused) {
	const float width = std::max(ImGui::GetContentRegionAvail().x * 0.66f, 240.f);
	ImGui::BeginChild("##materialGraphPane", {width, 0.f}, ImGuiChildFlags_ResizeX);
	renderCanvas(iFocused);
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##materialSidePane", {0.f, 0.f}, ImGuiChildFlags_Borders);
	const float previewHeight = std::min(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y * 0.5f);
	ImGui::BeginChild("##materialPreview", {0.f, previewHeight}, ImGuiChildFlags_None,
					  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	renderPreview();
	ImGui::EndChild();
	if (gui::widgets::sectionHeader("Diagnostics", gui::icon::triangleAlert, false).open) {
		renderDiagnostics();
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Settings", gui::icon::slidersHorizontal, false).open) {
		renderGraphSettings();
		gui::widgets::endSection();
	}
	ImGui::EndChild();
}

void MaterialDocument::renderCanvas(const bool iFocused) {
	syncCanvas();
	m_linkDeleted = false;
	if (m_fitFrames > 0) {
		m_canvas.requestFitToContent();
		--m_fitFrames;
	}
	m_canvas.setSnapToGrid(isGraphSnapOn(mp_editorLayer));
	m_canvas.onRender();
	if (ImGui::BeginDragDropTargetCustom({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()},
										 ImGui::GetID("##materialDrop"))) {
		if (const auto* payload = ImGui::AcceptDragDropPayload(g_MaterialNodePayload);
			payload != nullptr && payload->DataSize > 0) {
			const auto mouse = m_canvas.getMouseCanvasPosition();
			addPaletteNode(
					std::string_view{static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize)},
					{mouse.x(), mouse.y()});
		}
		ImGui::EndDragDropTarget();
	}
	if (m_moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		m_moving = false;
		std::string layout;
		if (const auto* nodes = commands::getMaterialScopeNodes(*m_asset.graph, m_scope); nodes != nullptr) {
			for (const auto& canvasNode: m_canvas.nodes()) {
				const auto found = std::ranges::find(*nodes, canvasNode.id, &MaterialGraphNode::id);
				if (found == nodes->end() || (std::abs(found->position[0] - canvasNode.position.x()) < 0.01f &&
											  std::abs(found->position[1] - canvasNode.position.y()) < 0.01f))
					continue;
				layout += std::format("{}{} {} {}", layout.empty() ? "" : "; ", idText(canvasNode.id),
									  canvasNode.position.x(), canvasNode.position.y());
			}
		}
		if (!layout.empty())
			std::ignore = runCommand("material.move_nodes", scoped({{"layout", layout}}, m_scope));
	}
	const auto selection = m_canvas.selection();
	if (iFocused && !ImGui::GetIO().WantTextInput) {
		const bool ctrl = ImGui::GetIO().KeyCtrl;
		const auto mouse = m_canvas.getMouseCanvasPosition();
		if (!m_linkDeleted && !selection.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			std::ignore = runCommand("material.remove_nodes",
									 scoped({{"nodes", commands::formatMaterialNodes(selection)}}, m_scope));
		if (ctrl && !selection.empty() && ImGui::IsKeyPressed(ImGuiKey_C, false))
			ImGui::SetClipboardText(commands::copyMaterialNodes(*m_asset.graph, m_scope, selection).c_str());
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
			pasteClipboard({mouse.x(), mouse.y()});
		if (ctrl && !selection.empty() && ImGui::IsKeyPressed(ImGuiKey_D, false))
			std::ignore = runCommand(
					"material.paste_nodes",
					scoped({{"nodes", commands::copyMaterialNodes(*m_asset.graph, m_scope, selection)}}, m_scope));
		if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
			m_canvas.requestFitToContent();
	}
	renderCanvasMenu();
}

void MaterialDocument::pasteClipboard(const std::array<float, 2> iPosition) {
	const char* text = ImGui::GetClipboardText();
	if (text == nullptr || !std::string_view{text}.contains("Material:"))
		return;
	if (const auto result = runCommand(
				"material.paste_nodes",
				scoped({{"nodes", std::string{text}}, {"x", double{iPosition[0]}}, {"y", double{iPosition[1]}}},
					   m_scope));
		result.has_value())
		m_canvasStale = true;
}

void MaterialDocument::renderCanvasMenu() {
	if (m_openMenu) {
		ImGui::OpenPopup("##materialGraphMenu");
		m_paletteFilter.clear();
		m_openMenu = false;
	}
	if (!ImGui::BeginPopup("##materialGraphMenu"))
		return;
	const auto selection = m_canvas.selection();
	if (m_menuNode) {
		const std::vector<core::UUID> nodes =
				std::ranges::contains(selection, *m_menuNode) ? selection : std::vector<core::UUID>{*m_menuNode};
		gui::widgets::menuHeader(nodes.size() == 1 ? "Node" : std::format("{} Nodes", nodes.size()).c_str(), "Node");
		if (gui::widgets::menuItem({.glyph = gui::icon::clipboardCopy, .label = "Copy", .shortcut = "Ctrl+C"}))
			ImGui::SetClipboardText(commands::copyMaterialNodes(*m_asset.graph, m_scope, nodes).c_str());
		if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
			std::ignore = runCommand(
					"material.paste_nodes",
					scoped({{"nodes", commands::copyMaterialNodes(*m_asset.graph, m_scope, nodes)}}, m_scope));
		if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Del", .danger = true}))
			std::ignore = runCommand("material.remove_nodes",
									 scoped({{"nodes", commands::formatMaterialNodes(nodes)}}, m_scope));
	} else {
		gui::widgets::menuHeader("Add Node", "Palette");
		renderPalette(m_menuPosition, true);
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::clipboardPaste, .label = "Paste Here", .shortcut = "Ctrl+V"}))
			pasteClipboard(m_menuPosition);
		if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Frame All", .shortcut = "Home"}))
			m_canvas.requestFitToContent();
	}
	ImGui::EndPopup();
}

void MaterialDocument::addPaletteNode(const std::string_view iEntry, const std::array<float, 2> iPosition) {
	const auto bar = iEntry.find('|');
	commands::CommandArgs args{{"type", std::string{iEntry.substr(0, bar)}},
							   {"x", double{iPosition[0]}},
							   {"y", double{iPosition[1]}}};
	if (bar != std::string_view::npos)
		args.set("item", std::string{iEntry.substr(bar + 1)});
	if (const auto result = runCommand("material.add_node", scoped(std::move(args), m_scope)); result.has_value()) {
		if (const auto node = result->data.find("node"); node != result->data.end())
			if (const auto* text = std::get_if<std::string>(&node->second); text != nullptr)
				m_selectedNode = core::UUID{std::stoull(*text)};
	}
}

void MaterialDocument::renderPalette(const std::array<float, 2> iPosition, const bool iPopup) {
	if (!isGraph())
		return;
	if (iPopup && ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	std::ignore = gui::widgets::searchField(iPopup ? "##matPaletteSearch" : "##matLibrarySearch", m_paletteFilter,
											"Search nodes");
	const auto entries = paletteEntries(*m_asset.graph, m_scope);
	std::optional<std::string> clicked;
	const auto item = [&](const PaletteEntry& iEntry) -> void {
		ImGui::PushID(iEntry.entry.c_str());
		const auto row = gui::widgets::treeRow("##entry", {.label = iEntry.label.c_str(),
														   .glyph = categoryGlyph(iEntry.category),
														   .glyphColor = gui::Theme::mix(categoryColor(iEntry.category),
																						 {1.f, 1.f, 1.f, 1.f}, 0.45f)});
		if (row.clicked)
			clicked = iEntry.entry;
		if (!iPopup && ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(g_MaterialNodePayload, iEntry.entry.data(), iEntry.entry.size());
			ImGui::Text("%s %s", categoryGlyph(iEntry.category), iEntry.label.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s", iEntry.label.c_str(), iEntry.description.c_str());
		if (row.open)
			gui::widgets::endTreeRow();
		ImGui::PopID();
	};
	if (!m_paletteFilter.empty()) {
		for (const auto& entry: entries)
			if (gui::widgets::matchesFilter(entry.label, m_paletteFilter) ||
				gui::widgets::matchesFilter(entry.description, m_paletteFilter) ||
				gui::widgets::matchesFilter(entry.category, m_paletteFilter))
				item(entry);
	} else {
		std::vector<std::string> categories;
		for (const auto& entry: entries)
			if (!std::ranges::contains(categories, entry.category))
				categories.push_back(entry.category);
		for (const auto& category: categories) {
			ImGui::PushID(category.c_str());
			const auto row = gui::widgets::treeRow("##category", {.label = category.c_str(),
																  .glyph = categoryGlyph(category),
																  .glyphColor = categoryColor(category),
																  .hasChildren = true,
																  .defaultOpen = iPopup});
			if (row.open) {
				for (const auto& entry: entries)
					if (entry.category == category)
						item(entry);
				gui::widgets::endTreeRow();
			}
			ImGui::PopID();
		}
	}
	if (clicked.has_value()) {
		addPaletteNode(*clicked, iPosition);
		if (iPopup)
			ImGui::CloseCurrentPopup();
	}
}

void MaterialDocument::renderDiagnostics() {
	if (m_program.diagnostics.empty()) {
		ImGui::TextDisabled("%s The graph compiles.", gui::icon::circleCheck);
		return;
	}
	for (size_t index = 0; index < m_program.diagnostics.size(); ++index) {
		const auto& diagnostic = m_program.diagnostics[index];
		ImGui::PushID(static_cast<int>(index));
		std::string where = "Graph";
		const MaterialGraphNode* node = nullptr;
		for (const auto& candidate: m_asset.graph->nodes)
			if (candidate.id == diagnostic.node)
				node = &candidate;
		if (node != nullptr)
			where = nodeTitle(*m_asset.graph, *node);
		ImGui::PushStyleColor(ImGuiCol_Text, gui::vec(gui::UiLayer::getTheme().error));
		ImGui::TextUnformatted(gui::icon::triangleAlert);
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Selectable(std::format("{}: {}", where, diagnostic.message).c_str(), false) && node != nullptr) {
			std::ignore = setScope({});
			syncCanvas();
			selectNode(node->id);
		}
		ImGui::PopID();
	}
}

void MaterialDocument::renderGraphSettings() {
	using renderer::AlphaMode;
	using renderer::ShadingModel;
	auto values = m_asset.values;
	const renderer::MaterialParameters defaults;
	bool changed = false;
	if (gui::widgets::beginProperties("##graphSettings")) {
		static constexpr std::array g_shadings{"Standard", "Unlit", "Toon"};
		static constexpr std::array g_alphas{"Opaque", "Mask", "Blend"};
		gui::widgets::propertyLabel("Shading", "Reaction to the lights; the graph gives the surface.");
		if (const auto picked = enumCombo("##shading", g_shadings, static_cast<size_t>(values.shadingModel))) {
			values.shadingModel = static_cast<ShadingModel>(*picked);
			changed = true;
		}
		if (gui::widgets::propertyReset(values.shadingModel != defaults.shadingModel)) {
			values.shadingModel = defaults.shadingModel;
			changed = true;
		}
		gui::widgets::propertyLabel("Alpha", "Use of the alpha of the surface.");
		if (const auto picked = enumCombo("##alpha", g_alphas, static_cast<size_t>(values.alphaMode))) {
			values.alphaMode = static_cast<AlphaMode>(*picked);
			changed = true;
		}
		if (gui::widgets::propertyReset(values.alphaMode != defaults.alphaMode)) {
			values.alphaMode = defaults.alphaMode;
			changed = true;
		}
		if (values.alphaMode == AlphaMode::Mask)
			changed = gui::widgets::propertyFloat("Alpha cutoff", values.alphaCutoff, defaults.alphaCutoff, 0.01f,
												  "%.2f", "Alpha below which a fragment is discarded.") ||
					  changed;
		changed = gui::widgets::propertyBool("Double sided", values.doubleSided, defaults.doubleSided,
											 "Both faces lit, the back face with its normal flipped.") ||
				  changed;
		changed = gui::widgets::propertyFloat("IOR", values.ior, defaults.ior, 0.005f, "%.3f",
											  "Index of refraction of a dielectric: water 1.33, glass 1.5.") ||
				  changed;
		changed = gui::widgets::propertyFloat("Clearcoat IOR", values.clearcoatIor, defaults.clearcoatIor, 0.005f,
											  "%.3f", "Index of refraction of the clearcoat.") ||
				  changed;
		if (values.shadingModel == ShadingModel::Toon) {
			auto bands = static_cast<int>(values.toonBands);
			if (gui::widgets::propertyInt("Toon bands", bands, static_cast<int>(defaults.toonBands), 1, 16, "%d",
										  "Number of diffuse bands.")) {
				values.toonBands = static_cast<uint32_t>(bands);
				changed = true;
			}
			changed = gui::widgets::propertyFloat("Toon rim", values.toonRim, defaults.toonRim, 0.01f, "%.2f",
												  "Strength of the rim.") ||
					  changed;
		}
		gui::widgets::endProperties();
	}
	values.alphaCutoff = std::clamp(values.alphaCutoff, 0.f, 1.f);
	values.ior = std::clamp(values.ior, 1.f, 3.f);
	values.clearcoatIor = std::clamp(values.clearcoatIor, 1.f, 3.f);
	if (changed) {
		const renderer::MaterialOverrides overrides{.fields = renderer::g_allMaterialFields, .values = values};
		std::ignore = runCommand("material.set_values", {{"values", overrides.toYaml()}, {"merge", isDragging()}});
	}
}

void MaterialDocument::renderHierarchyPanel() {
	if (!isGraph()) {
		std::ignore = gui::widgets::emptyState(gui::icon::workflow, "No graph",
											   "Convert the material to a graph to edit its nodes.");
		return;
	}
	if (!ImGui::BeginTabBar("##materialTabs"))
		return;
	if (ImGui::BeginTabItem(std::format("{} Nodes###nodes", gui::icon::workflow).c_str())) {
		const auto centre = m_canvas.getMouseCanvasPosition();
		renderPalette({centre.x(), centre.y()}, false);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Parameters###parameters", gui::icon::slidersHorizontal).c_str())) {
		renderGraphParameters();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Custom###custom", gui::icon::code).c_str())) {
		renderCustomNodes();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem(std::format("{} Functions###functions", gui::icon::squareFunction).c_str())) {
		renderFunctions();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void MaterialDocument::renderPropertiesPanel() {
	if (!isGraph() || !m_selectedNode) {
		std::ignore = gui::widgets::emptyState(gui::icon::workflow, "No node selected",
											   "Click a node of the graph to edit its settings and inputs.");
		return;
	}
	renderNodeInspector();
}

void MaterialDocument::renderGraphParameters() {
	const auto& parameters = m_asset.graph->parameters;
	size_t numbers = 0;
	size_t textures = 0;
	for (const auto& parameter: parameters) ++(parameter.type == MaterialParameterType::Texture ? textures : numbers);
	ImGui::TextDisabled("%zu / %zu numbers, %zu / %zu textures", numbers, renderer::g_materialGraphValueCount, textures,
						renderer::g_materialGraphTextureCount);
	static constexpr std::array g_typeNames{"Float", "Vector", "Color", "Texture"};
	for (size_t index = 0; index < parameters.size(); ++index) {
		const auto parameter = parameters[index];
		ImGui::PushID(static_cast<int>(index));
		const auto section = gui::widgets::sectionHeader(parameter.name.c_str(), gui::icon::slidersHorizontal, true);
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format(
					"{}|{}", parameter.type == MaterialParameterType::Texture ? "texture.sample" : "param.get",
					parameter.name);
			ImGui::SetDragDropPayload(g_MaterialNodePayload, payload.data(), payload.size());
			ImGui::Text("%s %s", gui::icon::slidersHorizontal, parameter.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##parameterMenu");
		if (ImGui::BeginPopup("##parameterMenu")) {
			gui::widgets::menuHeader(parameter.name.c_str(), "Parameter");
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				std::ignore = runCommand("material.remove_parameter", {{"name", parameter.name}});
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##parameter")) {
				auto name = parameter.name;
				gui::widgets::propertyLabel("Name", "Slang name of the parameter; its nodes follow a rename.");
				if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && name != parameter.name)
					std::ignore = runCommand("material.set_parameter", {{"name", parameter.name}, {"new_name", name}});
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::propertyLabel("Type", "Kind of value; a change resets the default.");
				if (const auto picked = enumCombo("##type", g_typeNames, static_cast<size_t>(parameter.type)))
					std::ignore = runCommand("material.set_parameter",
											 {{"name", parameter.name}, {"type", std::string{g_typeNames[*picked]}}});
				std::ignore = gui::widgets::propertyReset(false);
				auto value = parameter.defaultValue;
				gui::widgets::propertyLabel("Default", "Value of the material; a child material overrides it.");
				if (parameterValueField("##default", parameter.type, value))
					std::ignore = runCommand("material.set_parameter",
											 {{"name", parameter.name}, {"default", value}, {"merge", isDragging()}});
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (parameters.empty())
		std::ignore =
				gui::widgets::emptyState(gui::icon::slidersHorizontal, "No parameter",
										 "A parameter is a value a child material overrides without a recompile.");
	ImGui::Spacing();
	if (gui::widgets::button("##addParameter", {.glyph = gui::icon::plus,
												.label = "Add Parameter",
												.tooltip = "Add Parameter",
												.description = "A number, vector, colour or texture the graph exposes.",
												.width = ImGui::GetContentRegionAvail().x}))
		ImGui::OpenPopup("##addParameterMenu");
	if (ImGui::BeginPopup("##addParameterMenu")) {
		for (const auto type: g_parameterTypes)
			if (gui::widgets::menuItem({.label = g_typeNames[static_cast<size_t>(type)]}))
				std::ignore =
						runCommand("material.add_parameter",
								   {{"name", std::string{type == MaterialParameterType::Texture ? "map" : "value"}},
									{"type", std::string{g_typeNames[static_cast<size_t>(type)]}}});
		ImGui::EndPopup();
	}
}

auto MaterialDocument::renderPins(const char* iTitle, const std::vector<MaterialPinSpec>& iPins, const char* iBase)
		-> std::optional<std::vector<MaterialPinSpec>> {
	static constexpr std::array g_typeNames{"Float", "Float2", "Float3", "Float4"};
	if (!gui::widgets::subHeader(iTitle, true))
		return std::nullopt;
	std::optional<std::vector<MaterialPinSpec>> result;
	std::optional<size_t> erased;
	for (size_t index = 0; index < iPins.size(); ++index) {
		ImGui::PushID(static_cast<int>(index));
		auto edited = iPins;
		auto& pin = edited[index];
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		if (ImGui::InputText("##name", &pin.name, ImGuiInputTextFlags_EnterReturnsTrue))
			result = edited;
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
		const auto current = static_cast<size_t>(std::ranges::find(g_pinTypes, pin.type) - g_pinTypes.begin());
		if (const auto picked = enumCombo("##type", g_typeNames, std::min(current, g_pinTypes.size() - 1))) {
			pin.type = g_pinTypes[*picked];
			result = edited;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.f);
		ImGui::InputTextWithHint("##default", "default", &pin.defaultValue);
		if (ImGui::IsItemDeactivatedAfterEdit())
			result = edited;
		ImGui::SameLine();
		if (gui::widgets::iconButton("##remove", {.glyph = gui::icon::x, .tooltip = "Remove"}))
			erased = index;
		ImGui::PopID();
	}
	if (erased.has_value()) {
		auto edited = iPins;
		edited.erase(edited.begin() + static_cast<std::ptrdiff_t>(*erased));
		result = std::move(edited);
	}
	if (gui::widgets::button("##add", {.glyph = gui::icon::plus, .label = "Add"})) {
		auto edited = iPins;
		std::string name = iBase;
		for (uint32_t counter = 2; std::ranges::contains(edited, name, &MaterialPinSpec::name); ++counter)
			name = std::format("{}_{}", iBase, counter);
		edited.push_back({.name = name, .type = MaterialPinType::Float, .defaultValue = "0"});
		result = std::move(edited);
	}
	return result;
}

void MaterialDocument::renderCustomNodes() {
	const auto customs = m_asset.graph->customNodes;
	for (size_t index = 0; index < customs.size(); ++index) {
		const auto& custom = customs[index];
		ImGui::PushID(static_cast<int>(index));
		const auto section = gui::widgets::sectionHeader(
				custom.title.empty() ? custom.name.c_str() : custom.title.c_str(), gui::icon::code, true, false);
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format("custom|{}", custom.name);
			ImGui::SetDragDropPayload(g_MaterialNodePayload, payload.data(), payload.size());
			ImGui::Text("%s %s", gui::icon::code, custom.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##customMenu");
		if (ImGui::BeginPopup("##customMenu")) {
			gui::widgets::menuHeader(custom.name.c_str(), "Custom Node");
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				std::ignore = runCommand("material.remove_custom_node", {{"name", custom.name}});
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##custom")) {
				auto name = custom.name;
				gui::widgets::propertyLabel("Name", "Slang name of the node; its nodes follow a rename.");
				if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && name != custom.name)
					std::ignore = runCommand("material.set_custom_node", {{"name", custom.name}, {"new_name", name}});
				std::ignore = gui::widgets::propertyReset(false);
				auto title = custom.title;
				gui::widgets::propertyLabel("Title", "Title shown on the node.");
				if (ImGui::InputText("##title", &title))
					std::ignore = runCommand("material.set_custom_node",
											 {{"name", custom.name}, {"title", title}, {"merge", true}});
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			ImGui::PushID("inputs");
			if (const auto inputs = renderPins("Inputs", custom.inputs, "value"); inputs.has_value())
				std::ignore = runCommand("material.set_custom_node",
										 {{"name", custom.name}, {"inputs", commands::formatMaterialPins(*inputs)}});
			ImGui::PopID();
			ImGui::PushID("outputs");
			if (const auto outputs = renderPins("Outputs", custom.outputs, "result"); outputs.has_value())
				std::ignore = runCommand("material.set_custom_node",
										 {{"name", custom.name}, {"outputs", commands::formatMaterialPins(*outputs)}});
			ImGui::PopID();
			if (gui::widgets::subHeader("Slang", true)) {
				auto code = custom.code;
				gui::UiLayer::pushTextStyle(gui::TextStyle::Code);
				if (ImGui::InputTextMultiline("##code", &code,
											  {ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight() * 10.f},
											  ImGuiInputTextFlags_AllowTabInput))
					std::ignore = runCommand("material.set_custom_node",
											 {{"name", custom.name}, {"code", code}, {"merge", true}});
				gui::UiLayer::popTextStyle();
				ImGui::TextDisabled("The inputs are variables of the body; assign every output.");
			}
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (customs.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::code, "No custom node",
											   "A custom node runs Slang you write, with the pins you give it.");
	ImGui::Spacing();
	if (gui::widgets::button("##addCustom", {.glyph = gui::icon::plus,
											 .label = "Add Custom Node",
											 .tooltip = "Add Custom Node",
											 .description = "Typed inputs and outputs, a Slang body.",
											 .width = ImGui::GetContentRegionAvail().x}))
		std::ignore = runCommand("material.add_custom_node", {});
}

void MaterialDocument::renderFunctions() {
	const auto functions = m_asset.graph->functions;
	for (size_t index = 0; index < functions.size(); ++index) {
		const auto& function = functions[index];
		ImGui::PushID(static_cast<int>(index));
		const auto section = gui::widgets::sectionHeader(function.name.c_str(), gui::icon::squareFunction, true, false);
		if (ImGui::BeginDragDropSource()) {
			const auto payload = std::format("function.call|{}", function.name);
			ImGui::SetDragDropPayload(g_MaterialNodePayload, payload.data(), payload.size());
			ImGui::Text("%s %s", gui::icon::squareFunction, function.name.c_str());
			ImGui::EndDragDropSource();
		}
		if (section.menuClicked)
			ImGui::OpenPopup("##functionMenu");
		if (ImGui::BeginPopup("##functionMenu")) {
			gui::widgets::menuHeader(function.name.c_str(), "Function");
			if (gui::widgets::menuItem({.glyph = gui::icon::workflow, .label = "Edit Body"}))
				std::ignore = setScope(function.name);
			if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Remove", .danger = true}))
				std::ignore = runCommand("material.remove_function", {{"name", function.name}});
			ImGui::EndPopup();
		}
		if (section.open) {
			if (gui::widgets::beginProperties("##function")) {
				auto name = function.name;
				gui::widgets::propertyLabel("Name", "Slang name of the function; its calls follow a rename.");
				if (ImGui::InputText("##name", &name, ImGuiInputTextFlags_EnterReturnsTrue) && name != function.name) {
					const bool shown = m_scope == function.name;
					if (runCommand("material.set_function", {{"name", function.name}, {"new_name", name}}) && shown)
						std::ignore = setScope(name);
				}
				std::ignore = gui::widgets::propertyReset(false);
				gui::widgets::endProperties();
			}
			ImGui::PushID("inputs");
			if (const auto inputs = renderPins("Inputs", function.inputs, "value"); inputs.has_value())
				std::ignore = runCommand("material.set_function",
										 {{"name", function.name}, {"inputs", commands::formatMaterialPins(*inputs)}});
			ImGui::PopID();
			ImGui::PushID("outputs");
			if (const auto outputs = renderPins("Outputs", function.outputs, "result"); outputs.has_value())
				std::ignore =
						runCommand("material.set_function",
								   {{"name", function.name}, {"outputs", commands::formatMaterialPins(*outputs)}});
			ImGui::PopID();
			if (gui::widgets::button("##editBody", {.glyph = gui::icon::workflow,
													.label = m_scope == function.name ? "Body Shown" : "Edit Body",
													.tooltip = "Edit Body",
													.description = "Show the nodes of the function on the canvas.",
													.disabled = m_scope == function.name}))
				std::ignore = setScope(function.name);
			gui::widgets::endSection();
		}
		ImGui::PopID();
	}
	if (functions.empty())
		std::ignore = gui::widgets::emptyState(gui::icon::squareFunction, "No function",
											   "A function is a sub-graph used as one node, in this graph only.");
	ImGui::Spacing();
	if (gui::widgets::button("##addFunction", {.glyph = gui::icon::plus,
											   .label = "Add Function",
											   .tooltip = "Add Function",
											   .description = "Typed inputs and outputs, a body of nodes.",
											   .width = ImGui::GetContentRegionAvail().x})) {
		if (const auto result = runCommand("material.add_function", {}); result.has_value())
			if (const auto name = result->data.find("name"); name != result->data.end())
				if (const auto* text = std::get_if<std::string>(&name->second); text != nullptr)
					std::ignore = setScope(*text);
	}
}

void MaterialDocument::renderGraphOverrides() {
	if (m_asset.parent.empty())
		return;
	auto* library =
			app::Application::instanced() ? &app::Application::get().getEngineContext().getMaterialLibrary() : nullptr;
	const auto parent = library != nullptr ? library->get(m_asset.parent) : nullptr;
	if (!parent || !parent->getGraph() || parent->getGraph()->parameters.empty())
		return;
	if (!gui::widgets::sectionHeader("Graph Parameters", gui::icon::slidersHorizontal, false).open)
		return;
	ImGui::TextDisabled("Check a parameter to override it; the others follow the parent.");
	if (gui::widgets::beginProperties("##graphOverrides")) {
		for (const auto& parameter: parent->getGraph()->parameters) {
			ImGui::PushID(parameter.name.c_str());
			const auto own = m_asset.graphValues.find(parameter.name);
			bool overridden = own != m_asset.graphValues.end();
			const auto inherited = parent->getGraphValues().find(parameter.name);
			auto value = inherited != parent->getGraphValues().end() ? inherited->second : parameter.defaultValue;
			if (overridden)
				value = own->second;
			gui::widgets::propertyLabel(parameter.name.c_str(), "Checked: this material sets the value.");
			if (ImGui::Checkbox("##override", &overridden))
				std::ignore =
						overridden
								? runCommand("material.set_graph_value", {{"name", parameter.name}, {"value", value}})
								: runCommand("material.set_graph_value", {{"name", parameter.name}, {"reset", true}});
			ImGui::SameLine();
			ImGui::BeginDisabled(!overridden);
			if (parameterValueField("##value", parameter.type, value))
				std::ignore = runCommand("material.set_graph_value",
										 {{"name", parameter.name}, {"value", value}, {"merge", isDragging()}});
			ImGui::EndDisabled();
			std::ignore = gui::widgets::propertyReset(false);
			ImGui::PopID();
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

void MaterialDocument::renderNodeInspector() {
	const auto& graph = *m_asset.graph;
	const auto* nodes = commands::getMaterialScopeNodes(graph, m_scope);
	const auto* links = commands::getMaterialScopeLinks(graph, m_scope);
	if (nodes == nullptr || links == nullptr)
		return;
	const auto found = std::ranges::find(*nodes, *m_selectedNode, &MaterialGraphNode::id);
	if (found == nodes->end())
		return;
	// NOLINTNEXTLINE(performance-unnecessary-copy-initialization): the commands of the inspector replace the graph.
	const auto node = *found;
	const auto* type = renderer::findMaterialNodeType(node.type);
	if (type == nullptr) {
		ImGui::TextDisabled("Unknown node type '%s'.", node.type.c_str());
		return;
	}
	const auto section = gui::widgets::sectionHeader(nodeTitle(graph, node).c_str(), categoryGlyph(type->category),
													 false, true, categoryColor(type->category));
	if (!section.open)
		return;
	ImGui::TextWrapped("%s", type->description.c_str());
	for (const auto& diagnostic: m_program.diagnostics)
		if (diagnostic.node == node.id)
			gui::widgets::notice(diagnostic.message.c_str(), gui::widgets::BadgeKind::Error);
	const auto* function = m_scope.empty() ? nullptr : graph.findFunction(m_scope);
	const auto pins = renderer::getMaterialNodePins(graph, node, function);
	if (gui::widgets::beginProperties("##materialNode")) {
		for (const auto& setting: type->settings) renderNodeSetting(node, setting);
		for (const auto& input: pins.inputs) renderNodeInput(node, input);
		gui::widgets::endProperties();
	}
	if (pins.inputs.empty() && type->settings.empty())
		ImGui::TextDisabled("This node has no setting.");
	gui::widgets::endSection();
}

void MaterialDocument::setNodeValue(const core::UUID iNode, const std::string& iName,
									const std::optional<std::string>& iValue, const bool iMerge) {
	commands::CommandArgs args{{"node", idText(iNode)}, {"name", iName}, {"merge", iMerge}};
	if (iValue.has_value())
		args.set("value", *iValue);
	else
		args.set("reset", true);
	std::ignore = runCommand("material.set_node_value", scoped(std::move(args), m_scope));
}

void MaterialDocument::renderNodeSetting(const MaterialGraphNode& iNode,
										 const renderer::MaterialSettingSpec& iSetting) {
	const auto& node = iNode;
	const auto& setting = iSetting;
	const auto& graph = *m_asset.graph;
	const auto set = [&](const std::string& iName, const std::string& iValue, const bool iMerge) -> void {
		setNodeValue(node.id, iName, iValue, iMerge);
	};
	const auto reset = [&](const std::string& iName) -> void { setNodeValue(node.id, iName, std::nullopt, false); };
	ImGui::PushID(setting.name.c_str());
	auto value = renderer::getMaterialNodeValue(node, setting.name);
	const bool modified = node.values.contains(setting.name);
	gui::widgets::propertyLabel(setting.name.c_str(), setting.description.c_str());
	switch (setting.type) {
		case MaterialValueType::Float:
			{
				auto numbers = parseNumbers(value, 1);
				if (ImGui::DragFloat("##value", numbers.data(), 0.01f))
					set(setting.name, formatNumbers(std::span{numbers}.first(1)), isDragging());
				break;
			}
		case MaterialValueType::Vector:
			{
				auto numbers = parseNumbers(value, 4);
				if (gui::widgets::vectorField("##value", numbers, 0.01f))
					set(setting.name, formatNumbers(numbers), isDragging());
				break;
			}
		case MaterialValueType::Color:
			{
				auto numbers = parseNumbers(value, 4);
				if (ImGui::ColorEdit4("##value", numbers.data()))
					set(setting.name, formatNumbers(numbers), isDragging());
				break;
			}
		case MaterialValueType::Choice:
			if (gui::widgets::beginCombo("##value", value.c_str())) {
				for (const auto& choice: setting.choices)
					if (ImGui::Selectable(choice.c_str(), choice == value))
						set(setting.name, choice, false);
				ImGui::EndCombo();
			}
			break;
		case MaterialValueType::Name:
			{
				std::vector<std::string> names;
				if (setting.name == "parameter" || setting.name == "texture") {
					for (const auto& parameter: graph.parameters)
						if ((parameter.type == MaterialParameterType::Texture) == (setting.name == "texture"))
							names.push_back(parameter.name);
				} else if (setting.name == "custom") {
					for (const auto& custom: graph.customNodes) names.push_back(custom.name);
				} else if (setting.name == "function") {
					for (const auto& candidate: graph.functions)
						if (candidate.name != m_scope)
							names.push_back(candidate.name);
				}
				if (gui::widgets::beginCombo("##value", value.empty() ? "(none)" : value.c_str())) {
					for (const auto& name: names)
						if (ImGui::Selectable(name.c_str(), name == value))
							set(setting.name, name, false);
					if (names.empty())
						ImGui::TextDisabled("Nothing of that kind in the graph.");
					ImGui::EndCombo();
				}
				break;
			}
	}
	if (gui::widgets::propertyReset(modified))
		reset(setting.name);
	ImGui::PopID();
}

void MaterialDocument::renderNodeInput(const MaterialGraphNode& iNode, const MaterialPinSpec& iInput) {
	const auto& node = iNode;
	const auto& input = iInput;
	const auto* links = commands::getMaterialScopeLinks(*m_asset.graph, m_scope);
	if (links == nullptr)
		return;
	const auto set = [&](const std::string& iName, const std::string& iValue, const bool iMerge) -> void {
		setNodeValue(node.id, iName, iValue, iMerge);
	};
	const auto reset = [&](const std::string& iName) -> void { setNodeValue(node.id, iName, std::nullopt, false); };
	ImGui::PushID(input.name.c_str());
	const bool linked = std::ranges::any_of(*links, [&](const renderer::MaterialGraphLink& iLink) -> bool {
		return iLink.toNode == node.id && iLink.toPin == input.name;
	});
	const auto own = node.values.find(input.name);
	const bool modified = own != node.values.end();
	auto value = modified ? own->second : input.defaultValue;
	gui::widgets::propertyLabel(
			input.name.c_str(),
			input.defaultValue.starts_with('@')
					? std::format("Unlinked: the mesh {} unless a value is set.", input.defaultValue.substr(1)).c_str()
					: "Value of the input while it is not linked.");
	if (linked) {
		ImGui::TextDisabled("linked");
	} else if (const auto width = widthOf(input.type); width > 0) {
		auto numbers = parseNumbers(value, width);
		const bool changed = width == 1 ? ImGui::DragFloat("##value", numbers.data(), 0.01f)
										: gui::widgets::vectorField("##value", std::span{numbers}.first(width), 0.01f);
		if (changed)
			set(input.name, formatNumbers(std::span{numbers}.first(width)), isDragging());
	} else {
		ImGui::InputTextWithHint("##value", "numbers", &value);
		if (ImGui::IsItemDeactivatedAfterEdit())
			set(input.name, value, false);
	}
	if (gui::widgets::propertyReset(modified && !linked))
		reset(input.name);
	ImGui::PopID();
}

}// namespace owl::nest
