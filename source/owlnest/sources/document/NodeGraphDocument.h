/**
 * @file NodeGraphDocument.h
 * @author Silmaen
 * @date 24/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include <gui/widgets/NodeCanvas.h>

namespace owl::nest {
/**
 * @brief
 *  Undo manager typed for node-graph edits (nodes, pins, links on a `NodeCanvas`).
 */
using NodeGraphUndoManager = UndoManager<gui::widgets::NodeCanvas>;

/**
 * @brief
 *  Node-graph document (`.owlflow`), the third kind of document alongside Scene and Code.
 *
 * Owns a `gui::widgets::NodeCanvas` and a `NodeGraphUndoManager` typed over it. Subclasses
 * (e.g. `SceneFlowDocument`, future animation graph / behaviour tree) specialize the canvas
 * contents and the callbacks wired at attach time; this base class handles save/load, dirty
 * tracking and the ImGui window boilerplate.
 *
 * The generic `Document::undoManager()` returns an empty `SceneUndoManager` here — node graph
 * undo goes through the typed `nodeGraphUndoManager()` accessor so panels that know they are
 * editing a node graph can push `UndoCommand<NodeCanvas>` instances without downcasting.
 */
class NodeGraphDocument : public Document {
public:
	NodeGraphDocument(const NodeGraphDocument&) = delete;

	NodeGraphDocument(NodeGraphDocument&&) = delete;

	auto operator=(const NodeGraphDocument&) -> NodeGraphDocument& = delete;

	auto operator=(NodeGraphDocument&&) -> NodeGraphDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	NodeGraphDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~NodeGraphDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return The document type.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::NodeGraph; }

	/**
	 * @brief
	 *  Tab title displayed in the document tab bar.
	 * @return The display title.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  On-disk path of the document.
	 * @return The document file path (empty if never saved).
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the document has unsaved changes.
	 * @return True when the document is dirty.
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
	 *  Called once when the document is being removed from the manager.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update tick.
	 * @param[in] iTimeStep Frame time delta.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Dispatch an input/system event to the document.
	 * @param[in,out] ioEvent The incoming event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Render the document's panels in the current ImGui frame.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save the document to its current file path.
	 * @return True on success; false on failure or when no path is set.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save the document to an explicit path.
	 * @param[in] iPath Destination file.
	 * @return True on success; false on failure.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Empty scene undo manager — node-graph edits use `nodeGraphUndoManager()`.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	// --- Node-graph-specific API ---------------------------------------------
	/**
	 * @brief
	 *  Typed undo manager for `gui::widgets::NodeCanvas` operations.
	 */
	[[nodiscard]] auto nodeGraphUndoManager() -> NodeGraphUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the typed undo manager for `gui::widgets::NodeCanvas` operations.
	 * @return The node-graph undo manager.
	 */
	[[nodiscard]] auto nodeGraphUndoManager() const -> const NodeGraphUndoManager& { return m_undoManager; }

	/**
	 * @brief
	 *  The canvas owned by this document. Subclasses fill it on attach.
	 */
	[[nodiscard]] auto canvas() -> gui::widgets::NodeCanvas& { return m_canvas; }

	/**
	 * @brief
	 *  Const access to the canvas owned by this document.
	 * @return The node canvas.
	 */
	[[nodiscard]] auto canvas() const -> const gui::widgets::NodeCanvas& { return m_canvas; }

	/**
	 * @brief
	 *  Load a node graph from disk. Returns false on I/O or parse error.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  True while the user hasn't clicked the tab's close X.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

protected:
	/**
	 * @brief
	 *  Hook for subclasses to install canvas callbacks (called at the end of `onAttach`).
	 */
	virtual void onCanvasReady() {}

	/// Disk path of the YAML node-graph file.
	std::filesystem::path m_path;
	/// Node canvas widget the document edits.
	gui::widgets::NodeCanvas m_canvas;
	/// Per-document undo/redo manager for graph edits.
	NodeGraphUndoManager m_undoManager;
	/// YAML snapshot at last save — used by `isDirty()` for a cheap whole-document comparison.
	std::string m_savedSnapshot;
	/// Editor layer back-pointer (for menus, dialogs, status messages).
	EditorLayer* mp_editorLayer = nullptr;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;

private:
	/// Unused placeholder to satisfy the Document::undoManager() contract.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
