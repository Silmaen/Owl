/**
 * @file BehaviourTreeDocument.cpp
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "BehaviourTreeDocument.h"

#include "EditorLayer.h"
#include "SceneDocument.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/Combo.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SearchField.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/TreeRow.h>
#include <scene/Scene.h>
#include <scene/component/components.h>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wreserved-identifier")
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
OWL_DIAG_POP

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace owl::nest {

namespace {

using scene::ai::NodeKind;
using scene::ai::NodeStatus;

constexpr uint32_t g_fitFrames = 3;
// Height of the footer of a node, canvas units: its summary and, in Play, its status.
constexpr float g_footerHeight = 20.f;
// Drag-and-drop payload of a node type dragged from the library onto the canvas.
constexpr auto g_TreeNodePayload = "OWL_BT_NODE";

class TreeCommand final : public UndoCommand<scene::ai::BehaviourTree> {
public:
	TreeCommand(scene::ai::BehaviourTree iBefore, scene::ai::BehaviourTree iAfter, std::string iDescription,
				const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(scene::ai::BehaviourTree& ioTarget) override { ioTarget = m_before; }

	void redo(scene::ai::BehaviourTree& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return typeid(TreeCommand).hash_code(); }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto* other = dynamic_cast<const TreeCommand*>(&iOther);
		if (!m_merge || other == nullptr || !other->m_merge || other->m_description != m_description)
			return false;
		m_after = other->m_after;
		return true;
	}

private:
	scene::ai::BehaviourTree m_before;
	scene::ai::BehaviourTree m_after;
	std::string m_description;
	bool m_merge;
};

auto kindColor(const NodeKind iKind) -> math::vec4 {
	switch (iKind) {
		case NodeKind::Root:
			return {0.85f, 0.85f, 0.9f, 1.f};
		case NodeKind::Composite:
			return {0.55f, 0.75f, 1.f, 1.f};
		case NodeKind::Decorator:
			return {0.8f, 0.6f, 1.f, 1.f};
		case NodeKind::Condition:
			return {1.f, 0.8f, 0.4f, 1.f};
		case NodeKind::Action:
			return {0.5f, 0.9f, 0.6f, 1.f};
	}
	return {1.f, 1.f, 1.f, 1.f};
}

auto kindLabel(const NodeKind iKind) -> const char* {
	switch (iKind) {
		case NodeKind::Root:
			return "Root";
		case NodeKind::Composite:
			return "Composites";
		case NodeKind::Decorator:
			return "Decorators";
		case NodeKind::Condition:
			return "Conditions";
		case NodeKind::Action:
			return "Actions";
	}
	return "";
}

auto kindIcon(const NodeKind iKind) -> const char* {
	switch (iKind) {
		case NodeKind::Root:
		case NodeKind::Composite:
			return "vs_node_flow";
		case NodeKind::Decorator:
			return "vs_node_math";
		case NodeKind::Condition:
			return "vs_node_pins";
		case NodeKind::Action:
			return "vs_node_constant";
	}
	return "vs_node_flow";
}

auto statusColor(const NodeStatus iStatus) -> ImU32 {
	switch (iStatus) {
		case NodeStatus::Running:
			return IM_COL32(250, 200, 60, 255);
		case NodeStatus::Success:
			return IM_COL32(90, 210, 120, 255);
		case NodeStatus::Failure:
			return IM_COL32(235, 85, 85, 255);
		case NodeStatus::Idle:
			break;
	}
	return IM_COL32(90, 90, 100, 255);
}

auto statusName(const NodeStatus iStatus) -> const char* {
	switch (iStatus) {
		case NodeStatus::Running:
			return "Running";
		case NodeStatus::Success:
			return "Success";
		case NodeStatus::Failure:
			return "Failure";
		case NodeStatus::Idle:
			break;
	}
	return "Idle";
}

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

auto idText(const core::UUID iId) -> std::string { return std::format("{}", static_cast<uint64_t>(iId)); }

auto idsText(const std::span<const core::UUID> iIds) -> std::string {
	std::string text;
	for (const auto id: iIds) text += std::format("{}{}", text.empty() ? "" : ",", static_cast<uint64_t>(id));
	return text;
}

// Whether a tree path written in a component names the file of the document.
auto namesFile(const std::string& iAsset, const std::filesystem::path& iFile) -> bool {
	if (iAsset.empty())
		return false;
	const auto file = iFile.lexically_normal().generic_string();
	const auto asset = std::filesystem::path{iAsset}.lexically_normal().generic_string();
	return file == asset || (file.ends_with(asset) && file[file.size() - asset.size() - 1] == '/');
}

// The field editing one parameter of a node: `iSet` sends the new value (merged while typing or dragging).
void parameterField(const scene::ai::ParamSpec& iParam, const std::string& iValue, const std::string& iLabel,
					const scene::ai::BehaviourTree& iTree, const std::function<void(const std::string&, bool)>& iSet) {
	switch (iParam.type) {
		case scene::ai::ParamType::Float:
			{
				float number = parseFloat(iValue);
				if (gui::widgets::propertyFloat(iLabel.c_str(), number, parseFloat(iParam.defaultValue), 0.01f, "%.3f",
												iParam.description.c_str()))
					iSet(std::format("{}", std::clamp(number, iParam.min, iParam.max)), true);
				break;
			}
		case scene::ai::ParamType::Int:
			{
				int number = static_cast<int>(parseFloat(iValue));
				if (gui::widgets::propertyInt(iLabel.c_str(), number, static_cast<int>(parseFloat(iParam.defaultValue)),
											  static_cast<int>(iParam.min), static_cast<int>(iParam.max), "%d",
											  iParam.description.c_str()))
					iSet(std::format("{}", number), true);
				break;
			}
		case scene::ai::ParamType::Bool:
			{
				bool flag = iValue == "true";
				if (gui::widgets::propertyBool(iLabel.c_str(), flag, iParam.defaultValue == "true",
											   iParam.description.c_str()))
					iSet(flag ? "true" : "false", false);
				break;
			}
		case scene::ai::ParamType::Text:
			{
				std::string text = iValue;
				if (gui::widgets::propertyText(iLabel.c_str(), text, nullptr, iParam.description.c_str()))
					iSet(text, true);
				break;
			}
		case scene::ai::ParamType::Choice:
		case scene::ai::ParamType::Key:
			{
				gui::widgets::propertyLabel(iLabel.c_str(), iParam.description.c_str());
				const bool isKey = iParam.type == scene::ai::ParamType::Key;
				const char* shown = iValue.empty() ? "(none)" : iValue.c_str();
				if (gui::widgets::beginCombo(std::format("##{}", iParam.name).c_str(), shown)) {
					if (isKey) {
						if (iParam.name != "key" && ImGui::Selectable("(none)", iValue.empty()))
							iSet("", false);
						for (const auto& key: iTree.blackboard)
							if (ImGui::Selectable(key.name.c_str(), key.name == iValue))
								iSet(key.name, false);
					} else {
						for (const auto& choice: iParam.choices)
							if (ImGui::Selectable(choice.c_str(), choice == iValue))
								iSet(choice, false);
					}
					ImGui::EndCombo();
				}
				if (gui::widgets::propertyReset(iValue != iParam.defaultValue))
					iSet(iParam.defaultValue, false);
				break;
			}
	}
}

}// namespace

auto getBehaviourNodeGlyph(const std::string_view iType) -> const char* {
	static const std::map<std::string_view, const char*> glyphs{{"root", gui::icon::flag},
																{"composite.sequence", gui::icon::listOrdered},
																{"composite.selector", gui::icon::gitBranch},
																{"composite.parallel", gui::icon::columns3},
																{"composite.random", gui::icon::split},
																{"decorator.inverter", gui::icon::refreshCw},
																{"decorator.succeeder", gui::icon::circleCheck},
																{"decorator.repeat", gui::icon::repeat},
																{"decorator.retry", gui::icon::rotateCcw},
																{"decorator.cooldown", gui::icon::history},
																{"decorator.timeout", gui::icon::timer},
																{"decorator.guard", gui::icon::lock},
																{"condition.compare", gui::icon::equal},
																{"condition.distance", gui::icon::move},
																{"condition.chance", gui::icon::sigma},
																{"condition.lua", gui::icon::fileCode2},
																{"action.wait", gui::icon::timer},
																{"action.set", gui::icon::pencil},
																{"action.move_to", gui::icon::route},
																{"action.steer", gui::icon::rabbit},
																{"action.find_player", gui::icon::userRound},
																{"action.lua", gui::icon::fileCode2},
																{"action.log", gui::icon::scrollText}};
	const auto found = glyphs.find(iType);
	return found == glyphs.end() ? gui::icon::circleHelp : found->second;
}

auto getBehaviourNodeSummary(const scene::ai::TreeNode& iNode) -> std::string {
	const auto value = [&iNode](const std::string_view iParam) -> std::string {
		return scene::ai::BehaviourTree::getValue(iNode, iParam);
	};
	const std::string_view type = iNode.type;
	if (type == "composite.selector")
		return value("reactive") == "true" ? "reactive" : "";
	if (type == "composite.parallel")
		return std::format("succeed: {}", value("policy"));
	if (type == "decorator.repeat" || type == "decorator.retry")
		return value("count") == "0" ? "forever" : std::format("{} times", value("count"));
	if (type == "decorator.cooldown" || type == "decorator.timeout" || type == "action.wait")
		return std::format("{} s", value("seconds"));
	if (type == "decorator.guard" || type == "condition.compare") {
		const auto op = value("op");
		if (op == "is set" || op == "is not set")
			return std::format("{} {}", value("key"), op);
		return std::format("{} {} {}", value("key"), op, value("value"));
	}
	if (type == "condition.distance")
		return std::format("{} {} {}{}", value("key"), value("op"), value("distance"),
						   value("space") == "3D" ? "" : std::format(" ({})", value("space")));
	if (type == "condition.chance")
		return std::format("{} %", parseFloat(value("probability")) * 100.f);
	if (type == "condition.lua" || type == "action.lua")
		return std::format("{}()", value("function"));
	if (type == "action.set")
		return std::format("{} = {}", value("key"), value("value"));
	if (type == "action.move_to" || type == "action.find_player")
		return value("key");
	if (type == "action.steer")
		return value("key").empty() ? value("behaviour") : std::format("{} {}", value("behaviour"), value("key"));
	if (type == "action.log")
		return value("message");
	return {};
}

BehaviourTreeDocument::BehaviourTreeDocument() = default;

BehaviourTreeDocument::~BehaviourTreeDocument() = default;

auto BehaviourTreeDocument::isDirty() const -> bool { return scene::ai::serialize(m_tree) != m_savedText; }

void BehaviourTreeDocument::onAttach(EditorLayer* iEditor) {
	NodeGraphDocument::onAttach(iEditor);
	m_savedText = scene::ai::serialize(m_tree);
	rebuildCanvas();
}

void BehaviourTreeDocument::onCanvasReady() {
	m_canvas.setLinkColorsEnabled(false);
	m_canvas.setLinkValidator([this](const core::UUID iFrom, const core::UUID iTo) -> bool {
		const auto from = m_pins.find(iFrom);
		const auto to = m_pins.find(iTo);
		return from != m_pins.end() && to != m_pins.end() && from->second.children && !to->second.children &&
			   from->second.node != to->second.node;
	});
	m_canvas.setOnLinkCreated([this](core::UUID, const core::UUID iFrom, const core::UUID iTo) -> void {
		const auto& from = m_pins.at(iFrom);
		const auto& to = m_pins.at(iTo);
		if (!sendCommand("bt.link", {{"parent", idText(from.node)}, {"child", idText(to.node)}}))
			rebuildCanvas();
	});
	m_canvas.setOnLinkDeleted([this](core::UUID) -> void {
		m_linkDeleted = true;
		for (const auto& link: m_tree.links) {
			const auto from = m_childPins.find(link.parent);
			const auto to = m_parentPins.find(link.child);
			const bool kept = from != m_childPins.end() && to != m_parentPins.end() &&
							  std::ranges::any_of(m_canvas.links(), [&](const gui::widgets::Link& iCanvasLink) -> bool {
								  return iCanvasLink.fromPin == from->second && iCanvasLink.toPin == to->second;
							  });
			if (!kept) {
				std::ignore = sendCommand("bt.unlink", {{"child", idText(link.child)}});
				return;
			}
		}
	});
	m_canvas.setOnNodeMoved([this](core::UUID, math::vec2f) -> void { m_moving = true; });
	m_canvas.setOnNodeSelected([this](const core::UUID iNode) -> void { m_selectedNode = iNode; });
	m_canvas.setOnContextMenu([this](const std::optional<core::UUID> iNode) -> void {
		m_menuNode = iNode;
		const auto mouse = m_canvas.getMouseCanvasPosition();
		m_menuPosition = {mouse.x(), mouse.y()};
		m_openMenu = true;
	});
	m_canvas.setNodeFooterRenderer([this](const core::UUID iNode, const math::vec2f iMin, const math::vec2f iMax,
										  const float iZoom) -> void { drawFooter(iNode, iMin, iMax, iZoom); });
}

void BehaviourTreeDocument::rebuildCanvas() {
	const auto selection = m_canvas.selection();
	m_canvas.clear();
	m_pins.clear();
	m_parentPins.clear();
	m_childPins.clear();
	m_problems = m_tree.validate();
	for (const auto& node: m_tree.nodes) {
		const auto* type = scene::ai::findNodeType(node.type);
		const auto kind = type != nullptr ? type->kind : NodeKind::Action;
		std::string tooltip = node.comment;
		if (tooltip.empty() && type != nullptr)
			tooltip = type->description;
		gui::widgets::Node canvasNode{.id = node.id,
									  .title = type != nullptr ? type->title : node.type,
									  .position = {node.position[0], node.position[1]},
									  .inputs = {},
									  .outputs = {},
									  .customData = {},
									  .titleColor = {1.f, 1.f, 1.f, 1.f},
									  .glyph = getBehaviourNodeGlyph(node.type),
									  .glyphColor = kindColor(kind),
									  .faulty = type == nullptr,
									  .tooltip = std::move(tooltip),
									  .footerHeight = g_footerHeight};
		if (kind != NodeKind::Root) {
			const core::UUID pin;
			canvasNode.inputs.push_back({.id = pin,
										 .label = "Parent",
										 .typeTag = "tree",
										 .kind = gui::widgets::PinKind::Input,
										 .labelColor = {0.8f, 0.8f, 0.85f, 1.f},
										 .exec = true});
			m_pins[pin] = {.node = node.id, .children = false};
			m_parentPins[node.id] = pin;
		}
		if (scene::ai::getMaxChildren(kind) > 0) {
			const core::UUID pin;
			canvasNode.outputs.push_back({.id = pin,
										  .label = scene::ai::getMaxChildren(kind) == 1 ? "Child" : "Children",
										  .typeTag = "tree",
										  .kind = gui::widgets::PinKind::Output,
										  .labelColor = {0.8f, 0.8f, 0.85f, 1.f},
										  .exec = true});
			m_pins[pin] = {.node = node.id, .children = true};
			m_childPins[node.id] = pin;
		}
		std::ignore = m_canvas.addNode(std::move(canvasNode));
	}
	for (const auto& link: m_tree.links) {
		const auto from = m_childPins.find(link.parent);
		const auto to = m_parentPins.find(link.child);
		if (from != m_childPins.end() && to != m_parentPins.end())
			std::ignore = m_canvas.addLink(from->second, to->second, {});
	}
	std::vector<core::UUID> kept;
	for (const auto id: selection)
		if (m_tree.findNode(id) != nullptr)
			kept.push_back(id);
	m_canvas.setSelection(kept);
	if (m_selectedNode && m_tree.findNode(*m_selectedNode) == nullptr)
		m_selectedNode.reset();
}

auto BehaviourTreeDocument::apply(scene::ai::BehaviourTree iNext, const std::string& iDescription, const bool iMerge)
		-> bool {
	if (iNext == m_tree)
		return false;
	m_undo.push(mkUniq<TreeCommand>(m_tree, iNext, iDescription, iMerge));
	m_tree = std::move(iNext);
	rebuildCanvas();
	return true;
}

auto BehaviourTreeDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs)
		-> std::optional<commands::CommandResult> {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("BehaviourTreeDocument: '{}' ignored, no editor to run it.", iName)
		return std::nullopt;
	}
	iArgs.set("path", m_path.generic_string());
	auto result = mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture);
	if (!result)
		return std::nullopt;
	return std::move(*result);
}

void BehaviourTreeDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_tree);
	rebuildCanvas();
}

void BehaviourTreeDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_tree);
	rebuildCanvas();
}

void BehaviourTreeDocument::addNode(const std::string& iType, const std::array<float, 2> iPosition) {
	commands::CommandArgs args{{"type", iType}, {"position", math::vec2{iPosition[0], iPosition[1]}}};
	// A node added from the menu of a node becomes its child.
	if (m_menuNode && m_childPins.contains(*m_menuNode))
		args.set("parent", idText(*m_menuNode));
	if (const auto result = sendCommand("bt.add_node", std::move(args)); result) {
		if (const auto found = result->data.find("node"); found != result->data.end())
			if (const auto* text = std::get_if<std::string>(&found->second); text != nullptr) {
				uint64_t id = 0;
				std::ignore = std::from_chars(text->data(), text->data() + text->size(), id);
				selectNode(core::UUID{id});
			}
	}
}

void BehaviourTreeDocument::deleteNodes(const std::span<const core::UUID> iNodes) {
	if (!iNodes.empty())
		std::ignore = sendCommand("bt.remove_nodes", {{"nodes", idsText(iNodes)}});
}

void BehaviourTreeDocument::duplicateNodes(const std::span<const core::UUID> iNodes) {
	if (iNodes.empty())
		return;
	if (const auto result = sendCommand("bt.duplicate_nodes", {{"nodes", idsText(iNodes)}}); result) {
		std::vector<core::UUID> copies;
		if (const auto found = result->data.find("nodes"); found != result->data.end())
			if (const auto* text = std::get_if<std::string>(&found->second); text != nullptr)
				for (const auto part: std::string_view{*text} | std::views::split(',')) {
					uint64_t id = 0;
					std::ignore = std::from_chars(part.data(), part.data() + part.size(), id);
					copies.emplace_back(id);
				}
		m_canvas.setSelection(copies);
	}
}

void BehaviourTreeDocument::commitMove() {
	std::vector<core::UUID> moved;
	math::vec2 offset{0.f, 0.f};
	for (const auto& canvasNode: m_canvas.nodes()) {
		const auto* node = m_tree.findNode(canvasNode.id);
		if (node == nullptr)
			continue;
		const math::vec2 shift{canvasNode.position.x() - node->position[0],
							   canvasNode.position.y() - node->position[1]};
		if (shift.normSq() <= 0.f)
			continue;
		if (moved.empty())
			offset = shift;
		moved.push_back(node->id);
	}
	if (moved.empty() || !sendCommand("bt.move_nodes", {{"nodes", idsText(moved)}, {"offset", offset}}))
		rebuildCanvas();
}

void BehaviourTreeDocument::selectNode(const std::optional<core::UUID> iNode) {
	m_selectedNode = iNode && m_tree.findNode(*iNode) != nullptr ? iNode : std::nullopt;
	m_canvas.setSelection(m_selectedNode ? std::vector<core::UUID>{*m_selectedNode} : std::vector<core::UUID>{});
}

auto BehaviourTreeDocument::findWatched() const -> shared<scene::ai::BehaviourTreeInstance> {
	if (mp_editorLayer == nullptr)
		return nullptr;
	for (const auto& docPtr: mp_editorLayer->getDocumentManager().list()) {
		if (!docPtr || docPtr->type() != DocumentType::Scene)
			continue;
		const auto* sceneDocument = static_cast<const SceneDocument*>(docPtr.get());
		if (sceneDocument->state() == SceneDocument::State::Edit || !sceneDocument->getActiveScene())
			continue;
		auto& running = *sceneDocument->getActiveScene();
		// The agent selected in the editor first, else the first one running this tree.
		if (const auto selected = mp_editorLayer->getSelectedEntity();
			selected && selected.getScene() == &running &&
			selected.hasComponent<scene::component::BehaviourTreeAgent>()) {
			const auto& agent = selected.getComponent<scene::component::BehaviourTreeAgent>();
			if (agent.m_instance && namesFile(agent.tree, m_path))
				return agent.m_instance;
		}
		std::vector<std::tuple<uint64_t, shared<scene::ai::BehaviourTreeInstance>>> found;
		for (const auto view = running.registry.view<scene::component::BehaviourTreeAgent, scene::component::ID>();
			 const auto handle: view) {
			const auto& agent = view.get<scene::component::BehaviourTreeAgent>(handle);
			if (agent.m_instance && namesFile(agent.tree, m_path))
				found.emplace_back(static_cast<uint64_t>(view.get<scene::component::ID>(handle).id), agent.m_instance);
		}
		if (!found.empty())
			return std::get<1>(*std::ranges::min_element(
					found, {}, [](const auto& iEntry) -> uint64_t { return std::get<0>(iEntry); }));
	}
	return nullptr;
}

void BehaviourTreeDocument::drawFooter(const core::UUID iNode, const math::vec2f iMin, const math::vec2f iMax,
									   const float iZoom) const {
	const auto* node = m_tree.findNode(iNode);
	if (node == nullptr)
		return;
	auto* drawList = ImGui::GetWindowDrawList();
	if (m_watched) {
		const auto status = m_watched->getNodeStatus(iNode);
		drawList->AddRectFilled({iMin.x(), iMax.y() - 3.f * iZoom}, {iMax.x(), iMax.y()}, statusColor(status));
	}
	const auto summary = getBehaviourNodeSummary(*node);
	if (summary.empty() || iZoom < 0.45f)
		return;
	const float size = ImGui::GetFontSize() * std::min(iZoom, 1.f) * 0.9f;
	drawList->PushClipRect({iMin.x(), iMin.y()}, {iMax.x(), iMax.y()}, true);
	drawList->AddText(nullptr, size, {iMin.x() + 6.f * iZoom, iMin.y() + 2.f * iZoom}, IM_COL32(200, 200, 210, 255),
					  summary.c_str());
	drawList->PopClipRect();
}

auto BehaviourTreeDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!scene::ai::saveToFile(m_tree, iPath))
		return false;
	m_path = iPath;
	m_savedText = scene::ai::serialize(m_tree);
	// The next Play reads the tree again.
	if (app::Application::instanced())
		app::Application::get().getEngineContext().getBehaviourTrees().clear();
	return true;
}

auto BehaviourTreeDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto tree = scene::ai::loadFromFile(iPath);
	if (!tree) {
		OWL_WARN("BehaviourTreeDocument: Cannot open '{}'.", iPath.string())
		return false;
	}
	m_tree = std::move(*tree);
	m_path = iPath;
	m_savedText = scene::ai::serialize(m_tree);
	rebuildCanvas();
	m_fitFrames = g_fitFrames;
	return true;
}

auto BehaviourTreeDocument::recoverySnapshot() const -> std::optional<std::string> {
	return scene::ai::serialize(m_tree);
}

auto BehaviourTreeDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto tree = scene::ai::deserialize(iSnapshot, title());
	if (!tree) {
		OWL_WARN("Recovery: Cannot restore the autosave of behaviour tree '{}'.", title())
		return false;
	}
	m_tree = std::move(*tree);
	rebuildCanvas();
	return true;
}

void BehaviourTreeDocument::onImGuiRender() {
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
		m_watched = findWatched();
		renderToolbar();
		m_linkDeleted = false;
		if (m_fitFrames > 0) {
			m_canvas.requestFitToContent();
			--m_fitFrames;
		}
		m_canvas.setSnapToGrid(isGraphSnapOn(mp_editorLayer));
		m_canvas.onRender();
		if (ImGui::BeginDragDropTargetCustom({ImGui::GetItemRectMin(), ImGui::GetItemRectMax()},
											 ImGui::GetID("##btDrop"))) {
			if (const auto* payload = ImGui::AcceptDragDropPayload(g_TreeNodePayload);
				payload != nullptr && payload->DataSize > 0) {
				const auto mouse = m_canvas.getMouseCanvasPosition();
				m_menuNode.reset();
				addNode(std::string{static_cast<const char*>(payload->Data), static_cast<size_t>(payload->DataSize)},
						{mouse.x(), mouse.y()});
			}
			ImGui::EndDragDropTarget();
		}
		if (m_moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_moving = false;
			commitMove();
		}
		const auto selection = m_canvas.selection();
		const bool typing = ImGui::GetIO().WantTextInput;
		if (focused && !m_linkDeleted && !selection.empty() && !typing && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			deleteNodes(selection);
		if (focused && !selection.empty() && !typing && ImGui::GetIO().KeyCtrl &&
			ImGui::IsKeyPressed(ImGuiKey_D, false))
			duplicateNodes(selection);
		if (focused && ImGui::IsKeyPressed(ImGuiKey_Home, false))
			m_canvas.requestFitToContent();
		renderGraphMenu();
	}
	ImGui::End();
}

void BehaviourTreeDocument::renderToolbar() {
	if (gui::widgets::iconButton("##btFrame", {.glyph = gui::icon::focus,
											   .tooltip = "Frame All",
											   .shortcut = "Home",
											   .description = "Show every node."}))
		m_canvas.requestFitToContent();
	ImGui::SameLine();
	graphSnapButton(mp_editorLayer);
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	if (m_problems.empty())
		gui::widgets::badge(std::format("{} Runs as drawn", gui::icon::circleCheck).c_str(),
							gui::widgets::BadgeKind::Success);
	else {
		if (gui::widgets::button(
					"##btProblems",
					{.glyph = gui::icon::triangleAlert,
					 .label = std::format("{} problem{}", m_problems.size(), m_problems.size() > 1 ? "s" : "").c_str(),
					 .tooltip = "Problems of the tree",
					 .description = "Click to list them."}))
			ImGui::OpenPopup("##btProblemList");
		if (ImGui::BeginPopup("##btProblemList")) {
			gui::widgets::menuHeader("Problems of the tree");
			for (const auto& problem: m_problems)
				std::ignore = gui::widgets::menuItem({.glyph = gui::icon::triangleAlert, .label = problem.c_str()});
			ImGui::EndPopup();
		}
	}
	if (m_watched) {
		ImGui::SameLine();
		gui::widgets::badge(std::format("{} Playing: tick {}", gui::icon::play, m_watched->getTickCount()).c_str(),
							gui::widgets::BadgeKind::Info);
	}
}

void BehaviourTreeDocument::renderGraphMenu() {
	if (m_openMenu) {
		ImGui::OpenPopup("##btGraphMenu");
		m_paletteFilter.clear();
		m_openMenu = false;
	}
	if (!ImGui::BeginPopup("##btGraphMenu"))
		return;
	if (m_menuNode) {
		const auto* node = m_tree.findNode(*m_menuNode);
		const auto* type = node != nullptr ? scene::ai::findNodeType(node->type) : nullptr;
		gui::widgets::menuHeader(type != nullptr ? type->title.c_str() : "Node", "Node");
		if (m_childPins.contains(*m_menuNode) && ImGui::BeginMenu("Add Child")) {
			renderPalette(m_menuPosition, true);
			ImGui::EndMenu();
		}
		const std::array nodes{*m_menuNode};
		if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
			duplicateNodes(nodes);
		if (m_tree.getParent(*m_menuNode) &&
			gui::widgets::menuItem({.glyph = gui::icon::unlink, .label = "Unlink From Parent"}))
			std::ignore = sendCommand("bt.unlink", {{"child", idText(*m_menuNode)}});
		if (gui::widgets::menuItem(
					{.glyph = gui::icon::trash2, .label = "Delete Node", .shortcut = "Del", .danger = true}))
			deleteNodes(nodes);
	} else {
		gui::widgets::menuHeader("Add Node", "Palette");
		renderPalette(m_menuPosition, true);
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Frame All", .shortcut = "Home"}))
			m_canvas.requestFitToContent();
	}
	ImGui::EndPopup();
}

void BehaviourTreeDocument::renderPalette(const std::array<float, 2> iPosition, const bool iPopup) {
	if (iPopup && ImGui::IsWindowAppearing())
		ImGui::SetKeyboardFocusHere();
	std::ignore = gui::widgets::searchField(iPopup ? "##btPaletteSearch" : "##btLibrarySearch", m_paletteFilter,
											"Search nodes");
	const bool hasRoot = m_tree.getRoot() != nullptr;
	const auto item = [&](const scene::ai::NodeType& iType) -> void {
		ImGui::PushID(iType.id.c_str());
		const auto row = gui::widgets::treeRow(
				"##type", {.label = iType.title.c_str(),
						   .glyph = getBehaviourNodeGlyph(iType.id),
						   .glyphColor = gui::Theme::mix(kindColor(iType.kind), {1.f, 1.f, 1.f, 1.f}, 0.45f)});
		if (row.clicked) {
			if (!iPopup)
				m_menuNode.reset();
			addNode(iType.id, iPosition);
			if (iPopup)
				ImGui::CloseCurrentPopup();
		}
		if (!iPopup && ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload(g_TreeNodePayload, iType.id.data(), iType.id.size());
			ImGui::Text("%s %s", getBehaviourNodeGlyph(iType.id), iType.title.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s", iType.title.c_str(), iType.description.c_str());
		if (row.open)
			gui::widgets::endTreeRow();
		ImGui::PopID();
	};
	const auto& types = scene::ai::getNodeTypes();
	const auto shown = [hasRoot](const scene::ai::NodeType& iType) -> bool {
		return iType.kind != NodeKind::Root || !hasRoot;
	};
	if (!m_paletteFilter.empty()) {
		for (const auto& type: types)
			if (shown(type) && (gui::widgets::matchesFilter(type.title, m_paletteFilter) ||
								gui::widgets::matchesFilter(type.description, m_paletteFilter)))
				item(type);
		return;
	}
	for (const auto kind:
		 {NodeKind::Root, NodeKind::Composite, NodeKind::Decorator, NodeKind::Condition, NodeKind::Action}) {
		if (kind == NodeKind::Root && hasRoot)
			continue;
		ImGui::PushID(static_cast<int>(kind));
		const auto row = gui::widgets::treeRow(
				"##kind", {.label = kindLabel(kind), .iconName = kindIcon(kind), .hasChildren = true});
		if (row.open) {
			for (const auto& type: types)
				if (type.kind == kind)
					item(type);
			gui::widgets::endTreeRow();
		}
		ImGui::PopID();
	}
}

void BehaviourTreeDocument::renderHierarchyPanel() {
	if (gui::widgets::sectionHeader("Blackboard", "vs_node_pins", false).open) {
		renderBlackboard();
		gui::widgets::endSection();
	}
	if (gui::widgets::sectionHeader("Node Library", "vs_node_inputs", false).open) {
		const auto centre = m_canvas.getMouseCanvasPosition();
		renderPalette({centre.x(), centre.y()}, false);
		gui::widgets::endSection();
	}
}

void BehaviourTreeDocument::renderBlackboard() {
	ImGui::TextWrapped("The keys every agent running the tree holds, with the value it starts with.");
	if (gui::widgets::beginProperties("##btKeys")) {
		for (size_t index = 0; index < m_tree.blackboard.size(); ++index) {
			const auto key = m_tree.blackboard[index];
			ImGui::PushID(static_cast<int>(index));
			const auto type = scene::ai::getBlackboardType(key.value);
			std::string name = key.name;
			if (gui::widgets::propertyText("Name", name, nullptr, "Name of the key; the nodes reading it follow.") &&
				!name.empty())
				std::ignore = sendCommand("bt.set_key", {{"name", key.name}, {"rename", name}, {"merge", true}});
			gui::widgets::propertyLabel("Type", "Kind of the value.");
			if (gui::widgets::beginCombo("##type", std::string{scene::ai::getBlackboardTypeName(type)}.c_str())) {
				for (const auto option: scene::ai::g_BlackboardTypes)
					if (ImGui::Selectable(std::string{scene::ai::getBlackboardTypeName(option)}.c_str(),
										  option == type))
						std::ignore = sendCommand(
								"bt.set_key",
								{{"name", name}, {"type", std::string{scene::ai::getBlackboardTypeName(option)}}});
				ImGui::EndCombo();
			}
			std::ignore = gui::widgets::propertyReset(false);
			auto current = m_tree.findKey(name) != nullptr ? m_tree.findKey(name)->value : key.value;
			bool changed = false;
			switch (scene::ai::getBlackboardType(current)) {
				case scene::ai::BlackboardType::Bool:
					changed = gui::widgets::propertyBool("Value", std::get<bool>(current), false);
					break;
				case scene::ai::BlackboardType::Number:
					{
						auto number = static_cast<float>(std::get<double>(current));
						changed = gui::widgets::propertyFloat("Value", number, 0.f, 0.05f);
						current = static_cast<double>(number);
						break;
					}
				case scene::ai::BlackboardType::String:
					changed = gui::widgets::propertyText("Value", std::get<std::string>(current));
					break;
				case scene::ai::BlackboardType::Entity:
					ImGui::TextDisabled("Entity: set on the agent or by Lua");
					break;
				case scene::ai::BlackboardType::Vector:
					changed = gui::widgets::propertyVec3("Value", std::get<math::vec3>(current), {0.f, 0.f, 0.f});
					break;
			}
			if (changed)
				std::ignore = sendCommand("bt.set_key",
										  {{"name", name}, {"value", scene::ai::toText(current)}, {"merge", true}});
			std::string description = key.description;
			if (gui::widgets::propertyText("Description", description, "one sentence"))
				std::ignore =
						sendCommand("bt.set_key", {{"name", name}, {"description", description}, {"merge", true}});
			if (gui::widgets::button("##remove", {.glyph = gui::icon::trash2,
												  .label = "Remove",
												  .tooltip = "Remove the key",
												  .description = "The nodes reading it then fail."}))
				std::ignore = sendCommand("bt.remove_key", {{"name", name}});
			ImGui::Separator();
			ImGui::PopID();
		}
		gui::widgets::endProperties();
	}
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
	ImGui::InputTextWithHint("##newKey", "new key", &m_newKey);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
	if (gui::widgets::beginCombo("##newType", std::string{scene::ai::getBlackboardTypeName(m_newKeyType)}.c_str())) {
		for (const auto option: scene::ai::g_BlackboardTypes)
			if (ImGui::Selectable(std::string{scene::ai::getBlackboardTypeName(option)}.c_str(),
								  option == m_newKeyType))
				m_newKeyType = option;
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (gui::widgets::iconButton("##addKey", {.glyph = gui::icon::plus,
											  .tooltip = "Add Key",
											  .description = "Declare the key on the blackboard.",
											  .disabled = m_newKey.empty() || m_tree.findKey(m_newKey) != nullptr}) &&
		sendCommand("bt.add_key",
					{{"name", m_newKey}, {"type", std::string{scene::ai::getBlackboardTypeName(m_newKeyType)}}}))
		m_newKey.clear();
	if (m_watched) {
		ImGui::SeparatorText("Running");
		for (const auto& [name, value]: m_watched->getBlackboard())
			ImGui::Text("%s = %s", name.c_str(), scene::ai::toText(value).c_str());
	}
}

void BehaviourTreeDocument::renderPropertiesPanel() {
	const auto* node = m_selectedNode ? m_tree.findNode(*m_selectedNode) : nullptr;
	const auto* type = node != nullptr ? scene::ai::findNodeType(node->type) : nullptr;
	if (node == nullptr || type == nullptr) {
		std::ignore = gui::widgets::emptyState(gui::icon::workflow, "No node selected",
											   "Click a node of the tree to edit its parameters.");
		return;
	}
	if (gui::widgets::sectionHeader(type->title.c_str(), kindIcon(type->kind), false).open)
		renderNodeParameters();
}

void BehaviourTreeDocument::renderNodeParameters() {
	const auto nodeId = *m_selectedNode;
	const auto* node = m_tree.findNode(nodeId);
	const auto* type = scene::ai::findNodeType(node->type);
	ImGui::TextWrapped("%s", type->description.c_str());
	if (m_watched)
		ImGui::TextColored(ImColor(statusColor(m_watched->getNodeStatus(nodeId))), "%s  %s", gui::icon::play,
						   statusName(m_watched->getNodeStatus(nodeId)));
	if (gui::widgets::beginProperties("##btNode")) {
		std::string comment = node->comment;
		if (gui::widgets::propertyText("Comment", comment, "a note", "Shown when the mouse is over the node."))
			std::ignore =
					sendCommand("bt.set_comment", {{"node", idText(nodeId)}, {"comment", comment}, {"merge", true}});
		for (const auto& param: type->params) {
			node = m_tree.findNode(nodeId);
			if (node == nullptr)
				break;
			const auto value = scene::ai::BehaviourTree::getValue(*node, param.name);
			const auto label = toLabel(param.name);
			const auto set = [&](const std::string& iValue, const bool iMerge) -> void {
				std::ignore = sendCommand(
						"bt.set_value",
						{{"node", idText(nodeId)}, {"key", param.name}, {"value", iValue}, {"merge", iMerge}});
			};
			parameterField(param, value, label, m_tree, set);
		}
		gui::widgets::endProperties();
	}
	gui::widgets::endSection();
}

}// namespace owl::nest
