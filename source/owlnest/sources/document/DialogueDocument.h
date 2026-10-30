/**
 * @file DialogueDocument.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"
#include "UndoManager.h"
#include "commands/CommandRegistry.h"
#include "commands/DialogueCommands.h"

#include <core/UUID.h>
#include <data/dialogue/Conversation.h>
#include <data/dialogue/Dialogue.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace owl::nest {

/// Undo stack of a dialogue.
using DialogueUndoManager = UndoManager<data::dialogue::Dialogue>;

/**
 * @brief
 *  Glyph of a dialogue node kind, in the graph, the node list and the menus.
 * @param[in] iKind The kind.
 * @return The `gui::icon` glyph.
 */
[[nodiscard]] auto getDialogueNodeGlyph(data::dialogue::NodeKind iKind) -> const char*;

/**
 * @brief
 *  Title of a dialogue node in the graph: its glyph, then who speaks and what, or the function it calls.
 * @param[in] iNode The node.
 * @return The title, shortened to fit a node.
 */
[[nodiscard]] auto getDialogueNodeTitle(const data::dialogue::DialogueNode& iNode) -> std::string;

/**
 * @brief
 *  Label of an output of a dialogue node: `Next`, `True` / `False`, or the text of the option.
 * @param[in] iNode The node.
 * @param[in] iOutput Index of the output.
 * @return The label, shortened to fit a pin.
 */
[[nodiscard]] auto getDialogueOutputLabel(const data::dialogue::DialogueNode& iNode, size_t iOutput) -> std::string;

/**
 * @brief
 *  A `.owldlg` dialogue in a tab: the graph of its nodes, their texts in the side panels, and a preview that plays it
 *  with the conditions ticked by hand. Every gesture runs a `dialogue.*` command.
 */
class DialogueDocument final : public NodeGraphDocument, public commands::DialogueEditor {
public:
	DialogueDocument(const DialogueDocument&) = delete;

	DialogueDocument(DialogueDocument&&) = delete;

	auto operator=(const DialogueDocument&) -> DialogueDocument& = delete;

	auto operator=(DialogueDocument&&) -> DialogueDocument& = delete;

	/**
	 * @brief
	 *  An empty document.
	 */
	DialogueDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~DialogueDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Type of the document.
	 * @return `DocumentType::Dialogue`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Dialogue; }

	/**
	 * @brief
	 *  Whether the dialogue changed since its last save.
	 * @return True when unsaved.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach to the editor and build the graph.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Draw the tab: the toolbar, the graph and, when shown, the preview.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  The document draws the Hierarchy and Properties panels itself.
	 * @return True.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Draw the node list and the problems of the dialogue.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Draw the fields of the selected node, or the functions the dialogue calls.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the Hierarchy panel.
	 * @return `Nodes`.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Nodes"; }

	/**
	 * @brief
	 *  Title of the Properties panel.
	 * @return `Node`.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Node"; }

	/**
	 * @brief
	 *  Write the dialogue to its file.
	 * @return True when written.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Write the dialogue to a file, which becomes its file.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Read a dialogue file.
	 * @param[in] iPath The file.
	 * @return True when read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The dialogue as YAML, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore an autosave.
	 * @param[in] iSnapshot Text given by `recoverySnapshot`.
	 * @return True when restored.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when one can.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when one can.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last undone edit.
	 */
	void performRedo() override;

	// --- Dialogue editor -----------------------------------------------------
	/**
	 * @brief
	 *  The dialogue edited.
	 * @return The dialogue.
	 */
	[[nodiscard]] auto dialogue() const -> const data::dialogue::Dialogue& override { return m_dialogue; }

	/**
	 * @brief
	 *  Replace the dialogue as one undo entry; the graph follows.
	 * @param[in] iNext The dialogue after the edit.
	 * @param[in] iDescription Description of the undo entry.
	 * @param[in] iMerge Whether the entry merges with the previous one of the same description.
	 * @return False when nothing changes.
	 */
	auto apply(data::dialogue::Dialogue iNext, const std::string& iDescription, bool iMerge) -> bool override;

	// --- Selection and gestures ----------------------------------------------
	/**
	 * @brief
	 *  Select nodes in the graph; the first is shown in the Properties panel.
	 * @param[in] iNodes The nodes.
	 */
	void selectNodes(const std::vector<core::UUID>& iNodes);

	/**
	 * @brief
	 *  Node shown in the Properties panel.
	 * @return The node id, nothing when none is.
	 */
	[[nodiscard]] auto getSelectedNode() const -> const std::optional<core::UUID>& { return m_selectedNode; }

	/**
	 * @brief
	 *  Add a node at a point of the graph (`dialogue.add_node`) and select it.
	 * @param[in] iKind What the node does.
	 * @param[in] iPosition Top-left corner in the graph.
	 * @param[in] iFrom Node and output leading to it, when the node continues another one.
	 * @return The new node, nothing when the command failed.
	 */
	auto addNode(data::dialogue::NodeKind iKind, math::vec2f iPosition,
				 std::optional<std::pair<core::UUID, size_t>> iFrom = std::nullopt) -> std::optional<core::UUID>;

	/**
	 * @brief
	 *  Remove the selected nodes (`dialogue.remove_nodes`).
	 */
	void deleteSelection();

	/**
	 * @brief
	 *  Copy the selected nodes beside them (`dialogue.duplicate_nodes`) and select the copies.
	 */
	void duplicateSelection();

	/**
	 * @brief
	 *  Frame every node of the graph.
	 */
	void frameAll() { m_canvas.requestFitToContent(); }

	// --- Preview -------------------------------------------------------------
	/**
	 * @brief
	 *  Show or hide the preview, which plays the dialogue from its Start node when shown.
	 * @param[in] iShown Whether it is shown.
	 */
	void setPreviewShown(bool iShown);

	/**
	 * @brief
	 *  Whether the preview is shown.
	 * @return True when it is.
	 */
	[[nodiscard]] auto isPreviewShown() const -> bool { return m_previewShown; }

	/**
	 * @brief
	 *  Play the dialogue again from its Start node in the preview.
	 */
	void restartPreview();

	/**
	 * @brief
	 *  The conversation of the preview.
	 * @return The conversation, nothing before the preview starts.
	 */
	[[nodiscard]] auto getPreview() const -> const std::optional<data::dialogue::Conversation>& { return m_preview; }

	/**
	 * @brief
	 *  Answer a condition of the preview, as the script would.
	 * @param[in] iFunction The condition function.
	 * @param[in] iValue What it returns.
	 */
	void setPreviewCondition(const std::string& iFunction, bool iValue);

	/**
	 * @brief
	 *  Show the next line of the preview.
	 */
	void advancePreview();

	/**
	 * @brief
	 *  Pick an offered answer in the preview.
	 * @param[in] iOffered Index among the offered answers.
	 */
	void choosePreview(size_t iOffered);

	/**
	 * @brief
	 *  Functions the preview called, in order, since it started.
	 * @return The function names.
	 */
	[[nodiscard]] auto getPreviewActions() const -> const std::vector<std::string>& { return m_previewActions; }

