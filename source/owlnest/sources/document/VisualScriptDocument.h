/**
 * @file VisualScriptDocument.h
 * @author Silmaen
 * @date 27/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "NodeGraphDocument.h"

#include <script/visual/VisualScript.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Undo manager of a visual script: every edit stores the graph before and after.
 */
using VisualScriptUndoManager = UndoManager<script::visual::VisualScript>;

/**
 * @brief
 *  Visual script document (`.owlvs`): an event graph on the node canvas, compiled to Lua as it is edited.
 *
 * The `script::visual::VisualScript` is the document; the canvas is rebuilt from it after each edit. A right click
 * on the canvas opens the node palette, the side panel edits the variables, the values of the selected node and
 * shows the compilation errors and the generated Lua.
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
	 *  Render the canvas and the side panel.
	 */
	void onImGuiRender() override;

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
	 *  Add a node of a palette type.
	 * @param[in] iType Node type identifier.
	 * @param[in] iPosition Top-left corner on the canvas.
	 * @return The new node, or 0 when the type is unknown.
	 */
	auto addNode(const std::string& iType, std::array<float, 2> iPosition) -> core::UUID;

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

protected:
	/**
	 * @brief
	 *  Install the canvas callbacks.
	 */
	void onCanvasReady() override;

private:
	/// A pin of the canvas, by node and name.
	struct PinRef {
		/// Owning node.
		core::UUID node;
		/// Pin name.
		std::string name;
		/// Whether it is an output.
		bool output{false};
		/// Carried type.
		script::visual::PinType type{script::visual::PinType::Any};
	};

	/**
	 * @brief
	 *  Rebuild the canvas from the graph, keeping the selection, and compile.
	 */
	void rebuildCanvas();

	/**
	 * @brief
	 *  Whether a link may join two canvas pins.
	 * @param[in] iFrom Output pin.
	 * @param[in] iTo Input pin.
	 * @return True when allowed.
	 */
	[[nodiscard]] auto isLinkAllowed(core::UUID iFrom, core::UUID iTo) const -> bool;

	/**
	 * @brief
	 *  Edit shortcuts of the canvas: Delete, Ctrl+A / C / X / V / D.
	 */
	void handleShortcuts();

	/**
	 * @brief
	 *  Context menu: the node palette on the background, node actions on a node.
	 */
	void renderContextMenu();

	/**
	 * @brief
	 *  Node palette with its search field.
	 */
	void renderPalette();

	/**
	 * @brief
	 *  Side panel: variables, selected node, compilation.
	 */
	void renderSidePanel();

	/**
	 * @brief
	 *  Variable list of the side panel.
	 */
	void renderVariables();

	/**
	 * @brief
	 *  Settings and unlinked input values of the selected node.
	 */
	void renderSelectedNode();

	/**
	 * @brief
	 *  Compilation status and generated Lua.
	 */
	void renderCompilation();

	/// The graph.
	script::visual::VisualScript m_script;
	/// Undo history.
	VisualScriptUndoManager m_undo;
	/// Canvas pins by pin identifier.
	std::unordered_map<uint64_t, PinRef> m_pins;
	/// Graph links by canvas link identifier.
	std::unordered_map<uint64_t, script::visual::GraphLink> m_links;
	/// Errors of the last compilation.
	std::vector<script::visual::CompileDiagnostic> m_diagnostics;
	/// Lua of the last successful compilation.
	std::string m_compiledLua;
	/// Graph text at the last save.
	std::string m_savedText;
	/// Search text of the palette.
	std::string m_paletteFilter;
	/// Canvas position where the palette adds a node.
	std::array<float, 2> m_paletteTarget{0.f, 0.f};
	/// Node under the cursor at the last right click.
	std::optional<core::UUID> m_contextNode;
	/// Open the context menu on the next frame.
	bool m_openContextMenu{false};
	/// The canvas no longer matches the graph.
	bool m_canvasStale{true};
	/// Show the generated Lua in the side panel.
	bool m_showLua{false};
	/// Selection to apply at the next canvas rebuild (nodes just added or pasted).
	std::optional<std::vector<core::UUID>> m_pendingSelection;
	/// Counts the pastes, to shift each one a little more.
	uint32_t m_pasteCount{0};
};

}// namespace owl::nest
