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
#include <gui/Theme.h>
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
#include <gui/widgets/TreeRow.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <format>
#include <map>
#include <ranges>
#include <set>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest {

namespace {

constexpr uint32_t g_fitFrames = 3;
// Drag-and-drop payload of a node type dragged from the library onto the canvas.
constexpr auto g_PcgNodePayload = "OWL_PCG_NODE";

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

constexpr std::array<uint8_t, 4> g_emptyCell{30, 30, 36, 255};
constexpr std::array<uint8_t, 4> g_erasedCell{0, 0, 0, 255};

// A colour per tile index, stable between frames; black for an erased cell.
auto tileColor(const int32_t iTile) -> std::array<uint8_t, 4> {
	if (iTile < 0)
		return g_erasedCell;
	const auto hash = static_cast<uint32_t>(iTile) * 2654435761u;
	const float hue = static_cast<float>(hash % 360u) / 360.f;
	float red = 0.f;
	float green = 0.f;
	float blue = 0.f;
	ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 0.85f, red, green, blue);
	return {static_cast<uint8_t>(red * 255.f), static_cast<uint8_t>(green * 255.f), static_cast<uint8_t>(blue * 255.f),
			255};
}

// Duotone IconBank icon of the section of a node category.
auto categoryIcon(const std::string& iCategory) -> const char* {
	if (iCategory == "Sources")
		return "vs_node_constant";
	if (iCategory == "Points")
		return "vs_node_pins";
	if (iCategory == "Outputs")
		return "vs_node_flow";
	return "vs_node_math";
}

// Inspector label of a parameter: its name in Title Case (`spacing` -> `Spacing`).
auto toLabel(const std::string& iName) -> std::string {
	auto label = iName;
	if (!label.empty())
		label.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(label.front())));
	return label;
}

auto parseFloat(const std::string& iText) -> float {
	float value = 0.f;
	std::ignore = std::from_chars(iText.data(), iText.data() + iText.size(), value);
	return value;
}

}// namespace

auto getPcgNodeGlyph(const std::string_view iType) -> const char* {
	static const std::map<std::string_view, const char*> glyphs{{"source.noise", gui::icon::audioLines},
																{"source.random", gui::icon::binary},
																{"source.constant", gui::icon::hash},
																{"source.gradient", gui::icon::sun},
																{"field.threshold", gui::icon::slidersVertical},
																{"field.combine", gui::icon::merge},
																{"field.invert", gui::icon::split},
																{"field.blur", gui::icon::focus},
																{"field.cellular", gui::icon::grid3x3},
																{"field.edge", gui::icon::squareDashed},
																{"points.scatter", gui::icon::sparkles},
																{"points.filter", gui::icon::listFilter},
																{"points.to_mask", gui::icon::scan},
																{"output.tiles", gui::icon::paintbrush},
																{"output.points", gui::icon::flag},
																{"output.erase", gui::icon::eraser}};
	const auto found = glyphs.find(iType);
	return found == glyphs.end() ? gui::icon::circleHelp : found->second;
}

