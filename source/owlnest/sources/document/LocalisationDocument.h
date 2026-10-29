/**
 * @file LocalisationDocument.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"

#include <data/localisation/Localisation.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/// Undo stack of a localisation.
using LocalisationUndoManager = UndoManager<data::localisation::Localisation>;

/**
 * @brief
 *  Localisation document (`localisation.yml`): the languages of the game and its string tables, in a grid of one
 *  row per key and one column per language.
 */
class LocalisationDocument final : public Document {
public:
	LocalisationDocument(const LocalisationDocument&) = delete;

	LocalisationDocument(LocalisationDocument&&) = delete;

	auto operator=(const LocalisationDocument&) -> LocalisationDocument& = delete;

	auto operator=(LocalisationDocument&&) -> LocalisationDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	LocalisationDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~LocalisationDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document type.
	 * @return `DocumentType::Localisation`.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Localisation; }

	/**
	 * @brief
	 *  Title of the tab.
	 * @return "Localisation".
	 */
	[[nodiscard]] auto title() const -> std::string override { return "Localisation"; }

	/**
	 * @brief
	 *  File of the document.
	 * @return The path of `localisation.yml`.
	 */
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }

	/**
	 * @brief
	 *  Whether the localisation changed since the last save.
	 * @return True when unsaved changes exist.
	 */
	[[nodiscard]] auto isDirty() const -> bool override { return m_undo.isDirty(); }

	/**
	 * @brief
	 *  Attach the document to the editor.
	 * @param[in] iEditor The editor layer.
	 */
	void onAttach(EditorLayer* iEditor) override;

	/**
	 * @brief
	 *  Detach the document from the editor.
	 */
	void onDetach() override;

	/**
	 * @brief
	 *  Per-frame update (nothing to do).
	 * @param[in] iTimeStep Frame time.
	 */
	void onUpdate(const core::Timestep& iTimeStep) override;

	/**
	 * @brief
	 *  Event handler (no event used).
	 * @param[in,out] ioEvent The event.
	 */
	void onEvent(event::Event& ioEvent) override;

	/**
	 * @brief
	 *  Draw the document window.
	 */
	void onImGuiRender() override;

	/**
	 * @brief
	 *  Save to the document's file; the editor's texts follow it.
	 * @return True on success.
	 */
	auto save() -> bool override;

	/**
	 * @brief
	 *  Save to a file, which becomes the document's file.
	 * @param[in] iPath The file.
	 * @return True on success.
	 */
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	/**
	 * @brief
	 *  Unsaved state, for the autosave.
	 * @return The localisation as YAML.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Restore an autosave.
	 * @param[in] iSnapshot A `recoverySnapshot()` text.
	 * @return True when it was a localisation.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Scene undo stack, unused by this document.
	 * @return An empty stack.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Scene undo stack, unused by this document.
	 * @return An empty stack.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_emptySceneUndo; }

	/**
	 * @brief
	 *  Whether an edit can be undone.
	 * @return True when the stack holds one.
	 */
	[[nodiscard]] auto canUndo() const -> bool override { return m_undo.canUndo(); }

	/**
	 * @brief
	 *  Whether an edit can be redone.
	 * @return True when the stack holds one.
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

	// --- Localisation API ----------------------------------------------------
	/**
	 * @brief
	 *  The localisation being edited.
	 * @return The localisation.
	 */
	[[nodiscard]] auto localisation() const -> const data::localisation::Localisation& { return m_localisation; }

	/**
	 * @brief
	 *  Load a `localisation.yml`.
	 * @param[in] iPath The file.
	 * @return False when it is not a localisation.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Start a localisation not yet on disk: English only, written by the first save.
	 * @param[in] iPath The file to write.
	 */
	void startEmpty(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Add a language.
	 * @param[in] iCode Its code, unique.
	 * @param[in] iName Its name.
	 * @return False when the code is empty or taken.
	 */
	auto addLanguage(const std::string& iCode, const std::string& iName) -> bool;

	/**
	 * @brief
	 *  Remove a language and its values.
	 * @param[in] iCode Its code.
	 * @return True when it was removed.
	 */
	auto removeLanguage(std::string_view iCode) -> bool;

	/**
	 * @brief
	 *  Change the name, the font or the export flag of a language (its code stays).
	 * @param[in] iLanguage The language, found by its code.
	 * @param[in] iMerge Whether the edit merges with the previous one (typing).
	 * @return True when something changed.
	 */
	auto setLanguage(const data::localisation::Language& iLanguage, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Choose the fallback language.
	 * @param[in] iCode A language code of the localisation.
	 * @return True when it changed.
	 */
	auto setFallback(std::string_view iCode) -> bool;

	/**
	 * @brief
	 *  Append a string.
	 * @param[in] iKey Its key; made unique when taken.
	 * @param[in] iTable Its string table.
	 * @return The index of the string.
	 */
	auto addString(const std::string& iKey, const std::string& iTable) -> size_t;

	/**
	 * @brief
	 *  Rename the key of a string.
	 * @param[in] iIndex Index of the string.
	 * @param[in] iKey The new key, not empty and not taken.
	 * @return True when it changed.
	 */
	auto setKey(size_t iIndex, const std::string& iKey) -> bool;

	/**
	 * @brief
	 *  Set the value of a string in a language.
	 * @param[in] iIndex Index of the string.
	 * @param[in] iCode The language code.
	 * @param[in] iValue The text.
	 * @param[in] iMerge Whether the edit merges with the previous one (typing).
	 * @return True when it changed.
	 */
	auto setValue(size_t iIndex, const std::string& iCode, const std::string& iValue, bool iMerge = false) -> bool;

	/**
	 * @brief
	 *  Move strings to a string table.
	 * @param[in] iIndices Indices of the strings.
	 * @param[in] iTable The table.
	 * @return True when one moved.
	 */
	auto setTable(std::span<const size_t> iIndices, const std::string& iTable) -> bool;

	/**
	 * @brief
	 *  Remove strings.
	 * @param[in] iIndices Indices of the strings.
	 * @return True when one was removed.
	 */
	auto removeStrings(std::span<const size_t> iIndices) -> bool;

	/**
	 * @brief
	 *  Copy strings after the last of them, under unique keys.
	 * @param[in] iIndices Indices of the strings.
	 * @return True when one was copied.
	 */
	auto duplicateStrings(std::span<const size_t> iIndices) -> bool;

	/**
	 * @brief
	 *  A key not used yet.
	 * @param[in] iBase The wanted key.
	 * @return `iBase`, or `iBase` followed by a number.
	 */
	[[nodiscard]] auto getUniqueKey(std::string_view iBase) const -> std::string;

	// --- Selection -----------------------------------------------------------
	/**
	 * @brief
	 *  Selected strings, in increasing order.
	 * @return Their indices.
	 */
	[[nodiscard]] auto getSelection() const -> const std::vector<size_t>& { return m_selection; }

	/**
	 * @brief
	 *  Replace the selection.
	 * @param[in] iIndices Indices of the strings; out of range ones are dropped.
	 */
	void setSelection(std::vector<size_t> iIndices);

	// --- Tab -----------------------------------------------------------------
	/**
	 * @brief
	 *  Whether the tab is open.
	 * @return False once the user closed it.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Set the open state of the tab.
	 * @param[in] iOpen The state.
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	/**
	 * @brief
	 *  Apply a change as one undo step.
	 * @param[in] iNext The localisation after the change.
	 * @param[in] iDescription Text of the undo entry.
	 * @param[in] iMerge Whether it merges with the previous edit of the same text.
	 * @return True when the localisation changed.
	 */
	auto commit(data::localisation::Localisation iNext, const std::string& iDescription, bool iMerge) -> bool;

	/**
	 * @brief
	 *  Draw the languages: code, name, font, export flag and fallback.
	 */
	void renderLanguages();

	/**
	 * @brief
	 *  Draw the toolbar of the strings: table filter, search, add, move, duplicate, remove.
	 */
	void renderStringToolbar();

	/**
	 * @brief
	 *  Draw the grid of the strings.
	 */
	void renderStrings();

	/**
	 * @brief
	 *  Handle a click on the key cell of a row.
	 * @param[in] iIndex Index of the string.
	 */
	void clickRow(size_t iIndex);

	/// Disk path of `localisation.yml`.
	std::filesystem::path m_path;
	/// Localisation being edited.
	data::localisation::Localisation m_localisation;
	/// Undo stack.
	LocalisationUndoManager m_undo;
	/// Editor back-pointer.
	EditorLayer* mp_editorLayer{nullptr};
	/// Selected strings, in increasing order.
	std::vector<size_t> m_selection;
	/// Table shown, all when empty.
	std::optional<std::string> m_tableFilter;
	/// Text a shown key or value contains.
	std::string m_search;
	/// Code typed for a new language.
	std::string m_newLanguageCode;
	/// Table typed to move the selection to.
	std::string m_moveTable;
	/// Frame of the last edit from a field, to merge the keystrokes of one field.
	int m_lastEditFrame{-2};
	/// Tracks the ImGui "Open" state of the tab.
	bool m_pOpen{true};
	/// Whether the tab was focused last frame.
	bool m_wasFocused{false};
	/// Placeholder required by `Document::undoManager()`.
	SceneUndoManager m_emptySceneUndo;
};

}// namespace owl::nest
