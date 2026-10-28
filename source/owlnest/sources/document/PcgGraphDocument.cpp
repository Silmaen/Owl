/**
 * @file PcgGraphDocument.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "PcgGraphDocument.h"

#include "EditorLayer.h"

#include <gui/IconFont.h>
#include <gui/UiLayer.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/Tooltip.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <format>
#include <ranges>
#include <set>
#include <typeinfo>
#include <utility>
#include <variant>

namespace owl::nest {

namespace {

class PcgGraphCommand final : public UndoCommand<scene::pcg::Graph> {
public:
	PcgGraphCommand(scene::pcg::Graph iBefore, scene::pcg::Graph iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(scene::pcg::Graph& ioTarget) override { ioTarget = m_before; }

	void redo(scene::pcg::Graph& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return typeid(PcgGraphCommand).hash_code(); }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto* other = dynamic_cast<const PcgGraphCommand*>(&iOther);
		if (!m_merge || other == nullptr || !other->m_merge || other->m_description != m_description)
			return false;
		m_after = other->m_after;
		return true;
	}

private:
	scene::pcg::Graph m_before;
	scene::pcg::Graph m_after;
	std::string m_description;
	bool m_merge;
};

auto pinColor(const scene::pcg::PinType iType) -> math::vec4 {
	return iType == scene::pcg::PinType::Field ? math::vec4{0.55f, 0.8f, 1.f, 1.f} : math::vec4{1.f, 0.75f, 0.35f, 1.f};
}

auto categoryColor(const std::string& iCategory) -> math::vec4 {
	if (iCategory == "Sources")
		return {0.6f, 0.9f, 0.6f, 1.f};
	if (iCategory == "Points")
		return {1.f, 0.8f, 0.45f, 1.f};
	if (iCategory == "Outputs")
		return {1.f, 0.55f, 0.55f, 1.f};
	return {0.65f, 0.8f, 1.f, 1.f};
}

// A colour per tile index, stable between frames.
auto tileColor(const int32_t iTile) -> ImU32 {
	const auto hash = static_cast<uint32_t>(iTile) * 2654435761u;
	const float hue = static_cast<float>(hash % 360u) / 360.f;
	ImVec4 rgb{0.f, 0.f, 0.f, 1.f};
	ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, rgb.x, rgb.y, rgb.z);
	return ImGui::ColorConvertFloat4ToU32(rgb);
}

auto parseFloat(const std::string& iText) -> float {
	float value = 0.f;
	std::ignore = std::from_chars(iText.data(), iText.data() + iText.size(), value);
	return value;
}

}// namespace

PcgGraphDocument::PcgGraphDocument() = default;

PcgGraphDocument::~PcgGraphDocument() = default;

auto PcgGraphDocument::starterGraph() -> scene::pcg::Graph {
	scene::pcg::Graph graph;
	const core::UUID noise;
	const core::UUID threshold;
	const core::UUID output;
	graph.nodes.push_back({.id = noise, .type = "source.noise", .position = {0.f, 0.f}, .values = {}});
	graph.nodes.push_back({.id = threshold, .type = "field.threshold", .position = {220.f, 0.f}, .values = {}});
	graph.nodes.push_back({.id = output, .type = "output.tiles", .position = {440.f, 0.f}, .values = {}});
	graph.links.push_back({.fromNode = noise, .fromPin = "Field", .toNode = threshold, .toPin = "Field"});
	graph.links.push_back({.fromNode = threshold, .fromPin = "Mask", .toNode = output, .toPin = "Mask"});
	return graph;
}

auto PcgGraphDocument::isDirty() const -> bool { return scene::pcg::serialize(m_graph) != m_savedText; }

void PcgGraphDocument::onAttach(EditorLayer* iEditor) {
	NodeGraphDocument::onAttach(iEditor);
	m_savedText = scene::pcg::serialize(m_graph);
	rebuildCanvas();
}

void PcgGraphDocument::onCanvasReady() {
	m_canvas.setLinkValidator([this](const core::UUID iFrom, const core::UUID iTo) -> bool {
		const auto from = m_pins.find(iFrom);
		const auto to = m_pins.find(iTo);
		return from != m_pins.end() && to != m_pins.end() && scene::pcg::canConnect(from->second.type, to->second.type);
	});
	m_canvas.setOnLinkCreated([this](core::UUID, const core::UUID iFrom, const core::UUID iTo) -> void {
		const auto& from = m_pins.at(iFrom);
		const auto& to = m_pins.at(iTo);
		if (!connect(from.node, from.pin, to.node, to.pin))
			rebuildCanvas();
	});
	m_canvas.setOnLinkDeleted([this](core::UUID) -> void {
		m_linkDeleted = true;
		auto next = m_graph;
		std::erase_if(next.links, [this](const scene::pcg::GraphLink& iLink) -> bool {
			const auto from = m_pinIds.find({iLink.fromNode, true, iLink.fromPin});
			const auto to = m_pinIds.find({iLink.toNode, false, iLink.toPin});
			return from == m_pinIds.end() || to == m_pinIds.end() ||
				   std::ranges::none_of(m_canvas.links(), [&](const gui::widgets::Link& iCanvasLink) -> bool {
					   return iCanvasLink.fromPin == from->second && iCanvasLink.toPin == to->second;
				   });
		});
		std::ignore = commit(std::move(next), "Delete link");
	});
	m_canvas.setOnNodeMoved([this](core::UUID, math::vec2f) -> void { m_moving = true; });
	m_canvas.setOnNodeSelected([this](const core::UUID iNode) -> void { m_selectedNode = iNode; });
	m_canvas.setOnContextMenu([this](const std::optional<core::UUID> iNode) -> void {
		m_menuNode = iNode;
		const auto mouse = m_canvas.getMouseCanvasPosition();
		m_menuPosition = {mouse.x(), mouse.y()};
		m_openMenu = true;
	});
}

void PcgGraphDocument::rebuildCanvas() {
	const auto selection = m_canvas.selection();
	m_canvas.clear();
	m_pins.clear();
	m_pinIds.clear();
	for (const auto& node: m_graph.nodes) {
		const auto* type = scene::pcg::findNodeType(node.type);
		gui::widgets::Node canvasNode{.id = node.id,
									  .title = type != nullptr ? type->title : std::format("? {}", node.type),
									  .position = {node.position[0], node.position[1]},
									  .inputs = {},
									  .outputs = {},
									  .customData = {},
									  .titleColor = type != nullptr ? categoryColor(type->category)
																	: math::vec4{1.f, 0.3f, 0.3f, 1.f}};
		if (type != nullptr) {
			for (const auto& [pins, output, kind]: {std::tuple{&type->inputs, false, gui::widgets::PinKind::Input},
													std::tuple{&type->outputs, true, gui::widgets::PinKind::Output}}) {
				for (const auto& pin: *pins) {
					const core::UUID pinId;
					auto& side = output ? canvasNode.outputs : canvasNode.inputs;
					side.push_back({.id = pinId,
									.label = pin.name,
									.typeTag = pin.type == scene::pcg::PinType::Field ? "field" : "points",
									.kind = kind,
									.labelColor = pinColor(pin.type)});
					m_pins[pinId] = {.node = node.id, .pin = pin.name, .output = output, .type = pin.type};
					m_pinIds[{node.id, output, pin.name}] = pinId;
				}
			}
		}
		std::ignore = m_canvas.addNode(std::move(canvasNode));
	}
	for (const auto& link: m_graph.links) {
		const auto from = m_pinIds.find({link.fromNode, true, link.fromPin});
		const auto to = m_pinIds.find({link.toNode, false, link.toPin});
		if (from != m_pinIds.end() && to != m_pinIds.end())
			std::ignore = m_canvas.addLink(from->second, to->second);
	}
	std::vector<core::UUID> kept;
	for (const auto id: selection) {
		if (m_graph.findNode(id) != nullptr)
			kept.push_back(id);
	}
	m_canvas.setSelection(kept);
	if (m_selectedNode && m_graph.findNode(*m_selectedNode) == nullptr)
		m_selectedNode.reset();
}

auto PcgGraphDocument::commit(scene::pcg::Graph iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_graph)
		return false;
	m_undo.push(mkUniq<PcgGraphCommand>(m_graph, iNext, iDescription, iMerge));
	m_graph = std::move(iNext);
	m_previewStale = true;
	rebuildCanvas();
	return true;
}

void PcgGraphDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_graph);
	m_previewStale = true;
	rebuildCanvas();
}

void PcgGraphDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_graph);
	m_previewStale = true;
	rebuildCanvas();
}

auto PcgGraphDocument::addNode(const std::string& iType, const std::array<float, 2> iPosition)
		-> std::optional<core::UUID> {
	const auto* type = scene::pcg::findNodeType(iType);
	if (type == nullptr) {
		OWL_WARN("PcgGraphDocument: Unknown node type '{}'.", iType)
		return std::nullopt;
	}
	auto next = m_graph;
	const core::UUID id;
	next.nodes.push_back({.id = id, .type = iType, .position = iPosition, .values = {}});
	std::ignore = commit(std::move(next), std::format("Add {}", type->title));
	m_selectedNode = id;
	return id;
}

auto PcgGraphDocument::connect(const core::UUID iFromNode, const std::string& iFromPin, const core::UUID iToNode,
							   const std::string& iToPin) -> bool {
	const auto* from = m_graph.findNode(iFromNode);
	const auto* to = m_graph.findNode(iToNode);
	if (from == nullptr || to == nullptr || iFromNode == iToNode)
		return false;
	const auto* fromType = scene::pcg::findNodeType(from->type);
	const auto* toType = scene::pcg::findNodeType(to->type);
	if (fromType == nullptr || toType == nullptr)
		return false;
	const auto output = std::ranges::find(fromType->outputs, iFromPin, &scene::pcg::PinSpec::name);
	const auto input = std::ranges::find(toType->inputs, iToPin, &scene::pcg::PinSpec::name);
	if (output == fromType->outputs.end() || input == toType->inputs.end() ||
		!scene::pcg::canConnect(output->type, input->type))
		return false;
	auto next = m_graph;
	std::erase_if(next.links, [&](const scene::pcg::GraphLink& iLink) -> bool {
		return iLink.toNode == iToNode && iLink.toPin == iToPin;
	});
	next.links.push_back({.fromNode = iFromNode, .fromPin = iFromPin, .toNode = iToNode, .toPin = iToPin});
	if (scene::pcg::evaluate(next, {.size = {1, 1}, .seed = 0}).error_or(scene::pcg::PcgError::EmptyRegion) ==
		scene::pcg::PcgError::Cycle) {
		OWL_WARN("PcgGraphDocument: The link would make a node depend on itself.")
		return false;
	}
	return commit(std::move(next), "Link pins");
}

void PcgGraphDocument::setNodeValue(const core::UUID iNode, const std::string& iKey, const std::string& iValue,
									const bool iMerge) {
	auto next = m_graph;
	auto* node = next.findNode(iNode);
	if (node == nullptr)
		return;
	node->values[iKey] = iValue;
	std::ignore = commit(std::move(next), std::format("Set {}", iKey), iMerge);
}

void PcgGraphDocument::deleteNodes(const std::span<const core::UUID> iNodes) {
	auto next = m_graph;
	for (const auto id: iNodes) next.removeNode(id);
	std::ignore =
			commit(std::move(next), iNodes.size() == 1 ? "Delete node" : std::format("Delete {} nodes", iNodes.size()));
}

void PcgGraphDocument::selectNode(const std::optional<core::UUID> iNode) {
	m_selectedNode = iNode && m_graph.findNode(*iNode) != nullptr ? iNode : std::nullopt;
	m_canvas.setSelection(m_selectedNode ? std::vector<core::UUID>{*m_selectedNode} : std::vector<core::UUID>{});
}

void PcgGraphDocument::setPreview(const math::vec2ui& iSize, const uint64_t iSeed) {
	const math::vec2ui size{std::clamp(iSize.x(), 1u, 256u), std::clamp(iSize.y(), 1u, 256u)};
	if (size == m_previewSize && iSeed == m_previewSeed)
		return;
	m_previewSize = size;
	m_previewSeed = iSeed;
	m_previewStale = true;
}

auto PcgGraphDocument::getPreview() -> const std::optional<scene::pcg::Result>& {
	if (m_previewStale) {
		auto result = scene::pcg::evaluate(m_graph, {.size = m_previewSize, .seed = m_previewSeed});
		m_preview = result ? std::optional{std::move(*result)} : std::nullopt;
		m_previewStale = false;
	}
	return m_preview;
}

auto PcgGraphDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!scene::pcg::saveToFile(m_graph, iPath))
		return false;
	m_path = iPath;
	m_savedText = scene::pcg::serialize(m_graph);
	return true;
}

auto PcgGraphDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto graph = scene::pcg::loadFromFile(iPath);
	if (!graph) {
		OWL_WARN("PcgGraphDocument: Cannot open '{}'.", iPath.string())
		return false;
	}
	m_graph = std::move(*graph);
	m_path = iPath;
	m_savedText = scene::pcg::serialize(m_graph);
	m_previewStale = true;
	rebuildCanvas();
	m_canvas.requestFitToContent();
	return true;
}

auto PcgGraphDocument::recoverySnapshot() const -> std::optional<std::string> { return scene::pcg::serialize(m_graph); }

auto PcgGraphDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto graph = scene::pcg::deserialize(iSnapshot, title());
	if (!graph) {
		OWL_WARN("Recovery: Cannot restore the autosave of graph '{}'.", title())
		return false;
	}
	m_graph = std::move(*graph);
	m_previewStale = true;
	rebuildCanvas();
	return true;
}

void PcgGraphDocument::onImGuiRender() {
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
	if (wantFocus) {
		ImGui::SetWindowFocus();
		if (mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
	}
	if (open) {
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (focused && mp_editorLayer != nullptr)
			mp_editorLayer->getDocumentManager().setActive(this);
		if (focused && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		m_linkDeleted = false;
		m_canvas.onRender();
		if (m_moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_moving = false;
			auto next = m_graph;
			for (const auto& canvasNode: m_canvas.nodes()) {
				if (auto* node = next.findNode(canvasNode.id); node != nullptr)
					node->position = {canvasNode.position.x(), canvasNode.position.y()};
			}
			std::ignore = commit(std::move(next), "Move nodes");
		}
		if (const auto selection = m_canvas.selection();
			focused && !m_linkDeleted && !selection.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			deleteNodes(selection);
		renderGraphMenu();
	}
	ImGui::End();
}

void PcgGraphDocument::renderGraphMenu() {
	if (m_openMenu) {
		ImGui::OpenPopup("##pcgGraphMenu");
		m_openMenu = false;
	}
	if (!ImGui::BeginPopup("##pcgGraphMenu"))
		return;
	if (m_menuNode) {
		if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete Node", .danger = true})) {
			const std::array nodes{*m_menuNode};
			deleteNodes(nodes);
		}
	} else {
		gui::widgets::menuHeader("Add Node", "Graph");
		renderPalette(m_menuPosition);
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::maximize2, .label = "Fit to Content"}))
			m_canvas.requestFitToContent();
	}
	ImGui::EndPopup();
}

void PcgGraphDocument::renderPalette(const std::array<float, 2> iPosition) {
	std::string category;
	for (const auto& type: scene::pcg::getNodeTypes()) {
		if (type.category != category) {
			category = type.category;
			ImGui::SeparatorText(category.c_str());
		}
		if (ImGui::Selectable(type.title.c_str()))
			std::ignore = addNode(type.id, iPosition);
		gui::widgets::itemTooltip(type.title.c_str(), nullptr, type.description.c_str());
	}
}

void PcgGraphDocument::renderHierarchyPanel() {
	if (gui::widgets::sectionHeader("Preview", gui::icon::eye, false).open) {
		if (gui::widgets::beginProperties("##pcgPreview")) {
			auto width = static_cast<int>(m_previewSize.x());
			auto height = static_cast<int>(m_previewSize.y());
			auto seed = static_cast<int>(m_previewSeed);
			bool changed = gui::widgets::propertyInt("Width", width, 48, 1, 256, "%d cells");
			changed = gui::widgets::propertyInt("Height", height, 32, 1, 256, "%d cells") || changed;
			changed = gui::widgets::propertyInt("Seed", seed, 1, 0, 1000000, "%d",
												"Seed of the preview; a tilemap keeps its own seed.") ||
					  changed;
			if (changed)
				setPreview({static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, static_cast<uint64_t>(seed));
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Add Node", gui::icon::plus, false).open) {
		const auto centre = m_canvas.getMouseCanvasPosition();
		renderPalette({centre.x(), centre.y()});
		gui::widgets::endSection();
	}
}

void PcgGraphDocument::renderPropertiesPanel() {
	const auto* node = m_selectedNode ? m_graph.findNode(*m_selectedNode) : nullptr;
	const auto* type = node != nullptr ? scene::pcg::findNodeType(node->type) : nullptr;
	if (node != nullptr && type != nullptr &&
		gui::widgets::sectionHeader(type->title.c_str(), gui::icon::slidersHorizontal, false).open) {
		ImGui::TextWrapped("%s", type->description.c_str());
		if (gui::widgets::beginProperties("##pcgNode")) {
			for (const auto& param: type->params) {
				const auto value = scene::pcg::Graph::getValue(*node, param.name);
				switch (param.type) {
					case scene::pcg::ParamType::Float:
						{
							float number = parseFloat(value);
							if (gui::widgets::propertyFloat(param.name.c_str(), number, parseFloat(param.defaultValue),
															0.01f, "%.3f", param.description.c_str()))
								setNodeValue(node->id, param.name,
											 std::format("{}", std::clamp(number, param.min, param.max)), true);
							break;
						}
					case scene::pcg::ParamType::Int:
						{
							int number = static_cast<int>(parseFloat(value));
							if (gui::widgets::propertyInt(param.name.c_str(), number,
														  static_cast<int>(parseFloat(param.defaultValue)),
														  static_cast<int>(param.min), static_cast<int>(param.max),
														  "%d", param.description.c_str()))
								setNodeValue(node->id, param.name, std::format("{}", number), true);
							break;
						}
					case scene::pcg::ParamType::Choice:
						{
							gui::widgets::propertyLabel(param.name.c_str(), param.description.c_str());
							if (gui::widgets::beginCombo(std::format("##{}", param.name).c_str(), value.c_str())) {
								for (const auto& choice: param.choices) {
									if (ImGui::Selectable(choice.c_str(), choice == value))
										setNodeValue(node->id, param.name, choice);
								}
								ImGui::EndCombo();
							}
							if (gui::widgets::propertyReset(value != param.defaultValue))
								setNodeValue(node->id, param.name, param.defaultValue);
							break;
						}
				}
				// An edit replaces the graph: the node is looked up again before the next parameter.
				node = m_graph.findNode(*m_selectedNode);
				if (node == nullptr)
					break;
			}
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Output", gui::icon::image, false).open) {
		renderPreview();
		gui::widgets::endSection();
	}
}

void PcgGraphDocument::renderPreview() {
	const auto& result = getPreview();
	if (!result) {
		std::ignore = gui::widgets::emptyState(gui::icon::triangleAlert, "No preview",
											   "A node of the graph depends on itself: remove a link of the loop.");
		return;
	}
	for (const auto& warning: result->warnings)
		ImGui::TextColored(ImVec4{1.f, 0.75f, 0.3f, 1.f}, "%s", warning.c_str());
	const float available = std::max(32.f, ImGui::GetContentRegionAvail().x);
	const float cell = std::clamp(available / static_cast<float>(result->size.x()), 1.f, 8.f);
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 extent{cell * static_cast<float>(result->size.x()), cell * static_cast<float>(result->size.y())};
	ImGui::Dummy(extent);
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(origin, {origin.x + extent.x, origin.y + extent.y}, IM_COL32(30, 30, 36, 255));
	const auto rect = [&](const uint32_t iX, const uint32_t iY, const ImU32 iColor) -> void {
		const ImVec2 min{origin.x + static_cast<float>(iX) * cell, origin.y + static_cast<float>(iY) * cell};
		draw->AddRectFilled(min, {min.x + cell, min.y + cell}, iColor);
	};
	const scene::pcg::PinValue* shown = nullptr;
	if (m_selectedNode) {
		if (const auto values = result->values.find(*m_selectedNode);
			values != result->values.end() && !values->second.empty())
			shown = &values->second.begin()->second;
	}
	if (const auto* field = shown != nullptr ? std::get_if<scene::pcg::Field>(shown) : nullptr; field != nullptr) {
		for (uint32_t y = 0; y < field->size.y(); ++y) {
			for (uint32_t x = 0; x < field->size.x(); ++x) {
				const auto grey = static_cast<uint8_t>(std::clamp(field->get(x, y), 0.f, 1.f) * 255.f);
				rect(x, y, IM_COL32(grey, grey, grey, 255));
			}
		}
		ImGui::TextDisabled("Field of the selected node: black 0, white 1.");
		return;
	}
	if (const auto* points = shown != nullptr ? std::get_if<scene::pcg::Points>(shown) : nullptr; points != nullptr) {
		for (const auto& point: points->cells) rect(point.x(), point.y(), IM_COL32(255, 255, 255, 255));
		ImGui::TextDisabled("%zu points of the selected node.", points->cells.size());
		return;
	}
	std::set<int32_t> tiles;
	for (uint32_t y = 0; y < result->size.y(); ++y) {
		for (uint32_t x = 0; x < result->size.x(); ++x) {
			const auto tile = result->tiles[static_cast<size_t>(y) * result->size.x() + x];
			if (tile == scene::pcg::g_UntouchedTile)
				continue;
			rect(x, y, tile < 0 ? IM_COL32(0, 0, 0, 255) : tileColor(tile));
			tiles.insert(tile);
		}
	}
	ImGui::TextDisabled("Tiles written by the graph (%zu kinds); select a node to see its output.", tiles.size());
	for (const auto tile: tiles) {
		ImGui::ColorButton(std::format("##tile{}", tile).c_str(),
						   ImGui::ColorConvertU32ToFloat4(tile < 0 ? IM_COL32(0, 0, 0, 255) : tileColor(tile)),
						   ImGuiColorEditFlags_NoTooltip, {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()});
		ImGui::SameLine();
		ImGui::Text(tile < 0 ? "erased" : "tile %d", tile);
	}
}

}// namespace owl::nest
