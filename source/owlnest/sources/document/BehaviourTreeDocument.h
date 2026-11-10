/**
 * @file BehaviourTreeDocument.h
 * @author Silmaen
 * @date 10/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"
#include "UndoManager.h"
#include "commands/BehaviourTreeCommands.h"
#include "commands/CommandRegistry.h"

#include <core/UUID.h>
#include <scene/ai/BehaviourTree.h>
#include <scene/ai/BehaviourTreeInstance.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/// Undo stack of a behaviour tree.
using BehaviourTreeUndoManager = UndoManager<scene::ai::BehaviourTree>;

/**
 * @brief
 *  Glyph of a node type in the canvas and the palette.
 * @param[in] iType The node type identifier.
 * @return The `gui::icon` glyph.
 */
[[nodiscard]] auto getBehaviourNodeGlyph(std::string_view iType) -> const char*;

/**
 * @brief
 *  One line telling what a node does with its parameters (`health > 3`, `eat()`), drawn under its title.
 * @param[in] iNode The node.
 * @return The line, empty for a node without parameter.
 */
[[nodiscard]] auto getBehaviourNodeSummary(const scene::ai::TreeNode& iNode) -> std::string;

/**
 * @brief
 *  The tab of a `.owlbt` behaviour tree: the nodes on a canvas, the children of a node run from left to right; the
 *  blackboard keys in the left panel, the parameters of the selected node in the right one. While a scene plays, the
 *  status of every node of the agent watched is drawn on it.
 *
 * Every gesture goes through a `bt.*` command of the registry.
 */
class BehaviourTreeDocument final : public NodeGraphDocument, public commands::BehaviourTreeEditor {
public:
	BehaviourTreeDocument(const BehaviourTreeDocument&) = delete;

	BehaviourTreeDocument(BehaviourTreeDocument&&) = delete;

	auto operator=(const BehaviourTreeDocument&) -> BehaviourTreeDocument& = delete;

	auto operator=(BehaviourTreeDocument&&) -> BehaviourTreeDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	BehaviourTreeDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~BehaviourTreeDocument() override;

	/**
	 * @brief
	 *  Whether the tree changed since the last save.
	 * @return True when it did.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Draw the tab.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Whether the document draws the hierarchy and properties panels itself.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Draw the blackboard and the node library.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Draw the parameters of the selected node.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the hierarchy panel.
	 * @return The title.
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Blackboard"; }

	/**
	 * @brief
	 *  Title of the properties panel.
	 * @return The title.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Node"; }

	/**
	 * @brief
	 *  Write the tree to a file.
	 * @param[in] iPath The file.
	 * @return True when written.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Read the tree of a file.
	 * @param[in] iPath The file.
	 * @return True when read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  The text of the tree for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore the tree of an autosave.
	 * @param[in] iSnapshot The text.
	 * @return True when restored.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when it can.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when it can.
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

	/**
	 * @brief
	 *  The tree as it is.
	 * @return The tree.
	 */
	[[nodiscard]] auto tree() const -> const scene::ai::BehaviourTree& override { return m_tree; }

	/**
	 * @brief
	 *  Replace the tree, as one undo entry.
	 * @param[in] iNext The tree after the edit.
	 * @param[in] iDescription Description of the undo entry.
	 * @param[in] iMerge Whether it merges with the previous entry of the same description.
	 * @return False when nothing changes.
	 */
	auto apply(scene::ai::BehaviourTree iNext, const std::string& iDescription, bool iMerge) -> bool override;

	/**
	 * @brief
	 *  Select a node (shown in the Node panel).
	 * @param[in] iNode The node, none to clear.
	 */
	void selectNode(std::optional<core::UUID> iNode);

	/**
	 * @brief
	 *  The selected node.
	 * @return The node, none when nothing is selected.
	 */
	[[nodiscard]] auto getSelectedNode() const -> const std::optional<core::UUID>& { return m_selectedNode; }

	/**
	 * @brief
	 *  Run a `bt.*` command on this tree, as a gesture.
	 * @param[in] iName The command.
	 * @param[in] iArgs Its arguments, `path` added.
	 * @return The result, none when it failed.
	 */
	auto sendCommand(std::string_view iName, commands::CommandArgs iArgs) -> std::optional<commands::CommandResult>;

private:
	/// A pin of the canvas: its node and its side.
	struct PinRef {
		/// The node.
		core::UUID node{0};
		/// Whether it is the pin of the children (else the pin of the parent).
		bool children{false};
	};

	void onCanvasReady() override;

	void rebuildCanvas();

	void renderToolbar();

	void renderGraphMenu();

	void renderPalette(std::array<float, 2> iPosition, bool iPopup);

	void renderNodeParameters();

	void renderBlackboard();

	void addNode(const std::string& iType, std::array<float, 2> iPosition);

	void deleteNodes(std::span<const core::UUID> iNodes);

	void duplicateNodes(std::span<const core::UUID> iNodes);

	void commitMove();

	void drawFooter(core::UUID iNode, math::vec2f iMin, math::vec2f iMax, float iZoom) const;

	/**
	 * @brief
	 *  The agent of the playing scene whose run is drawn.
	 * @return Its run, null when no agent runs this tree.
	 */
	[[nodiscard]] auto findWatched() const -> shared<scene::ai::BehaviourTreeInstance>;

	/// The edited tree.
	scene::ai::BehaviourTree m_tree;
	/// Undo stack of the edits.
	BehaviourTreeUndoManager m_undo;
	/// Text of the tree at the last save or load.
	std::string m_savedText;
	/// Canvas pins by id.
	std::map<core::UUID, PinRef> m_pins;
	/// Canvas pin of the parent side of each node.
	std::map<core::UUID, core::UUID> m_parentPins;
	/// Canvas pin of the children side of each node.
	std::map<core::UUID, core::UUID> m_childPins;
	/// Node shown in the Node panel.
	std::optional<core::UUID> m_selectedNode;
	/// Node the canvas menu was opened on.
	std::optional<core::UUID> m_menuNode;
	/// Canvas position the menu adds nodes at.
	std::array<float, 2> m_menuPosition{0.f, 0.f};
	/// Whether the canvas asked for its menu this frame.
	bool m_openMenu{false};
	/// Whether nodes are being dragged (their move is committed on release).
	bool m_moving{false};
	/// Whether the canvas used this frame for a link edit (the Delete key is then not for the nodes).
	bool m_linkDeleted{false};
	/// Filter of the node library.
	std::string m_paletteFilter;
	/// Name typed for a new blackboard key.
	std::string m_newKey;
	/// Kind of a new blackboard key.
	scene::ai::BlackboardType m_newKeyType{scene::ai::BlackboardType::Number};
	/// Problems of the tree, refreshed after each edit.
	std::vector<std::string> m_problems;
	/// The run drawn on the nodes this frame.
	shared<scene::ai::BehaviourTreeInstance> m_watched;
	/// Frames left during which the canvas frames every node.
	uint32_t m_fitFrames{3};
};

}// namespace owl::nest