auto buildPcgPreviewImage(const scene::pcg::Result& iResult, const std::optional<core::UUID> iNode)
		-> std::vector<uint8_t> {
	std::vector<uint8_t> pixels;
	pixels.reserve(static_cast<size_t>(iResult.size.surface()) * 4);
	for (uint32_t cell = 0; cell < iResult.size.surface(); ++cell)
		pixels.insert(pixels.end(), g_emptyCell.begin(), g_emptyCell.end());
	const auto paint = [&](const uint32_t iX, const uint32_t iY, const std::array<uint8_t, 4>& iColor) -> void {
		if (iX < iResult.size.x() && iY < iResult.size.y())
			std::ranges::copy(iColor, pixels.begin() + static_cast<std::ptrdiff_t>(
															   (static_cast<size_t>(iY) * iResult.size.x() + iX) * 4));
	};
	const scene::pcg::PinValue* shown = nullptr;
	if (iNode) {
		if (const auto values = iResult.values.find(*iNode); values != iResult.values.end() && !values->second.empty())
			shown = &values->second.begin()->second;
	}
	if (const auto* field = shown != nullptr ? std::get_if<scene::pcg::Field>(shown) : nullptr; field != nullptr) {
		for (uint32_t y = 0; y < field->size.y(); ++y) {
			for (uint32_t x = 0; x < field->size.x(); ++x) {
				const auto grey = static_cast<uint8_t>(std::clamp(field->get(x, y), 0.f, 1.f) * 255.f);
				paint(x, y, {grey, grey, grey, 255});
			}
		}
		return pixels;
	}
	if (const auto* points = shown != nullptr ? std::get_if<scene::pcg::Points>(shown) : nullptr; points != nullptr) {
		for (const auto& point: points->cells) paint(point.x(), point.y(), {255, 255, 255, 255});
		return pixels;
	}
	for (uint32_t y = 0; y < iResult.size.y(); ++y) {
		for (uint32_t x = 0; x < iResult.size.x(); ++x) {
			if (const auto tile = iResult.tiles[static_cast<size_t>(y) * iResult.size.x() + x];
				tile != scene::pcg::g_UntouchedTile)
				paint(x, y, tileColor(tile));
		}
	}
	return pixels;
}

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
	graph.links.push_back(
			{.fromNode = noise, .fromPin = "Field", .toNode = threshold, .toPin = "Field", .waypoints = {}});
	graph.links.push_back(
			{.fromNode = threshold, .fromPin = "Mask", .toNode = output, .toPin = "Mask", .waypoints = {}});
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
	m_canvas.setOnLinkWaypointsChanged([this](core::UUID, const std::vector<math::vec2f>&) -> void {
		m_linkDeleted = true;
		if (!m_moving)
			std::ignore = commit(getCanvasLayout(), "Edit link waypoints");
	});
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
		gui::widgets::Node canvasNode{
				.id = node.id,
				.title = type != nullptr ? std::format("{}  {}", getPcgNodeGlyph(node.type), type->title)
										 : std::format("{}  {}", gui::icon::circleHelp, node.type),
				.position = {node.position[0], node.position[1]},
				.inputs = {},
				.outputs = {},
				.customData = {},
				.titleColor = type != nullptr ? categoryColor(type->category) : math::vec4{1.f, 0.3f, 0.3f, 1.f}};
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
		if (from == m_pinIds.end() || to == m_pinIds.end())
			continue;
		std::vector<math::vec2f> waypoints;
		waypoints.reserve(link.waypoints.size());
		for (const auto& [x, y]: link.waypoints) waypoints.emplace_back(x, y);
		std::ignore = m_canvas.addLink(from->second, to->second, std::move(waypoints));
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

auto PcgGraphDocument::getCanvasLayout() const -> scene::pcg::Graph {
	auto next = m_graph;
	for (const auto& canvasNode: m_canvas.nodes())
		if (auto* node = next.findNode(canvasNode.id); node != nullptr)
			node->position = {canvasNode.position.x(), canvasNode.position.y()};
	for (auto& link: next.links) {
		const auto from = m_pinIds.find({link.fromNode, true, link.fromPin});
		const auto to = m_pinIds.find({link.toNode, false, link.toPin});
		if (from == m_pinIds.end() || to == m_pinIds.end())
			continue;
		for (const auto& canvasLink: m_canvas.links())
			if (canvasLink.fromPin == from->second && canvasLink.toPin == to->second) {
				link.waypoints.clear();
				for (const auto& point: canvasLink.waypoints) link.waypoints.push_back({point.x(), point.y()});
			}
	}
	return next;
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
	next.links.push_back(
			{.fromNode = iFromNode, .fromPin = iFromPin, .toNode = iToNode, .toPin = iToPin, .waypoints = {}});
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
		++m_previewGeneration;
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
	m_fitFrames = g_fitFrames;
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
		renderToolbar();
		if (m_viewMode == PcgViewMode::Output) {
			renderPreview(true);
			ImGui::End();
			return;
		}
		if (m_viewMode == PcgViewMode::Split) {
			const float width = std::max(ImGui::GetContentRegionAvail().x * 0.6f, 200.f);
			ImGui::BeginChild("##pcgGraphPane", {width, 0.f}, ImGuiChildFlags_ResizeX);
		}
		m_linkDeleted = false;
		// The dock gives the canvas its final size over the first frames: the framing is asked again on each of them.
		if (m_fitFrames > 0) {
			m_canvas.requestFitToContent();
			--m_fitFrames;
		}
		m_canvas.onRender();
		if (ImGui::BeginDragDropTargetCustom({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()},
											 ImGui::GetID("##pcgDrop"))) {
			if (const auto* payload = ImGui::AcceptDragDropPayload(g_PcgNodePayload);
				payload != nullptr && payload->DataSize > 0) {
				const auto mouse = m_canvas.getMouseCanvasPosition();
				std::ignore = addNode(
						std::string{static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize)},
						{mouse.x(), mouse.y()});
			}
			ImGui::EndDragDropTarget();
		}
		if (m_moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_moving = false;
			std::ignore = commit(getCanvasLayout(), "Move nodes");
		}
		if (const auto selection = m_canvas.selection(); focused && !m_linkDeleted && !selection.empty() &&
														 !ImGui::GetIO().WantTextInput &&
														 ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			deleteNodes(selection);
		if (focused && ImGui::IsKeyPressed(ImGuiKey_Home, false))
			m_canvas.requestFitToContent();
		renderGraphMenu();
		if (m_viewMode == PcgViewMode::Split) {
			ImGui::EndChild();
			ImGui::SameLine();
			if (ImGui::BeginChild("##pcgPreviewPane"))
				renderPreview(true);
			ImGui::EndChild();
		}
	}
	ImGui::End();
}

