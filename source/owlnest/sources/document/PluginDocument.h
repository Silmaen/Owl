/**
 * @file PluginDocument.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/PluginCommands.h"
#include "../panel/PluginUiRenderer.h"
#include "Document.h"
#include "UndoManager.h"

#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest {

/**
 * @brief
 *  Editor tab of a file whose document type an Owl Nest plugin declares (`nest.document`).
 *
 * The document holds the text of the file; the handler of the plugin draws it through the plugin widgets and changes
 * it with the `plugin.document_set` command, one step of the undo history of the tab.
 */
class PluginDocument final : public Document, public commands::PluginDocumentEditor {
public:
	PluginDocument(const PluginDocument&) = delete;

	PluginDocument(PluginDocument&&) = delete;

	auto operator=(const PluginDocument&) -> PluginDocument& = delete;

	auto operator=(PluginDocument&&) -> PluginDocument& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iType Id of the plugin document type (PluginContribution::id).
	 */
	explicit PluginDocument(std::string iType);

	/**
	 * @brief
	 *  Destructor.
	 */
	~PluginDocument() override;

	/**
	 * @brief
	 *  Document kind.
	 * @return `DocumentType::Plugin`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Plugin; }

	/**
	 * @brief
	 *  Displayed title on the tab.
	 * @return The file name.
	 */
	[[nodiscard]] auto title() const -> std::string override;

	/**
	 * @brief
	 *  On-disk path.
	 * @return The file of the document.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  True when the document has unsaved changes.
	 * @return Whether the text differs from the file.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Called once when the document is inserted into the manager.
	 * @param[in] iEditor The editor.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Called once when the document is being removed.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update tick (nothing to advance).
	 * @param[in] iTimeStep Time since the previous frame.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event dispatch (nothing handled).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the tab: the widgets of the plugin handler.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Read the text of a file.
	 * @param[in] iPath The file.
	 * @return False when the file cannot be read.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Save to the current file path.
	 * @return False on failure or if no path is set.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save to an explicit path.
	 * @param[in] iPath The file written.
	 * @return False on failure.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Current text, for the autosave.
	 * @return The text.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override { return m_text; }

	/**
	 * @brief
	 *  Replace the text with an autosaved one; the document becomes dirty.
	 * @param[in] iSnapshot Text returned by `recoverySnapshot()` in a previous session.
	 * @return True.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Placeholder scene history: the document has no scene.
	 * @return An empty undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Placeholder scene history: the document has no scene.
	 * @return An empty undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether a text edit can be undone.
	 * @return True when the history holds an edit.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether a text edit can be redone.
	 * @return True when an edit was undone.
	 */
	[[nodiscard]] auto canRedo() const -> bool override { return m_undo.canRedo(); }

	/**
	 * @brief
	 *  Undo the last text edit.
	 */
	void performUndo() override;

	/**
	 * @brief
	 *  Redo the last text edit undone.
	 */
	void performRedo() override;

	/**
	 * @brief
	 *  The text of the document as it is.
	 * @return The text.
	 */
	[[nodiscard]] auto getText() const -> const std::string& override { return m_text; }

	/**
	 * @brief
	 *  Replace the text as one undoable step.
	 * @param[in] iText The text after the edit.
	 * @param[in] iDescription Name of the step in the history.
	 * @return False when nothing changes.
	 */
	auto setText(const std::string& iText, const std::string& iDescription) -> bool override;

	/**
	 * @brief
	 *  Id of the plugin document type.
	 * @return The id.
	 */
	[[nodiscard]] auto getPluginType() const -> const std::string& { return m_type; }

	/**
	 * @brief
	 *  Whether the tab is open (false after a click on its close button).
	 * @return The open state.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Set the open state of the tab (a cancelled close puts it back).
	 * @param[in] iOpen The open state.
	 */
	void setOpen(const bool iOpen) { m_open = iOpen; }

private:
	/// Id of the plugin document type.
	std::string m_type;
	/// The file.
	std::filesystem::path m_path;
	/// The text.
	std::string m_text;
	/// History of the text edits.
	UndoManager<std::string> m_undo;
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
	/// Draws the widgets of the plugin.
	panel::PluginUiRenderer m_ui;
	/// The editor (non-owning).
	EditorLayer* mp_editorLayer = nullptr;
	/// Whether the tab is open.
	bool m_open = true;
	/// Whether the window had the focus on the previous frame.
	bool m_wasFocused = false;
};

}// namespace owl::nest
