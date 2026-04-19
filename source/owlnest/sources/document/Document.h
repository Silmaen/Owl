/**
 * @file Document.h
 * @author Silmaen
 * @date 18/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {

class EditorLayer;
class UndoManager;

/// @brief Kind of editable document opened in a tab.
enum struct DocumentType : uint8_t {
	Scene,///< A `.owl` scene edited in the Viewport.
	// Future: LuaScript, NodeGraph, Animation...
};

/**
 * @brief Abstract base class for an editable document (scene, script, graph...).
 *
 * Each `Document` owns the state bound to one editable artifact: its file path,
 * its dirty flag, its undo/redo stack, and any per-document panels
 * (e.g. a `Viewport` for a scene). The editor holds a `DocumentManager` that
 * lists the open documents and tracks which one is currently active. Global
 * panels (scene hierarchy, inspector) show data from the active document.
 */
class Document {
public:
	Document(const Document&) = delete;
	Document(Document&&) = delete;
	auto operator=(const Document&) -> Document& = delete;
	auto operator=(Document&&) -> Document& = delete;
	Document() = default;
	virtual ~Document();

	/// @brief Unique id for this document (stable across its lifetime).
	[[nodiscard]] auto id() const -> core::UUID { return m_id; }

	/// @brief Document kind (scene, lua script...).
	[[nodiscard]] virtual auto type() const -> DocumentType = 0;

	/// @brief Displayed title on the tab (without dirty marker).
	[[nodiscard]] virtual auto title() const -> std::string = 0;

	/// @brief On-disk path, empty if untitled/never saved.
	[[nodiscard]] virtual auto filePath() const -> std::filesystem::path = 0;

	/// @brief True when the document has unsaved changes.
	[[nodiscard]] virtual auto isDirty() const -> bool = 0;

	/// @brief Called once when the document is inserted into the manager.
	virtual void onAttach(EditorLayer* iEditor) = 0;
	/// @brief Called once when the document is being removed.
	virtual void onDetach() = 0;

	/// @brief Per-frame update tick (scene logic, runtime update...).
	virtual void onUpdate(const core::Timestep& iTimeStep) = 0;
	/// @brief Event dispatch (keyboard, mouse, file drop).
	virtual void onEvent(event::Event& ioEvent) = 0;
	/// @brief Per-frame ImGui rendering of the document's own panels (e.g. viewport).
	virtual void onImGuiRender() = 0;

	/// @brief Save to the current file path. Returns false on failure or if no path is set.
	virtual auto save() -> bool = 0;
	/// @brief Save to an explicit path. Returns false on failure.
	virtual auto saveAs(const std::filesystem::path& iPath) -> bool = 0;

	/// @brief Access to the document's undo/redo stack.
	[[nodiscard]] virtual auto undoManager() -> UndoManager& = 0;
	[[nodiscard]] virtual auto undoManager() const -> const UndoManager& = 0;

private:
	/// Stable identifier used by the UI (tab id, ImGui window suffix).
	core::UUID m_id;
};

}// namespace owl::nest
