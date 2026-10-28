/**
 * @file PcgGraphDocument.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"
#include "UndoManager.h"

#include <core/UUID.h>
#include <scene/pcg/PcgGraph.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Undo manager typed for procedural generation graph edits.
 */
using PcgGraphUndoManager = UndoManager<scene::pcg::Graph>;

/**
 * @brief
 *  Document editing a procedural generation graph (`.owlpcg`) on the node canvas.
 *
 * The graph (`scene::pcg::Graph`) is the truth, the canvas a view rebuilt from it after each edit; every edit is one
 * undo entry holding the graph before and after. The Nodes panel lists the node types (click to add) and the preview
 * settings; the Graph panel edits the parameters of the selected node and previews its output (a field in grey, the
 * points in white, the tiles of the whole graph in colours), regenerated from the preview seed after every change.
 */
class PcgGraphDocument final : public NodeGraphDocument {
public:
	PcgGraphDocument(const PcgGraphDocument&) = delete;

	PcgGraphDocument(PcgGraphDocument&&) = delete;

	auto operator=(const PcgGraphDocument&) -> PcgGraphDocument& = delete;

	auto operator=(PcgGraphDocument&&) -> PcgGraphDocument& = delete;

	/**
	 * @brief
	 *  Default constructor: an empty graph.
	 */
	PcgGraphDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~PcgGraphDocument() override;

	/**
	 * @brief
	 *  Starter graph written by the Content Browser: noise, a threshold and a tile output.
	 * @return The graph.
	 */
	[[nodiscard]] static auto starterGraph() -> scene::pcg::Graph;

	/**
	 * @brief
	 *  Whether the graph differs from the saved file.
	 * @return True when dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Called once when the document is attached to the manager.
	 * @param[in] iEditor Owning editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Render the canvas tab.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Whether the document repurposes the global panels.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Render the Nodes panel: node types and preview settings.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Render the Graph panel: the selected node and the preview.
	 */
	void renderPropertiesPanel() override;

	/**
	 * @brief
	 *  Title of the hierarchy panel while this document is active.
	 * @return "Nodes".
	 */
	[[nodiscard]] auto hierarchyPanelTitle() const -> std::string override { return "Nodes"; }

	/**
	 * @brief
	 *  Title of the properties panel while this document is active.
	 * @return "Graph".
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Graph"; }

	/**
	 * @brief
	 *  Save the graph to a path.
	 * @param[in] iPath Destination file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Load a graph file.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Current graph, serialized.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Replace the graph by an autosaved one, keeping the path; the document becomes dirty.
	 * @param[in] iSnapshot The serialized graph.
	 * @return True when applied.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the undo stack is not empty.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when the redo stack is not empty.
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
	 *  The edited graph.
	 * @return The graph.
	 */
	[[nodiscard]] auto getGraph() const -> const scene::pcg::Graph& { return m_graph; }

