/**
 * @file VisualScriptDocument.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"
#include "VisualScriptCanvas.h"

#include <script/visual/VisualScript.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

class TextEditor;

namespace owl::nest {

/**
 * @brief
 *  Undo manager of a visual script: every edit stores the graph before and after.
 */
using VisualScriptUndoManager = UndoManager<script::visual::VisualScript>;

/**
 * @brief
 *  What the visual script document shows.
 */
enum struct VisualScriptViewMode : uint8_t {
	Graph,///< The nodes.
	Split,///< The nodes and the generated Lua side by side.
	Code///< The generated Lua.
};

/**
 * @brief
 *  Visual script document (`.owlvs`): an event graph on the node canvas, compiled to Lua as it is edited.
 *
 * The `script::visual::VisualScript` is the document, drawn by a `VisualScriptCanvas` and compiled after each edit.
 * A right click on the canvas opens the node palette; the Hierarchy panel lists every node to drag onto the canvas,
 * the Properties panel (also shown on the left of the canvas on demand) edits the variables and the selected node.
 * The toolbar shows the graph, the generated Lua (coloured, read-only) or both side by side.
 */
class VisualScriptDocument final : public NodeGraphDocument {
public:
	VisualScriptDocument(const VisualScriptDocument&) = delete;

	VisualScriptDocument(VisualScriptDocument&&) = delete;

	auto operator=(const VisualScriptDocument&) -> VisualScriptDocument& = delete;

	auto operator=(VisualScriptDocument&&) -> VisualScriptDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	VisualScriptDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~VisualScriptDocument() override;

	/**
	 * @brief
	 *  Tab title displayed in the document tab bar.
	 * @return The file name, or `Untitled`.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  Whether the graph changed since the last save.
	 * @return True when the document is dirty.
	 */
	[[nodiscard]] auto isDirty() const -> bool override;

	/**
	 * @brief
	 *  Render the toolbar, the canvas and the details panel when shown.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Attach to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  The graph replaces the scene panels: node library in the Hierarchy, details in the Properties.
	 * @return Always true.
	 */
	[[nodiscard]] auto overridesGlobalPanels() const -> bool override { return true; }

	/**
	 * @brief
	 *  Node library: every node, by group, to click or drag onto the canvas.
	 */
	void renderHierarchyPanel() override;

	/**
	 * @brief
	 *  Variables and selected node tabs.
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
	 * @return `Graph`.
	 */
	[[nodiscard]] auto propertiesPanelTitle() const -> std::string override { return "Graph"; }

	/**
	 * @brief
	 *  Save the graph to an explicit path.
	 * @param[in] iPath Destination file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Load a graph from disk.
	 * @param[in] iPath The `.owlvs` file.
	 * @return True on success.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Current graph, serialized.
	 * @return The YAML text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Replace the graph with an autosaved one, keeping the path; the document becomes dirty.
	 * @param[in] iSnapshot The serialized graph.
	 * @return True when the snapshot was applied.
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
	 * @return The visual script.
	 */
	[[nodiscard]] auto getScript() const -> const script::visual::VisualScript& { return m_script; }