void PcgGraphDocument::renderToolbar() {
	const auto separator = []() -> void {
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
	};
	if (gui::widgets::iconButton("##pcgFrame", {.glyph = gui::icon::focus,
												.tooltip = "Frame All",
												.shortcut = "Home",
												.description = "Show every node.",
												.disabled = m_viewMode == PcgViewMode::Output}))
		m_canvas.requestFitToContent();
	separator();
	constexpr std::array modes{
			std::tuple{PcgViewMode::Graph, gui::icon::workflow, "Graph", "The nodes only."},
			std::tuple{PcgViewMode::Split, gui::icon::columns3, "Split",
					   "The nodes and the tiles the whole graph writes side by side."},
			std::tuple{PcgViewMode::Output, gui::icon::image, "Output", "The tiles the whole graph writes, alone."}};
	for (const auto& [mode, glyph, label, tip]: modes) {
		ImGui::PushID(label);
		if (gui::widgets::iconButton(
					"##mode", {.glyph = glyph, .tooltip = label, .description = tip, .selected = m_viewMode == mode}))
			m_viewMode = mode;
		ImGui::PopID();
		ImGui::SameLine(0.f, 2.f);
	}
	separator();
	const auto& preview = getPreview();
	if (!preview) {
		gui::widgets::badge(std::format("{} A node depends on itself", gui::icon::triangleAlert).c_str(),
							gui::widgets::BadgeKind::Error);
		return;
	}
	if (preview->warnings.empty()) {
		gui::widgets::badge(
				std::format("{} Generates {}x{}", gui::icon::circleCheck, preview->size.x(), preview->size.y()).c_str(),
				gui::widgets::BadgeKind::Success);
		return;
	}
	if (gui::widgets::button("##pcgWarnings", {.glyph = gui::icon::triangleAlert,
											   .label = std::format("{} warning{}", preview->warnings.size(),
																	preview->warnings.size() > 1 ? "s" : "")
																.c_str(),
											   .tooltip = "Evaluation warnings",
											   .description = "Click to list them."}))
		ImGui::OpenPopup("##pcgWarningList");
	if (ImGui::BeginPopup("##pcgWarningList")) {
		gui::widgets::menuHeader("Evaluation warnings");
		for (const auto& warning: preview->warnings)
			std::ignore = gui::widgets::menuItem({.glyph = gui::icon::triangleAlert, .label = warning.c_str()});
		ImGui::EndPopup();
	}
}

void PcgGraphDocument::renderGraphMenu() {
	if (m_openMenu) {
		ImGui::OpenPopup("##pcgGraphMenu");
		m_paletteFilter.clear();
		m_openMenu = false;
	}
	if (!ImGui::BeginPopup("##pcgGraphMenu"))
		return;
	if (m_menuNode) {
		const auto* node = m_graph.findNode(*m_menuNode);
		const auto* type = node != nullptr ? scene::pcg::findNodeType(node->type) : nullptr;
		gui::widgets::menuHeader(type != nullptr ? type->title.c_str() : "Node", "Node");
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::trash2, .label = "Delete Node", .shortcut = "Del", .danger = true})) {
			const std::array nodes{*m_menuNode};
			deleteNodes(nodes);
		}
	} else {
		gui::widgets::menuHeader("Add Node", "Palette");
		renderPalette(m_menuPosition, true);
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Frame All", .shortcut = "Home"}))
			m_canvas.requestFitToContent();
	}
	ImGui::EndPopup();
}