private:
	/// A pin of the graph: its node, and its output (none for the input).
	struct PinRef {
		/// Node of the pin.
		core::UUID node{0};
		/// Output index, nothing for the input of the node.
		std::optional<size_t> output;
	};

	/**
	 * @brief
	 *  Install the graph callbacks.
	 */
	void onCanvasReady() override;

	/**
	 * @brief
	 *  Build the graph again from the dialogue.
	 */
	void rebuildCanvas();

	/**
	 * @brief
	 *  Send the moves of the nodes dragged in the graph, one command per shift.
	 */
	void commitMoves();

	/**
	 * @brief
	 *  Run a `dialogue.*` command on this dialogue as a gesture.
	 * @param[in] iName The command.
	 * @param[in] iArgs Its arguments, without `path`.
	 * @return The result, nothing when it failed or there is no editor.
	 */
	auto sendCommand(std::string_view iName, commands::CommandArgs iArgs) -> std::optional<commands::CommandResult>;

	/**
	 * @brief
	 *  Whether the edit of a text field continues the previous one (same field, still typing).
	 * @return True when it merges.
	 */
	[[nodiscard]] auto continuesTyping() -> bool;

	/**
	 * @brief
	 *  Hooks of the preview: the ticked conditions, the actions logged.
	 * @return The hooks.
	 */
	[[nodiscard]] auto previewHooks() -> data::dialogue::ConversationHooks;

	/**
	 * @brief
	 *  Draw the toolbar of the tab.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Draw the menu of the graph (add nodes on the background, edit a node).
	 */
	void renderGraphMenu();

	/**
	 * @brief
	 *  Draw the preview pane.
	 */
	void renderPreview();

	/**
	 * @brief
	 *  Draw a text and its localisation key, with the translation of the key.
	 * @param[in] iLabel Label of the text.
	 * @param[in] iText The text.
	 * @param[in] iTextField Argument the text is sent as.
	 * @param[in] iKeyField Argument the key is sent as.
	 * @param[in] iMultiline Whether the text takes several lines.
	 * @param[in] iArgs Arguments naming what is edited (`node`, `option`).
	 * @param[in] iCommand The command that edits it.
	 */
	void renderText(const char* iLabel, const data::dialogue::DialogueText& iText, const char* iTextField,
					const char* iKeyField, bool iMultiline, const commands::CommandArgs& iArgs,
					std::string_view iCommand);

	/**
	 * @brief
	 *  Draw the answers of a Choice node.
	 * @param[in] iNode The node.
	 */
	void renderOptions(const data::dialogue::DialogueNode& iNode);

	/// The edited dialogue.
	data::dialogue::Dialogue m_dialogue;
	/// Undo stack of the dialogue.
	DialogueUndoManager m_undo;
	/// Graph pins by id.
	std::map<core::UUID, PinRef> m_pins;
	/// Input pin of each node.
	std::map<uint64_t, core::UUID> m_inputPins;
	/// Output pins by node and output.
	std::map<std::pair<uint64_t, size_t>, core::UUID> m_outputPins;
	/// Node shown in the Properties panel.
	std::optional<core::UUID> m_selectedNode;
	/// Node the graph menu was opened on.
	std::optional<core::UUID> m_menuNode;
	/// Graph position the menu adds nodes at.
	math::vec2f m_menuPosition{0.f, 0.f};
	/// Whether the graph asked for its menu this frame.
	bool m_openMenu{false};
	/// Whether the graph must be built again from the dialogue.
	bool m_rebuild{true};
	/// Whether nodes are being dragged (their moves are sent on release).
	bool m_moving{false};
	/// Whether the graph used this frame for a link edit (the Delete key is then not for the nodes).
	bool m_linkEdited{false};
	/// ImGui id of the text field typed in last, so each typing is one undo entry.
	uint32_t m_typingItem{0};
	/// Frames left during which the graph frames every node (after a load, while the dock settles).
	uint32_t m_fitFrames{3};
	/// Whether the preview pane is shown.
	bool m_previewShown{false};
	/// Conversation of the preview.
	std::optional<data::dialogue::Conversation> m_preview;
	/// Answers of the conditions in the preview, by function.
	std::map<std::string, bool, std::less<>> m_previewConditions;
	/// Functions the preview called.
	std::vector<std::string> m_previewActions;
};

}// namespace owl::nest
