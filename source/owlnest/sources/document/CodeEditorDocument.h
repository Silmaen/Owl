/**
 * @file CodeEditorDocument.h
 * @author Silmaen
 * @date 19/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Document.h"
#include "UndoManager.h"
#include "codeEditor/LanguageDefinitions.h"
#include "codeEditor/MarkdownPreview.h"
#include "codeEditor/SvgPreview.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

class TextEditor;

namespace owl::nest {
/**
 * @brief
 *  Text-editor document: edits any supported source / config / markup file.
 *
 * Backed by the `ImGuiColorTextEdit` widget (goossens fork, Conan package
 * `imgui-color-text-edit`), which also brings selection, multi-cursor and the
 * find / replace bar. Built-in syntax highlighting for Lua, C, C++, Python, JSON
 * and Markdown; custom definitions for YAML, XML/SVG and Bash (see
 * `codeEditor::applyLanguage`).
 *
 * The document owns its own undo stack via the embedded `TextEditor` (the
 * document-level `UndoManager` stays empty — editor undo is separate from the
 * scene's global undo stack).
 */
class CodeEditorDocument final : public Document {
public:
	/**
	 * @brief
	 *  Position of the main cursor as shown on the status line.
	 */
	struct CursorLocation {
		size_t line = 0;///< Zero-based line.
		size_t column = 0;///< Zero-based visual column (tabs expanded).
	};

	CodeEditorDocument(const CodeEditorDocument&) = delete;

	CodeEditorDocument(CodeEditorDocument&&) = delete;

	auto operator=(const CodeEditorDocument&) -> CodeEditorDocument& = delete;

	auto operator=(CodeEditorDocument&&) -> CodeEditorDocument& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	CodeEditorDocument();

	/**
	 * @brief
	 *  Destructor.
	 */
	~CodeEditorDocument() override;

	// --- Document interface --------------------------------------------------
	/**
	 * @brief
	 *  Document kind.
	 * @return The document type.
	 */
	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Code; }

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
	 *  Current content, serialized in the document's file format.
	 * @return The content.
	 */
	[[nodiscard]] auto recoverySnapshot() const -> std::optional<std::string> override;

	/**
	 * @brief
	 *  Replace the content with an autosaved snapshot, keeping the path; the document becomes dirty.
	 * @param[in] iSnapshot The serialized content.
	 * @return True when the snapshot was applied.
	 */
	auto restoreRecoverySnapshot(const std::string& iSnapshot) -> bool override;

	/**
	 * @brief
	 *  Access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() -> SceneUndoManager& override { return m_undoManager; }

	/**
	 * @brief
	 *  Const access to the document's undo/redo stack.
	 * @return The undo manager.
	 */
	[[nodiscard]] auto undoManager() const -> const SceneUndoManager& override { return m_undoManager; }

	// --- Code-editor-specific API --------------------------------------------
	/**
	 * @brief
	 *  Load the file at `iPath` into the editor. Returns false on I/O error.
	 * @param[in] iPath The file to load.
	 * @return True on success, false otherwise.
	 */
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;

	/**
	 * @brief
	 *  Location of the main cursor, (0, 0) before the document is attached.
	 * @return The line and visual column of the main cursor.
	 */
	[[nodiscard]] auto cursorLocation() const -> CursorLocation;

	/**
	 * @brief
	 *  The embedded text-editor widget (editing, selection, search, undo), null before `onAttach`.
	 * @return The widget, or nullptr when the document is not attached.
	 */
	[[nodiscard]] auto editor() const -> TextEditor* { return mp_editor.get(); }

	/**
	 * @brief
	 *  Detected or explicitly-set language of this document.
	 * @return The codeEditor Language.
	 */
	[[nodiscard]] auto language() const -> codeEditor::Language { return m_language; }

	/**
	 * @brief
	 *  True while the user hasn't clicked the tab's close X.
	 * @return True when the object is open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }

	/**
	 * @brief
	 *  Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	 */
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

	/**
	 * @brief
	 *  True if a live preview pane is currently shown next to the editor.
	 * @return True when the object is preview visible.
	 */
	[[nodiscard]] auto isPreviewVisible() const -> bool { return m_previewVisible; }

	/**
	 * @brief
	 *  Toggle the live preview pane (only meaningful for Markdown / SVG documents).
	 */
	void togglePreview() { m_previewVisible = !m_previewVisible; }

	/**
	 * @brief
	 *  True if the active language has a live preview implementation.
	 * @return True when show preview is allowed.
	 */
	[[nodiscard]] auto canShowPreview() const -> bool {
		return m_language == codeEditor::Language::Markdown || m_language == codeEditor::Language::Xml;
	}

private:
	/// Disk path of the file currently loaded into the editor.
	std::filesystem::path m_path;
	/// Detected language used for syntax highlighting.
	codeEditor::Language m_language = codeEditor::Language::PlainText;
	/**
	 * @brief
	 *  Owned text-editor widget. Heap-allocated so the header doesn't need to include the
	 *  (heavy) vendored header — only the .cpp does.
	 */
	uniq<TextEditor> mp_editor;
	/// Unused (kept to satisfy the Document interface); code edits live in the TextEditor's own stack.
	SceneUndoManager m_undoManager;
	/// Snapshot of the text at last save — used for the dirty indicator.
	std::string m_savedText;
	/// Editor layer back-pointer (for menus, dialogs, status messages).
	EditorLayer* mp_editorLayer = nullptr;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;
	/// Whether the live preview pane is visible.
	bool m_previewVisible = false;
	/// Horizontal split between the editor (left) and the preview (right), in [0.1, 0.9].
	float m_splitRatio = 0.5f;
	/// Markdown renderer (used when language == Markdown).
	codeEditor::MarkdownPreview m_mdPreview;
	/// SVG renderer (used when language == Xml).
	codeEditor::SvgPreview m_svgPreview;
};

}// namespace owl::nest