void PcgGraphDocument::renderPalette(const std::array<float, 2> iPosition, const bool iPopup) {
	if (iPopup && ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	std::ignore = gui::widgets::searchField(iPopup ? "##pcgPaletteSearch" : "##pcgLibrarySearch", m_paletteFilter,
											"Search nodes");
	const auto item = [&](const scene::pcg::NodeType& iType) -> void {
		ImGui::PushID(iType.id.c_str());
		const auto row = gui::widgets::treeRow(
				"##type", {.label = iType.title.c_str(),
						   .glyph = getPcgNodeGlyph(iType.id),
						   .glyphColor = gui::Theme::mix(categoryColor(iType.category), {1.f, 1.f, 1.f, 1.f}, 0.45f)});
		if (row.clicked) {
			std::ignore = addNode(iType.id, iPosition);
			if (iPopup)
				ImGui::CloseCurrentPopup();
		}
		if (!iPopup && ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(g_PcgNodePayload, iType.id.data(), iType.id.size());
			ImGui::Text("%s %s", getPcgNodeGlyph(iType.id), iType.title.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s", iType.title.c_str(), iType.description.c_str());
		if (row.open)
			gui::widgets::endTreeRow();
		ImGui::PopID();
	};
	const auto& types = scene::pcg::getNodeTypes();
	if (!m_paletteFilter.empty()) {
		for (const auto& type: types) {
			if (gui::widgets::matchesFilter(type.title, m_paletteFilter) ||
				gui::widgets::matchesFilter(type.description, m_paletteFilter))
				item(type);
		}
		return;
	}
	std::vector<std::string> categories;
	for (const auto& type: types) {
		if (!std::ranges::contains(categories, type.category))
			categories.push_back(type.category);
	}
	for (const auto& category: categories) {
		ImGui::PushID(category.c_str());
		const auto row = gui::widgets::treeRow(
				"##category", {.label = category.c_str(), .iconName = categoryIcon(category), .hasChildren = true});
		if (row.open) {
			for (const auto& type: types) {
				if (type.category == category)
					item(type);
			}
			gui::widgets::endTreeRow();
		}
		ImGui::PopID();
	}
}

void PcgGraphDocument::renderHierarchyPanel() {
	if (gui::widgets::sectionHeader("Preview", "preview", false).open) {
		if (gui::widgets::beginProperties("##pcgPreview")) {
			auto width = static_cast<int>(m_previewSize.x());
			auto height = static_cast<int>(m_previewSize.y());
			auto seed = static_cast<int>(m_previewSeed);
			bool changed =
					gui::widgets::propertyInt("Width", width, 48, 1, 256, "%d cells", "Width of the preview region.");
			changed = gui::widgets::propertyInt("Height", height, 32, 1, 256, "%d cells",
												"Height of the preview region.") ||
					  changed;
			changed = gui::widgets::propertyInt("Seed", seed, 1, 0, 1000000, "%d",
												"Seed of the preview; a tilemap keeps its own seed.") ||
					  changed;
			if (changed)
				setPreview({static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, static_cast<uint64_t>(seed));
			gui::widgets::endProperties();
		}
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Node Library", "vs_node_inputs", false).open) {
		const auto centre = m_canvas.getMouseCanvasPosition();
		renderPalette({centre.x(), centre.y()}, false);
		gui::widgets::endSection();
	}
}

void PcgGraphDocument::renderPropertiesPanel() {
	const auto* node = m_selectedNode ? m_graph.findNode(*m_selectedNode) : nullptr;
	const auto* type = node != nullptr ? scene::pcg::findNodeType(node->type) : nullptr;
	if (node == nullptr || type == nullptr) {
		std::ignore = gui::widgets::emptyState(gui::icon::workflow, "No node selected",
											   "Click a node of the graph to edit its parameters.");
		return;
	}
	if (gui::widgets::sectionHeader(type->title.c_str(), categoryIcon(type->category), false).open)
		renderNodeParameters();
	if (gui::widgets::sectionHeader("Output", "preview", false).open) {
		renderPreview(false);
		gui::widgets::endSection();
	}
}

void PcgGraphDocument::renderNodeParameters() {
	const auto* node = m_graph.findNode(*m_selectedNode);
	const auto* type = scene::pcg::findNodeType(node->type);
	ImGui::TextWrapped("%s", type->description.c_str());
	if (type->params.empty())
		ImGui::TextDisabled("This node has no parameter.");
	else if (gui::widgets::beginProperties("##pcgNode")) {
		for (const auto& param: type->params) {
			const auto value = scene::pcg::Graph::getValue(*node, param.name);
			switch (param.type) {
				case scene::pcg::ParamType::Float:
					{
						float number = parseFloat(value);
						if (gui::widgets::propertyFloat(toLabel(param.name).c_str(), number,
														parseFloat(param.defaultValue), 0.01f, "%.3f",
														param.description.c_str()))
							setNodeValue(node->id, param.name,
										 std::format("{}", std::clamp(number, param.min, param.max)), true);
						break;
					}
				case scene::pcg::ParamType::Int:
					{
						int number = static_cast<int>(parseFloat(value));
						if (gui::widgets::propertyInt(toLabel(param.name).c_str(), number,
													  static_cast<int>(parseFloat(param.defaultValue)),
													  static_cast<int>(param.min), static_cast<int>(param.max), "%d",
													  param.description.c_str()))
							setNodeValue(node->id, param.name, std::format("{}", number), true);
						break;
					}
				case scene::pcg::ParamType::Choice:
					{
						gui::widgets::propertyLabel(toLabel(param.name).c_str(), param.description.c_str());
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

void PcgGraphDocument::renderPreview(const bool iTiles) {
	const auto& result = getPreview();
	if (!result) {
		std::ignore = gui::widgets::emptyState(gui::icon::triangleAlert, "No preview",
											   "A node of the graph depends on itself: remove a link of the loop.");
		return;
	}
	const auto shown = iTiles ? std::nullopt : m_selectedNode;
	auto& image = iTiles ? m_tilesImage : m_nodeImage;
	const std::pair key{m_previewGeneration, shown};
	if (!image.texture || key != image.key) {
		auto pixels = buildPcgPreviewImage(*result, shown);
		if (!image.texture || image.texture->getSpecification().size != result->size) {
			if (image.texture)
				gui::UiLayer::deferTextureRelease(image.texture);
			image.texture = renderer::gpu::Texture2D::create(
					renderer::gpu::Texture::Specification{.size = result->size,
														  .format = renderer::gpu::ImageFormat::Rgba8});
			if (image.texture)
				image.texture->setFilterMode(renderer::gpu::FilterMode::Nearest);
		}
		if (image.texture)
			image.texture->setData(pixels.data(), static_cast<uint32_t>(pixels.size()));
		image.key = key;
	}
	const auto* node = shown ? m_graph.findNode(*shown) : nullptr;
	const auto* type = node != nullptr ? scene::pcg::findNodeType(node->type) : nullptr;
	if (type == nullptr) {
		ImGui::Text("%s  Tiles of the graph", gui::icon::image);
		ImGui::TextDisabled("(one colour per tile, black erased)");
	} else {
		ImGui::Text("%s  %s", getPcgNodeGlyph(type->id), type->title.c_str());
		if (type->outputs.empty())
			ImGui::TextDisabled("(the mask it writes)");
		else
			ImGui::TextDisabled(type->outputs.front().type == scene::pcg::PinType::Points ? "(points in white)"
																						  : "(black 0, white 1)");
	}
	if (!image.texture)
		return;
	const auto available = ImGui::GetContentRegionAvail();
	const float scale = std::max(0.1f, std::min(available.x / static_cast<float>(result->size.x()),
												available.y / static_cast<float>(result->size.y())));
	const ImVec2 extent{scale * static_cast<float>(result->size.x()), scale * static_cast<float>(result->size.y())};
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (available.x - extent.x) * 0.5f));
	ImGui::Image(static_cast<ImTextureID>(image.texture->getRendererId()), extent);
}

}// namespace owl::nest