	/**
	 * @brief
	 *  Replace the graph as one undo entry.
	 * @param[in] iNext The graph after the edit.
	 * @param[in] iDescription The undo entry.
	 * @param[in] iMerge Whether it merges with the previous entry of the same text (slider drags).
	 * @return False when nothing changed.
	 */
	auto commit(scene::pcg::Graph iNext, const std::string& iDescription, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Add a node.
	 * @param[in] iType Node type identifier.
	 * @param[in] iPosition Top-left corner on the canvas.
	 * @return The new node, none for an unknown type.
	 */
	auto addNode(const std::string& iType, std::array<float, 2> iPosition) -> std::optional<core::UUID>;

	/**
	 * @brief
	 *  Link an output to an input of the same type, replacing the link the input had.
	 * @param[in] iFromNode Source node.
	 * @param[in] iFromPin Output pin name.
	 * @param[in] iToNode Destination node.
	 * @param[in] iToPin Input pin name.
	 * @return False for unknown pins, different types or a link to the node itself.
	 */
	auto connect(core::UUID iFromNode, const std::string& iFromPin, core::UUID iToNode, const std::string& iToPin)
			-> bool;

	/**
	 * @brief
	 *  Set a parameter of a node.
	 * @param[in] iNode The node.
	 * @param[in] iKey The parameter.
	 * @param[in] iValue The value as text.
	 * @param[in] iMerge Whether it merges with the previous edit of the parameter.
	 */
	void setNodeValue(core::UUID iNode, const std::string& iKey, const std::string& iValue, bool iMerge = false);

	/**
	 * @brief
	 *  Delete nodes and their links (one undo entry).
	 * @param[in] iNodes The nodes.
	 */
	void deleteNodes(std::span<const core::UUID> iNodes);

	/**
	 * @brief
	 *  Select a node: its parameters and output in the Graph panel.
	 * @param[in] iNode The node, none for the whole graph.
	 */
	void selectNode(std::optional<core::UUID> iNode);

	/**
	 * @brief
	 *  Node shown in the Graph panel.
	 * @return The node, none for the whole graph.
	 */
	[[nodiscard]] auto getSelectedNode() const -> const std::optional<core::UUID>& { return m_selectedNode; }

	/**
	 * @brief
	 *  Size and seed of the preview.
	 * @param[in] iSize Size in cells (each side clamped to [1, 256]).
	 * @param[in] iSeed Seed.
	 */
	void setPreview(const math::vec2ui& iSize, uint64_t iSeed);

	/**
	 * @brief
	 *  Result of the graph on the preview region, evaluated again when the graph or the settings changed.
	 * @return The result, none when the graph cannot be evaluated (a cycle).
	 */
	auto getPreview() -> const std::optional<scene::pcg::Result>&;

private:
	/// A pin of the canvas: its node, name, side and type.
	struct PinRef {
		core::UUID node{0};
		std::string pin;
		bool output{false};
		scene::pcg::PinType type{scene::pcg::PinType::Field};
	};

	/**
	 * @brief
	 *  Install the canvas callbacks.
	 */
	void onCanvasReady() override;

	/**
	 * @brief
	 *  Rebuild the canvas from the graph, keeping the selection.
	 */
	void rebuildCanvas();

	/**
	 * @brief
	 *  Palette of node types, by category.
	 * @param[in] iPosition Where a clicked type is added.
	 */
	void renderPalette(std::array<float, 2> iPosition);

	/**
	 * @brief
	 *  Context menu of the canvas: add a node, delete the selection.
	 */
	void renderGraphMenu();

	/**
	 * @brief
	 *  Draw a pin value or the tiles of the graph as a grid of cells.
	 */
	void renderPreview();

	/// The edited graph.
	scene::pcg::Graph m_graph;
	/// Undo stack of the graph edits.
	PcgGraphUndoManager m_undo;
	/// Text of the graph at the last save or load.
	std::string m_savedText;
	/// Canvas pins by id.
	std::map<core::UUID, PinRef> m_pins;
	/// Canvas pin ids by node, side and name.
	std::map<std::tuple<core::UUID, bool, std::string>, core::UUID> m_pinIds;
	/// Node shown in the Graph panel.
	std::optional<core::UUID> m_selectedNode;
	/// Node the canvas menu was opened on.
	std::optional<core::UUID> m_menuNode;
	/// Canvas position the menu adds nodes at.
	std::array<float, 2> m_menuPosition{0.f, 0.f};
	/// Whether the canvas asked for its menu this frame.
	bool m_openMenu{false};
	/// Whether nodes are being dragged (their positions are committed on release).
	bool m_moving{false};
	/// Whether a link was deleted this frame (the Delete key is then not for the nodes).
	bool m_linkDeleted{false};
	/// Size of the preview region.
	math::vec2ui m_previewSize{48, 32};
	/// Seed of the preview.
	uint64_t m_previewSeed{1};
	/// Result on the preview region.
	std::optional<scene::pcg::Result> m_preview;
	/// Whether the preview must be evaluated again.
	bool m_previewStale{true};
};

}// namespace owl::nest