	/**
	 * @brief
	 *  Undo history of the graph.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto getUndoManager() const -> const VisualScriptUndoManager& { return m_undo; }

	/**
	 * @brief
	 *  Replace the graph as one undoable edit.
	 * @param[in] iNext The new graph.
	 * @param[in] iDescription Undo entry text; consecutive edits with the same text merge into one.
	 * @param[in] iMerge Whether the edit may merge with the previous one of the same text.
	 * @return True when the graph changed.
	 */
	auto commit(script::visual::VisualScript iNext, const std::string& iDescription, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Add a node of a palette type, linked to a pin when one is given.
	 * @param[in] iType Node type identifier; `var.get|name` and `var.set|name` name the variable.
	 * @param[in] iPosition Top-left corner on the canvas.
	 * @param[in] iLinkFrom Pin to link to the first compatible pin of the new node.
	 * @return The new node, or 0 when the type is unknown.
	 */
	auto addNode(const std::string& iType, std::array<float, 2> iPosition,
				 const std::optional<VisualScriptPinRef>& iLinkFrom = std::nullopt) -> core::UUID;

	/**
	 * @brief
	 *  Show a grouped pin (`rgba`, `xyz`) as its components or as one pin, as one edit; its links are removed and
	 *  its value is carried over.
	 * @param[in] iNode The node.
	 * @param[in] iGroup Group name.
	 * @param[in] iSplit True to show the components.
	 */
	void setPinSplit(core::UUID iNode, const std::string& iGroup, bool iSplit);

	/**
	 * @brief
	 *  What the document shows.
	 * @return The view mode.
	 */
	[[nodiscard]] auto getViewMode() const -> VisualScriptViewMode { return m_viewMode; }

	/**
	 * @brief
	 *  Show the graph, the generated Lua or both.
	 * @param[in] iMode The view mode.
	 */
	void setViewMode(const VisualScriptViewMode iMode) { m_viewMode = iMode; }

	/**
	 * @brief
	 *  Selected nodes.
	 * @return Their identifiers.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<core::UUID>& { return m_view.getSelection(); }

	/**
	 * @brief
	 *  Replace the selected nodes.
	 * @param[in] iNodes The nodes.
	 */
	void setSelection(std::vector<core::UUID> iNodes) { m_view.setSelection(std::move(iNodes)); }

	/**
	 * @brief
	 *  Delete nodes and their links, as one edit.
	 * @param[in] iNodes The nodes.
	 */
	void deleteNodes(std::span<const core::UUID> iNodes);

	/**
	 * @brief
	 *  Copy nodes and the links between them, as a graph text.
	 * @param[in] iNodes The nodes.
	 * @return The YAML of the partial graph.
	 */
	[[nodiscard]] auto copyNodes(std::span<const core::UUID> iNodes) const -> std::string;

	/**
	 * @brief
	 *  Paste copied nodes with fresh identifiers, as one edit; they become the selection.
	 * @param[in] iText Text from `copyNodes`.
	 * @param[in] iOffset Offset added to every pasted position.
	 * @return The pasted nodes.
	 */
	auto pasteNodes(const std::string& iText, std::array<float, 2> iOffset) -> std::vector<core::UUID>;

	/**
	 * @brief
	 *  Errors of the last compilation.
	 * @return The diagnostics, empty when the graph compiles.
	 */
	[[nodiscard]] auto getDiagnostics() const -> const std::vector<script::visual::CompileDiagnostic>& {
		return m_diagnostics;
	}

	/**
	 * @brief
	 *  Lua generated by the last successful compilation.
	 * @return The Lua source.
	 */
	[[nodiscard]] auto getCompiledLua() const -> const std::string& { return m_compiledLua; }

	/**
	 * @brief
	 *  A new graph: an `On Create` and an `On Update` event.
	 * @return The starter graph.
	 */
	[[nodiscard]] static auto starterScript() -> script::visual::VisualScript;

private:
	/**
	 * @brief
	 *  Compile the graph when it changed.
	 */
	void compileIfStale();

	/**
	 * @brief
	 *  Edit shortcuts of the canvas: Delete, Home, Ctrl+A / C / X / V / D.
	 * @param[in] iDeleteUsed Whether the canvas used Delete on a link this frame.
	 */
	void handleShortcuts(bool iDeleteUsed);

	/**
	 * @brief
	 *  Toolbar above the canvas: details panel, framing, view mode, compilation status.
	 */
	void renderToolbar();

	/**
	 * @brief
	 *  Context menu: the palette on the background, node actions on a node, split and links on a pin.
	 */
	void renderContextMenu();

	/**
	 * @brief
	 *  Nodes by group, with the search field.
	 * @param[in] iPopup Whether it is the right-click palette (click adds at the cursor) or the library panel
	 * (click adds in the view, drag drops).
	 */
	void renderNodeList(bool iPopup);

	/**
	 * @brief
	 *  Variables and node tabs, in the Properties panel or the details panel.
	 */
	void renderDetails();

	/**
	 * @brief
	 *  Variable list.
	 */
	void renderVariables();

	/**
	 * @brief
	 *  Settings, grouped pins and unlinked input values of the selected node.
	 */
	void renderSelectedNode();

	/**
	 * @brief
	 *  Set or clear a value of a node, as one edit merged with the next of the same kind.
	 * @param[in] iNode The node.
	 * @param[in] iKey Pin name or setting.
	 * @param[in] iValue The value; empty clears it.
	 */
	void setNodeValue(core::UUID iNode, const std::string& iKey, const std::string& iValue);

	/**
	 * @brief
	 *  Title section of the selected node: description, errors, variable or function name.
	 * @param[in] iNode The node.
	 * @param[in] iType Its type.
	 */
	void renderNodeHeader(const script::visual::GraphNode& iNode, const script::visual::NodeType& iType);

	/**
	 * @brief
	 *  Split / recombine buttons of the grouped pins of the selected node.
	 * @param[in] iNode The node.
	 */
	void renderGroupedPins(const script::visual::GraphNode& iNode);

	/**
	 * @brief
	 *  Values of the unlinked inputs of the selected node.
	 * @param[in] iNode The node.
	 */
	void renderNodeInputs(const script::visual::GraphNode& iNode);

	/**
	 * @brief
	 *  The generated Lua, read-only and coloured.
	 */
	void renderLua();

	/// The graph.
	script::visual::VisualScript m_script;
	/// Undo history.
	VisualScriptUndoManager m_undo;
	/// Graph view.
	VisualScriptCanvas m_view;
	/// Errors of the last compilation.
	std::vector<script::visual::CompileDiagnostic> m_diagnostics;
	/// Lua of the last successful compilation.
	std::string m_compiledLua;
	/// Graph text at the last save.
	std::string m_savedText;
	/// Search text of the palette and of the library.
	std::string m_paletteFilter;
	/// Copied nodes in the clipboard when the palette opened, empty when there are none.
	std::string m_paletteClipboard;
	/// Canvas position where the palette adds a node.
	std::array<float, 2> m_paletteTarget{0.f, 0.f};
	/// Node under the cursor at the last right click.
	std::optional<core::UUID> m_contextNode;
	/// Pin under the cursor at the last right click.
	std::optional<VisualScriptPinRef> m_contextPin;
	/// Pin whose link was dropped on the background: the palette links the new node to it.
	std::optional<VisualScriptPinRef> m_pendingLink;
	/// Open the context menu on the next frame.
	bool m_openContextMenu{false};
	/// The graph changed since the last compilation.
	bool m_compileStale{true};
	/// Show the details panel on the left of the canvas.
	bool m_showDetails{false};
	/// What the document shows.
	VisualScriptViewMode m_viewMode{VisualScriptViewMode::Graph};
	/// Read-only Lua view.
	uniq<TextEditor> mp_luaView;
	/// Text shown by the Lua view.
	std::string m_luaViewText;
	/// Counts the pastes, to shift each one a little more.
	uint32_t m_pasteCount{0};
};

}// namespace owl::nest
