/**
 * @file DialogueDocument.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "DialogueDocument.h"

#include "EditorLayer.h"

#include <app/Application.h>
#include <app/EngineContext.h>
#include <gui/IconFont.h>
#include <gui/Theme.h>
#include <gui/UiLayer.h>
#include <gui/widgets/AssetField.h>
#include <gui/widgets/Badge.h>
#include <gui/widgets/EmptyState.h>
#include <gui/widgets/IconButton.h>
#include <gui/widgets/Menu.h>
#include <gui/widgets/PropertyGrid.h>
#include <gui/widgets/SectionHeader.h>
#include <gui/widgets/TreeRow.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cfloat>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::nest {

namespace {

using data::dialogue::Dialogue;
using data::dialogue::DialogueNode;
using data::dialogue::DialogueText;
using data::dialogue::NodeKind;

// Longest text written in a node title or a pin label.
constexpr size_t g_titleLength = 28;

class ModifyDialogueCommand final : public UndoCommand<Dialogue> {
public:
	ModifyDialogueCommand(Dialogue iBefore, Dialogue iAfter, std::string iDescription, const bool iMerge)
		: m_before{std::move(iBefore)}, m_after{std::move(iAfter)}, m_description{std::move(iDescription)},
		  m_merge{iMerge} {}

	void undo(Dialogue& ioTarget) override { ioTarget = m_before; }

	void redo(Dialogue& ioTarget) override { ioTarget = m_after; }

	[[nodiscard]] auto description() const -> std::string override { return m_description; }

	[[nodiscard]] auto typeId() const -> size_t override { return m_merge ? 1 : 0; }

	[[nodiscard]] auto mergeWith(const UndoCommand& iOther) -> bool override {
		const auto& other = static_cast<const ModifyDialogueCommand&>(iOther);
		if (other.m_description != m_description)
			return false;
		m_after = other.m_after;
		return true;
	}

private:
	Dialogue m_before;
	Dialogue m_after;
	std::string m_description;
	bool m_merge;
};

auto shortened(const std::string_view iText) -> std::string {
	std::string text{iText.substr(0, iText.find('\n'))};
	if (text.size() <= g_titleLength)
		return text;
	// Cut on a UTF-8 character boundary.
	size_t cut = g_titleLength;
	while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) --cut;
	return text.substr(0, cut) + "…";
}

auto shownText(const DialogueText& iText) -> std::string_view { return iText.text.empty() ? iText.key : iText.text; }

auto formatId(const core::UUID iId) -> std::string { return std::to_string(static_cast<uint64_t>(iId)); }

auto parseIds(const std::string_view iText) -> std::vector<core::UUID> {
	std::vector<core::UUID> ids;
	for (const auto part: iText | std::views::split(',')) {
		uint64_t value = 0;
		if (const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), value);
			error == std::errc{})
			ids.emplace_back(value);
	}
	return ids;
}

auto joinIds(const std::vector<core::UUID>& iIds) -> std::string {
	std::string text;
	for (const auto id: iIds) text += (text.empty() ? "" : ",") + formatId(id);
	return text;
}

}// namespace

auto getDialogueNodeGlyph(const NodeKind iKind) -> const char* {
	switch (iKind) {
		case NodeKind::Start:
			return gui::icon::play;
		case NodeKind::Line:
			return gui::icon::userRound;
		case NodeKind::Choice:
			return gui::icon::listOrdered;
		case NodeKind::Branch:
			return gui::icon::split;
		case NodeKind::Event:
			return gui::icon::zap;
		case NodeKind::End:
			return gui::icon::logOut;
	}
	return gui::icon::circleHelp;
}

auto getDialogueNodeLabel(const DialogueNode& iNode) -> std::string {
	switch (iNode.kind) {
		case NodeKind::Line:
		case NodeKind::Choice:
			{
				const auto speaker = shownText(iNode.speaker);
				const auto text = shownText(iNode.text);
				if (speaker.empty() && text.empty())
					return std::string{data::dialogue::getNodeKindName(iNode.kind)};
				return shortened(speaker.empty() ? std::string{text} : std::format("{}: {}", speaker, text));
			}
		case NodeKind::Branch:
			return std::format("{}?", iNode.function.empty() ? std::string{"Branch"} : shortened(iNode.function));
		case NodeKind::Event:
			return std::format("{}()", iNode.function.empty() ? std::string{"Event"} : shortened(iNode.function));
		case NodeKind::Start:
		case NodeKind::End:
			break;
	}
	return std::string{data::dialogue::getNodeKindName(iNode.kind)};
}

auto getDialogueNodeTitle(const DialogueNode& iNode) -> std::string {
	return std::format("{}  {}", getDialogueNodeGlyph(iNode.kind), getDialogueNodeLabel(iNode));
}

auto getDialogueNodeColor(const NodeKind iKind) -> math::vec4 {
	const auto& theme = gui::UiLayer::getTheme();
	switch (iKind) {
		case NodeKind::Start:
			return theme.success;
		case NodeKind::Line:
			return theme.info;
		case NodeKind::Choice:
			return {0.69f, 0.52f, 0.96f, 1.f};
		case NodeKind::Branch:
			return {0.3f, 0.8f, 0.78f, 1.f};
		case NodeKind::Event:
			return theme.accent;
		case NodeKind::End:
			return theme.error;
	}
	return theme.text;
}

auto getDialogueOutputLabel(const DialogueNode& iNode, const size_t iOutput) -> std::string {
	if (iNode.kind == NodeKind::Choice && iOutput < iNode.options.size()) {
		const auto text = shownText(iNode.options[iOutput].text);
		return text.empty() ? std::format("Answer {}", iOutput + 1) : shortened(text);
	}
	if (iNode.kind == NodeKind::Branch)
		return iOutput == 0 ? "True" : "False";
	return "Next";
}

DialogueDocument::DialogueDocument() { m_undo.markSaved(); }

DialogueDocument::~DialogueDocument() = default;

void DialogueDocument::onAttach(EditorLayer* iEditor) {
	NodeGraphDocument::onAttach(iEditor);
	m_rebuild = true;
}

void DialogueDocument::onCanvasReady() {
	m_canvas.setLinkValidator([this](const core::UUID iFrom, const core::UUID iTo) -> bool {
		const auto from = m_pins.find(iFrom);
		const auto to = m_pins.find(iTo);
		return from != m_pins.end() && to != m_pins.end() && from->second.output.has_value() &&
			   !to->second.output.has_value() && from->second.node != to->second.node;
	});
	m_canvas.setOnLinkCreated([this](core::UUID, const core::UUID iFrom, const core::UUID iTo) -> void {
		m_linkEdited = true;
		const auto& from = m_pins.at(iFrom);
		const auto& to = m_pins.at(iTo);
		std::ignore = sendCommand("dialogue.link", {{"from", formatId(from.node)},
													{"output", static_cast<int64_t>(*from.output)},
													{"to", formatId(to.node)}});
		m_rebuild = true;
	});
	m_canvas.setOnLinkDeleted([this](core::UUID) -> void {
		m_linkEdited = true;
		// The canvas dropped the link already: unlink the output whose link is no longer drawn.
		for (const auto& node: m_dialogue.nodes) {
			for (size_t output = 0; output < Dialogue::getOutputCount(node); ++output) {
				const auto target = Dialogue::getOutput(node, output);
				if (target == core::UUID{0})
					continue;
				const auto fromPin = m_outputPins.find({node.id, output});
				const auto toPin = m_inputPins.find(target);
				const bool drawn = fromPin != m_outputPins.end() && toPin != m_inputPins.end() &&
								   std::ranges::any_of(m_canvas.links(), [&](const gui::widgets::Link& iLink) -> bool {
									   return iLink.fromPin == fromPin->second && iLink.toPin == toPin->second;
								   });
				if (!drawn) {
					std::ignore = sendCommand("dialogue.link", {{"from", formatId(node.id)},
																{"output", static_cast<int64_t>(output)},
																{"to", std::string{}}});
					m_rebuild = true;
					return;
				}
			}
		}
	});
	m_canvas.setOnLinkWaypointsChanged([this](const core::UUID iLink, const std::vector<math::vec2f>&) -> void {
		m_linkEdited = true;
		const auto* link = m_canvas.findLink(iLink);
		const auto pin = link != nullptr ? m_pins.find(link->fromPin) : m_pins.end();
		if (pin == m_pins.end() || !pin->second.output)
			return;
		std::string points;
		for (const auto& point: link->waypoints)
			points += std::format("{}{},{}", points.empty() ? "" : ";", point.x(), point.y());
		std::ignore = sendCommand("dialogue.set_route", {{"node", formatId(pin->second.node)},
														 {"output", static_cast<int64_t>(*pin->second.output)},
														 {"points", points}});
	});
	m_canvas.setOnLinkColorChanged([this](const core::UUID iLink, const std::string&) -> void {
		m_linkEdited = true;
		const auto* link = m_canvas.findLink(iLink);
		const auto pin = link != nullptr ? m_pins.find(link->fromPin) : m_pins.end();
		if (pin == m_pins.end() || !pin->second.output)
			return;
		std::ignore = sendCommand("dialogue.set_link_color", {{"node", formatId(pin->second.node)},
															  {"output", static_cast<int64_t>(*pin->second.output)},
															  {"color", link->color}});
	});
	m_canvas.setOnNodeMoved([this](core::UUID, math::vec2f) -> void { m_moving = true; });
	m_canvas.setOnNodeSelected([this](const core::UUID iNode) -> void { m_selectedNode = iNode; });
	m_canvas.setOnContextMenu([this](const std::optional<core::UUID> iNode) -> void {
		m_menuNode = iNode;
		m_menuPosition = m_canvas.getMouseCanvasPosition();
		m_openMenu = true;
	});
}

void DialogueDocument::rebuildCanvas() {
	m_rebuild = false;
	const auto selection = m_canvas.selection();
	m_canvas.clear();
	m_pins.clear();
	m_inputPins.clear();
	m_outputPins.clear();
	const auto problems = m_dialogue.getProblems();
	const auto& theme = gui::UiLayer::getTheme();
	for (const auto& node: m_dialogue.nodes) {
		const bool faulty =
				std::ranges::any_of(problems, [&node](const data::dialogue::DialogueProblem& iProblem) -> bool {
					return iProblem.node == node.id;
				});
		gui::widgets::Node canvasNode{.id = node.id,
									  .title = getDialogueNodeLabel(node),
									  .position = node.position,
									  .inputs = {},
									  .outputs = {},
									  .customData = {},
									  .titleColor = faulty ? theme.error : theme.text,
									  .glyph = getDialogueNodeGlyph(node.kind),
									  .glyphColor = getDialogueNodeColor(node.kind)};
		if (node.kind != NodeKind::Start) {
			const core::UUID pin;
			canvasNode.inputs.push_back({.id = pin,
										 .label = "In",
										 .typeTag = "flow",
										 .kind = gui::widgets::PinKind::Input,
										 .labelColor = theme.text,
										 .exec = true});
			m_pins[pin] = {.node = node.id, .output = std::nullopt};
			m_inputPins[node.id] = pin;
		}
		for (size_t output = 0; output < Dialogue::getOutputCount(node); ++output) {
			const core::UUID pin;
			canvasNode.outputs.push_back({.id = pin,
										  .label = getDialogueOutputLabel(node, output),
										  .typeTag = "flow",
										  .kind = gui::widgets::PinKind::Output,
										  .labelColor = theme.text,
										  .exec = true});
			m_pins[pin] = {.node = node.id, .output = output};
			m_outputPins[{node.id, output}] = pin;
		}
		std::ignore = m_canvas.addNode(std::move(canvasNode));
	}
	for (const auto& node: m_dialogue.nodes) {
		for (size_t output = 0; output < Dialogue::getOutputCount(node); ++output) {
			const auto target = Dialogue::getOutput(node, output);
			const auto from = m_outputPins.find({node.id, output});
			const auto to = m_inputPins.find(target);
			if (target == core::UUID{0} || from == m_outputPins.end() || to == m_inputPins.end())
				continue;
			const auto* route = m_dialogue.findRoute(node.id, output);
			const auto link = m_canvas.addLink(from->second, to->second,
											   route != nullptr ? route->points : std::vector<math::vec2f>{});
			if (route != nullptr && !route->color.empty())
				std::ignore = m_canvas.setLinkColor(link, route->color);
		}
	}
	std::vector<core::UUID> kept;
	for (const auto id: selection)
		if (m_dialogue.findNode(id) != nullptr)
			kept.push_back(id);
	m_canvas.setSelection(kept);
	if (m_selectedNode && m_dialogue.findNode(*m_selectedNode) == nullptr)
		m_selectedNode.reset();
}

auto DialogueDocument::loadFromFile(const std::filesystem::path& iPath) -> bool {
	auto loaded = Dialogue::loadFile(iPath);
	if (!loaded) {
		OWL_CORE_WARN("DialogueDocument: {} is not a dialogue.", iPath.string())
		return false;
	}
	m_dialogue = std::move(*loaded);
	m_path = iPath;
	m_undo.clear();
	m_undo.markSaved();
	m_selectedNode.reset();
	m_rebuild = true;
	m_fitFrames = 3;
	return true;
}

auto DialogueDocument::save() -> bool {
	if (m_path.empty())
		return false;
	return saveAs(m_path);
}

auto DialogueDocument::saveAs(const std::filesystem::path& iPath) -> bool {
	if (!m_dialogue.saveToFile(iPath)) {
		OWL_CORE_ERROR("DialogueDocument: Failed to write '{}'.", iPath.string())
		return false;
	}
	m_path = iPath;
	m_undo.markSaved();
	// The next conversations read the saved file; those playing keep their copy.
	if (app::Application::instanced())
		app::Application::get().getEngineContext().getDialogues().invalidateAll();
	return true;
}

auto DialogueDocument::recoverySnapshot() const -> std::optional<std::string> { return m_dialogue.serializeToString(); }

auto DialogueDocument::restoreRecoverySnapshot(const std::string& iSnapshot) -> bool {
	auto restored = Dialogue::deserializeFromString(iSnapshot);
	if (!restored) {
		OWL_WARN("Recovery: Cannot restore the autosave of the dialogue.")
		return false;
	}
	m_dialogue = std::move(*restored);
	m_undo.clear();
	m_undo.markUnsaved();
	m_rebuild = true;
	return true;
}

void DialogueDocument::performUndo() {
	if (!m_undo.canUndo())
		return;
	m_undo.undo(m_dialogue);
	m_rebuild = true;
	if (m_previewShown)
		restartPreview();
}

void DialogueDocument::performRedo() {
	if (!m_undo.canRedo())
		return;
	m_undo.redo(m_dialogue);
	m_rebuild = true;
	if (m_previewShown)
		restartPreview();
}

auto DialogueDocument::apply(Dialogue iNext, const std::string& iDescription, const bool iMerge) -> bool {
	if (iNext == m_dialogue)
		return false;
	auto before = std::move(m_dialogue);
	m_dialogue = iNext;
	m_undo.push(mkUniq<ModifyDialogueCommand>(std::move(before), std::move(iNext), iDescription, iMerge));
	m_rebuild = true;
	if (m_previewShown)
		restartPreview();
	return true;
}

void DialogueDocument::selectNodes(const std::vector<core::UUID>& iNodes) {
	if (m_rebuild)
		rebuildCanvas();
	m_canvas.setSelection(iNodes);
	if (iNodes.empty())
		m_selectedNode.reset();
	else
		m_selectedNode = iNodes.front();
}

auto DialogueDocument::addNode(const NodeKind iKind, const math::vec2f iPosition,
							   const std::optional<std::pair<core::UUID, size_t>> iFrom) -> std::optional<core::UUID> {
	commands::CommandArgs args{{"kind", std::string{data::dialogue::getNodeKindName(iKind)}},
							   {"position", math::vec2{iPosition.x(), iPosition.y()}}};
	if (iFrom) {
		args.set("from", formatId(iFrom->first));
		args.set("output", static_cast<int64_t>(iFrom->second));
	}
	const auto result = sendCommand("dialogue.add_node", std::move(args));
	if (!result)
		return std::nullopt;
	const auto found = result->data.find("node");
	if (found == result->data.end())
		return std::nullopt;
	const auto ids = parseIds(std::get<std::string>(found->second));
	if (ids.empty())
		return std::nullopt;
	selectNodes(ids);
	return ids.front();
}

void DialogueDocument::deleteSelection() {
	const auto selection = m_canvas.selection();
	if (selection.empty())
		return;
	if (sendCommand("dialogue.remove_nodes", {{"nodes", joinIds(selection)}}))
		selectNodes({});
}

void DialogueDocument::duplicateSelection() {
	const auto selection = m_canvas.selection();
	if (selection.empty())
		return;
	const auto result = sendCommand("dialogue.duplicate_nodes", {{"nodes", joinIds(selection)}});
	if (!result)
		return;
	if (const auto found = result->data.find("nodes"); found != result->data.end())
		selectNodes(parseIds(std::get<std::string>(found->second)));
}

void DialogueDocument::commitMoves() {
	// Nodes dragged together share one shift: one command per shift.
	std::map<std::pair<float, float>, std::vector<core::UUID>> shifts;
	for (const auto& canvasNode: m_canvas.nodes()) {
		const auto* node = m_dialogue.findNode(canvasNode.id);
		if (node == nullptr)
			continue;
		const float dx = canvasNode.position.x() - node->position.x();
		const float dy = canvasNode.position.y() - node->position.y();
		if (std::abs(dx) > 1e-3f || std::abs(dy) > 1e-3f)
			shifts[{dx, dy}].push_back(canvasNode.id);
	}
	for (const auto& [shift, nodes]: shifts)
		std::ignore = sendCommand("dialogue.move_nodes",
								  {{"nodes", joinIds(nodes)}, {"offset", math::vec2{shift.first, shift.second}}});
}

auto DialogueDocument::sendCommand(const std::string_view iName, commands::CommandArgs iArgs)
		-> std::optional<commands::CommandResult> {
	if (mp_editorLayer == nullptr) {
		OWL_WARN("DialogueDocument: {} ignored, no editor to run it.", iName)
		return std::nullopt;
	}
	iArgs.set("path", m_path.generic_string());
	auto result = mp_editorLayer->execute(iName, iArgs, commands::CommandSource::Gesture);
	if (!result)
		return std::nullopt;
	return std::move(*result);
}

auto DialogueDocument::continuesTyping() -> bool {
	const auto item = ImGui::GetItemID();
	const bool merge = item == m_typingItem;
	m_typingItem = item;
	return merge;
}

// --- Preview -----------------------------------------------------------------

auto DialogueDocument::previewHooks() -> data::dialogue::ConversationHooks {
	return {.condition = [this](const std::string_view iName) -> bool {
				const auto found = m_previewConditions.find(iName);
				return found != m_previewConditions.end() && found->second;
			},
			.action = [this](const std::string_view iName) -> void { m_previewActions.emplace_back(iName); }};
}

void DialogueDocument::setPreviewShown(const bool iShown) {
	m_previewShown = iShown;
	if (iShown)
		restartPreview();
	else
		m_preview.reset();
}

void DialogueDocument::restartPreview() {
	m_previewActions.clear();
	m_preview.emplace(mkShared<const Dialogue>(m_dialogue));
	std::ignore = m_preview->start(previewHooks());
	if (const auto* current = m_preview->getCurrent(); current != nullptr)
		selectNodes({current->id});
}

void DialogueDocument::setPreviewCondition(const std::string& iFunction, const bool iValue) {
	m_previewConditions[iFunction] = iValue;
}

void DialogueDocument::advancePreview() {
	if (!m_preview)
		return;
	std::ignore = m_preview->advance(previewHooks());
	if (const auto* current = m_preview->getCurrent(); current != nullptr)
		selectNodes({current->id});
}

void DialogueDocument::choosePreview(const size_t iOffered) {
	if (!m_preview)
		return;
	std::ignore = m_preview->choose(iOffered, previewHooks());
	if (const auto* current = m_preview->getCurrent(); current != nullptr)
		selectNodes({current->id});
}

// --- Rendering ---------------------------------------------------------------

void DialogueDocument::onImGuiRender() {
	if (m_rebuild)
		rebuildCanvas();
	const auto winTitle = documentWindowTitle(DocumentType::Dialogue, title(), static_cast<uint64_t>(id()));
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
		const auto& io = ImGui::GetIO();
		if (focused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
			std::ignore = save();
		renderToolbar();
		if (m_previewShown) {
			const float width = std::max(ImGui::GetContentRegionAvail().x * 0.65f, 200.f);
			ImGui::BeginChild("##dialogueGraphPane", {width, 0.f}, ImGuiChildFlags_ResizeX);
		}
		m_linkEdited = false;
		// The dock gives the canvas its final size over the first frames: the framing is asked again on each of them.
		if (m_fitFrames > 0) {
			m_canvas.requestFitToContent();
			--m_fitFrames;
		}
		m_canvas.setSnapToGrid(isGraphSnapOn(mp_editorLayer));
		m_canvas.onRender();
		if (m_moving && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_moving = false;
			commitMoves();
		}
		if (focused && !m_linkEdited && !io.WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
				deleteSelection();
			if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
				duplicateSelection();
			if (ImGui::IsKeyPressed(ImGuiKey_Home, false))
				frameAll();
		}
		renderGraphMenu();
		if (m_previewShown) {
			ImGui::EndChild();
			ImGui::SameLine();
			if (ImGui::BeginChild("##dialoguePreviewPane"))
				renderPreview();
			ImGui::EndChild();
		}
	}
	ImGui::End();
}

void DialogueDocument::renderToolbar() {
	const auto separator = []() -> void {
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();
	};
	if (gui::widgets::iconButton("##dlgFrame", {.glyph = gui::icon::focus,
												.tooltip = "Frame All",
												.shortcut = "Home",
												.description = "Show every node."}))
		frameAll();
	ImGui::SameLine();
	graphSnapButton(mp_editorLayer);
	separator();
	const auto center = m_canvas.getMouseCanvasPosition();
	for (const auto kind: data::dialogue::getNodeKinds()) {
		ImGui::PushID(static_cast<int>(kind));
		const auto tip = std::format("Add {}", data::dialogue::getNodeKindName(kind));
		if (gui::widgets::iconButton("##add", {.glyph = getDialogueNodeGlyph(kind),
											   .tooltip = tip.c_str(),
											   .description = "Add a node beside the selected one, linked from it."})) {
			std::optional<std::pair<core::UUID, size_t>> from;
			math::vec2f position = center;
			if (const auto* selected = m_selectedNode ? m_dialogue.findNode(*m_selectedNode) : nullptr;
				selected != nullptr && kind != NodeKind::Start) {
				position = {selected->position.x() + 260.f, selected->position.y()};
				for (size_t output = 0; output < Dialogue::getOutputCount(*selected); ++output)
					if (Dialogue::getOutput(*selected, output) == core::UUID{0}) {
						from = std::pair{selected->id, output};
						break;
					}
			}
			std::ignore = addNode(kind, position, from);
		}
		ImGui::PopID();
		ImGui::SameLine(0.f, 2.f);
	}
	separator();
	if (gui::widgets::iconButton("##dlgDuplicate", {.glyph = gui::icon::copy,
													.tooltip = "Duplicate",
													.shortcut = "Ctrl+D",
													.description = "Copy the selected nodes beside them.",
													.disabled = m_canvas.selection().empty()}))
		duplicateSelection();
	ImGui::SameLine(0.f, 2.f);
	if (gui::widgets::iconButton("##dlgDelete", {.glyph = gui::icon::trash2,
												 .tooltip = "Delete",
												 .shortcut = "Del",
												 .description = "Remove the selected nodes and the links to them.",
												 .disabled = m_canvas.selection().empty()}))
		deleteSelection();
	separator();
	if (gui::widgets::iconButton("##dlgPreview", {.glyph = gui::icon::play,
												  .tooltip = "Preview",
												  .description = "Play the dialogue beside the graph, the conditions "
																 "answered by hand.",
												  .selected = m_previewShown}))
		setPreviewShown(!m_previewShown);
	separator();
	const auto problems = m_dialogue.getProblems();
	if (problems.empty()) {
		gui::widgets::badge(std::format("{} Ready", gui::icon::circleCheck).c_str(), gui::widgets::BadgeKind::Success);
	} else {
		gui::widgets::badge(std::format("{} {} problem{}", gui::icon::triangleAlert, problems.size(),
										problems.size() > 1 ? "s" : "")
									.c_str(),
							gui::widgets::BadgeKind::Warning);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", problems.front().message.c_str());
		if (ImGui::IsItemClicked() && problems.front().node != core::UUID{0})
			selectNodes({problems.front().node});
	}
}

void DialogueDocument::renderGraphMenu() {
	if (m_openMenu) {
		m_openMenu = false;
		ImGui::OpenPopup("##dialogueMenu");
	}
	if (!ImGui::BeginPopup("##dialogueMenu"))
		return;
	const auto* node = m_menuNode ? m_dialogue.findNode(*m_menuNode) : nullptr;
	if (node != nullptr) {
		if (!std::ranges::contains(m_canvas.selection(), node->id))
			selectNodes({node->id});
		for (size_t output = 0; output < Dialogue::getOutputCount(*node); ++output) {
			ImGui::PushID(static_cast<int>(output));
			const auto label = std::format("Add Line After '{}'", getDialogueOutputLabel(*node, output));
			if (gui::widgets::menuItem({.glyph = gui::icon::userRound, .label = label.c_str()}))
				std::ignore = addNode(NodeKind::Line, {node->position.x() + 260.f, node->position.y()},
									  std::pair{node->id, output});
			ImGui::PopID();
		}
		if (node->kind == NodeKind::Choice && gui::widgets::menuItem({.glyph = gui::icon::plus, .label = "Add Option"}))
			std::ignore = sendCommand("dialogue.add_option", {{"node", formatId(node->id)}});
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::copy, .label = "Duplicate", .shortcut = "Ctrl+D"}))
			duplicateSelection();
		if (gui::widgets::menuItem({.glyph = gui::icon::trash2, .label = "Delete", .shortcut = "Del", .danger = true}))
			deleteSelection();
	} else {
		for (const auto kind: data::dialogue::getNodeKinds()) {
			const auto label = std::format("Add {}", data::dialogue::getNodeKindName(kind));
			if (gui::widgets::menuItem({.glyph = getDialogueNodeGlyph(kind), .label = label.c_str()}))
				std::ignore = addNode(kind, m_menuPosition);
		}
		ImGui::Separator();
		if (gui::widgets::menuItem({.glyph = gui::icon::focus, .label = "Frame All", .shortcut = "Home"}))
			frameAll();
	}
	ImGui::EndPopup();
}

void DialogueDocument::renderPreview() {
	if (gui::widgets::iconButton("##dlgRestart", {.glyph = gui::icon::rotateCcw,
												  .tooltip = "Restart",
												  .description = "Play the dialogue again from its Start node."}))
		restartPreview();
	ImGui::SameLine();
	ImGui::TextDisabled("Preview");
	ImGui::Separator();
	const auto* current = m_preview ? m_preview->getCurrent() : nullptr;
	if (current == nullptr) {
		gui::widgets::emptyState(gui::icon::logOut, "Conversation over",
								 "The dialogue reached an End node, or a link that leads nowhere.");
	} else {
		if (const auto speaker = shownText(current->speaker); !speaker.empty()) {
			gui::UiLayer::pushTextStyle(gui::TextStyle::Strong);
			ImGui::TextUnformatted(speaker.data(), speaker.data() + speaker.size());
			gui::UiLayer::popTextStyle();
		}
		const auto text = shownText(current->text);
		ImGui::TextWrapped("%.*s", static_cast<int>(text.size()), text.data());
		if (!current->sound.empty())
			ImGui::TextDisabled("%s %s", gui::icon::volume2, current->sound.c_str());
		ImGui::Spacing();
		if (current->kind == NodeKind::Line) {
			if (ImGui::Button(std::format("{} Next", gui::icon::arrowRight).c_str()))
				advancePreview();
		} else {
			const auto offered = m_preview->getOfferedOptions();
			for (size_t index = 0; index < offered.size(); ++index) {
				ImGui::PushID(static_cast<int>(index));
				const auto label = std::format("{}. {}", index + 1, shownText(current->options[offered[index]].text));
				if (ImGui::Button(label.c_str(), {-FLT_MIN, 0.f})) {
					ImGui::PopID();
					choosePreview(index);
					break;
				}
				ImGui::PopID();
			}
		}
	}
	ImGui::Spacing();
	std::vector<std::string> conditions;
	for (const auto& node: m_dialogue.nodes) {
		if (node.kind == NodeKind::Branch && !node.function.empty())
			conditions.push_back(node.function);
		for (const auto& option: node.options)
			if (!option.condition.empty())
				conditions.push_back(option.condition);
	}
	std::ranges::sort(conditions);
	const auto [first, last] = std::ranges::unique(conditions);
	conditions.erase(first, last);
	if (!conditions.empty() && gui::widgets::subHeader("Conditions", true)) {
		ImGui::TextDisabled("What the script's functions return.");
		for (const auto& condition: conditions) {
			bool value = m_previewConditions[condition];
			if (ImGui::Checkbox(std::format("{}()", condition).c_str(), &value))
				setPreviewCondition(condition, value);
		}
	}
	if (!m_previewActions.empty() && gui::widgets::subHeader("Actions called", true))
		for (const auto& action: m_previewActions) ImGui::BulletText("%s()", action.c_str());
}

void DialogueDocument::renderHierarchyPanel() {
	const auto problems = m_dialogue.getProblems();
	for (const auto& problem: problems) {
		ImGui::PushID(&problem);
		gui::widgets::notice(problem.message.c_str(), gui::widgets::BadgeKind::Warning);
		if (ImGui::IsItemClicked() && problem.node != core::UUID{0})
			selectNodes({problem.node});
		ImGui::PopID();
	}
	const auto selection = m_canvas.selection();
	for (const auto& node: m_dialogue.nodes) {
		ImGui::PushID(static_cast<int>(static_cast<uint64_t>(node.id) & 0x7FFFFFFFU));
		const auto label = getDialogueNodeLabel(node);
		const auto result = gui::widgets::treeRow("##node", {.label = label.c_str(),
															 .glyph = getDialogueNodeGlyph(node.kind),
															 .glyphColor = getDialogueNodeColor(node.kind),
															 .selected = std::ranges::contains(selection, node.id)});
		if (result.clicked) {
			if (ImGui::GetIO().KeyCtrl) {
				auto next = selection;
				if (const auto found = std::ranges::find(next, node.id); found != next.end())
					next.erase(found);
				else
					next.push_back(node.id);
				selectNodes(next);
			} else {
				selectNodes({node.id});
			}
		}
		ImGui::PopID();
	}
}

void DialogueDocument::renderText(const char* iLabel, const DialogueText& iText, const char* iTextField,
								  const char* iKeyField, const bool iMultiline, const commands::CommandArgs& iArgs,
								  const std::string_view iCommand) {
	std::string text = iText.text;
	gui::widgets::propertyLabel(iLabel);
	ImGui::SetNextItemWidth(-FLT_MIN);
	const bool changed = iMultiline ? ImGui::InputTextMultiline(std::format("##{}", iTextField).c_str(), &text,
																{-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f})
									: ImGui::InputText(std::format("##{}", iTextField).c_str(), &text);
	std::ignore = gui::widgets::propertyReset(false);
	if (changed) {
		auto args = iArgs;
		args.set(iTextField, text);
		args.set("merge", continuesTyping());
		std::ignore = sendCommand(iCommand, std::move(args));
	}
	std::string key = iText.key;
	if (gui::widgets::propertyText(std::format("{} Key", iLabel).c_str(), key, "No translation",
								   "Key of the string tables (Localisation) translating the text.")) {
		auto args = iArgs;
		args.set(iKeyField, key);
		args.set("merge", continuesTyping());
		std::ignore = sendCommand(iCommand, std::move(args));
	}
	if (!iText.key.empty() && app::Application::instanced()) {
		const auto translated = app::Application::get().getEngineContext().translate(iText.key);
		gui::widgets::propertyLabel("");
		if (translated == iText.key)
			ImGui::TextDisabled("%s No string with this key", gui::icon::triangleAlert);
		else
			ImGui::TextDisabled("%s %.*s", gui::icon::globe, static_cast<int>(translated.size()), translated.data());
		std::ignore = gui::widgets::propertyReset(false);
	}
}

void DialogueDocument::renderOptions(const DialogueNode& iNode) {
	if (!gui::widgets::subHeader("Answers", true))
		return;
	const auto node = formatId(iNode.id);
	for (size_t index = 0; index < iNode.options.size(); ++index) {
		const auto& option = iNode.options[index];
		ImGui::PushID(static_cast<int>(index));
		ImGui::SeparatorText(std::format("Answer {}", index + 1).c_str());
		const commands::CommandArgs args{{"node", node}, {"option", static_cast<int64_t>(index)}};
		if (gui::widgets::beginProperties("##option")) {
			renderText("Text", option.text, "text", "text_key", false, args, "dialogue.set_option");
			std::string condition = option.condition;
			if (gui::widgets::propertyText("Condition", condition, "Always offered",
										   "Global function of the script that starts the conversation; the answer is "
										   "offered when it returns true.")) {
				auto edit = args;
				edit.set("condition", condition);
				edit.set("merge", continuesTyping());
				std::ignore = sendCommand("dialogue.set_option", std::move(edit));
			}
			const auto target = option.next;
			const auto* next = m_dialogue.findNode(target);
			gui::widgets::propertyLabel("Leads To");
			ImGui::TextDisabled("%s", next != nullptr ? getDialogueNodeTitle(*next).c_str() : "Nothing: ends");
			std::ignore = gui::widgets::propertyReset(false);
			gui::widgets::endProperties();
		}
		if (gui::widgets::iconButton("##up", {.glyph = gui::icon::arrowUp,
											  .tooltip = "Move Up",
											  .description = "Offer the answer one place earlier.",
											  .disabled = index == 0}))
			std::ignore =
					sendCommand("dialogue.move_option", {{"node", node}, {"option", static_cast<int64_t>(index)}});
		ImGui::SameLine(0.f, 2.f);
		if (gui::widgets::iconButton("##down", {.glyph = gui::icon::arrowDown,
												.tooltip = "Move Down",
												.description = "Offer the answer one place later.",
												.disabled = index + 1 >= iNode.options.size()}))
			std::ignore = sendCommand("dialogue.move_option",
									  {{"node", node}, {"option", static_cast<int64_t>(index)}, {"down", true}});
		ImGui::SameLine(0.f, 2.f);
		if (gui::widgets::iconButton("##remove", {.glyph = gui::icon::trash2,
												  .tooltip = "Remove",
												  .description = "Remove the answer and its link."})) {
			std::ignore =
					sendCommand("dialogue.remove_option", {{"node", node}, {"option", static_cast<int64_t>(index)}});
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	if (ImGui::Button(std::format("{} Add Answer", gui::icon::plus).c_str()))
		std::ignore = sendCommand("dialogue.add_option", {{"node", node}});
}

void DialogueDocument::renderPropertiesPanel() {
	const auto* node = m_selectedNode ? m_dialogue.findNode(*m_selectedNode) : nullptr;
	if (node == nullptr) {
		gui::widgets::emptyState(gui::icon::mousePointerClick, "No node selected",
								 "Click a node in the graph or the list. Right-click the graph to add one.");
		const auto functions = m_dialogue.getFunctions();
		if (!functions.empty() && gui::widgets::subHeader("Lua Functions", true)) {
			ImGui::TextWrapped("Global functions of the script that starts the conversation (dialogue.start):");
			for (const auto& function: functions) ImGui::BulletText("%s()", function.c_str());
		}
		return;
	}
	const auto id = formatId(node->id);
	ImGui::Text("%s  %.*s", getDialogueNodeGlyph(node->kind),
				static_cast<int>(data::dialogue::getNodeKindName(node->kind).size()),
				data::dialogue::getNodeKindName(node->kind).data());
	const commands::CommandArgs args{{"node", id}};
	if (gui::widgets::beginProperties("##dialogueNode")) {
		if (node->kind == NodeKind::Line || node->kind == NodeKind::Choice) {
			renderText("Speaker", node->speaker, "speaker", "speaker_key", false, args, "dialogue.set_node");
			renderText("Text", node->text, "text", "text_key", true, args, "dialogue.set_node");
			std::string sound = node->sound;
			bool changed = gui::widgets::propertyText("Sound", sound, "None",
													  "Voice line or sound played with the text; "
													  "drop a sound file here.");
			if (std::filesystem::path dropped; gui::widgets::assetDropTarget(gui::widgets::AssetKind::Sound, dropped)) {
				sound = dropped.generic_string();
				changed = true;
			}
			if (changed) {
				auto edit = args;
				edit.set("sound", sound);
				edit.set("merge", continuesTyping());
				std::ignore = sendCommand("dialogue.set_node", std::move(edit));
			}
		}
		if (node->kind == NodeKind::Branch || node->kind == NodeKind::Event) {
			std::string function = node->function;
			const bool branch = node->kind == NodeKind::Branch;
			if (gui::widgets::propertyText(branch ? "Condition" : "Function", function, "Function name",
										   branch ? "Global function of the script that starts the conversation; "
													"True is followed when it returns true."
												  : "Global function of the script that starts the conversation, "
													"called when the conversation passes.")) {
				auto edit = args;
				edit.set("function", function);
				edit.set("merge", continuesTyping());
				std::ignore = sendCommand("dialogue.set_node", std::move(edit));
			}
		}
		if (node->kind != NodeKind::Choice) {
			for (size_t output = 0; output < Dialogue::getOutputCount(*node); ++output) {
				const auto* next = m_dialogue.findNode(Dialogue::getOutput(*node, output));
				gui::widgets::propertyLabel(getDialogueOutputLabel(*node, output).c_str());
				ImGui::TextDisabled("%s", next != nullptr ? getDialogueNodeTitle(*next).c_str() : "Nothing: ends");
				std::ignore = gui::widgets::propertyReset(false);
			}
		}
		gui::widgets::endProperties();
	}
	if (node->kind == NodeKind::Choice)
		renderOptions(*node);
}

}// namespace owl::nest
