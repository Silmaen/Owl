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

class TextEditor;

namespace owl::nest {

/**
 * @brief Text-editor document: edits any supported source / config / markup file.
 *
 * Backed by the vendored `ImGuiColorTextEdit` widget (MIT, see
 * `source/owl/private/gui/external/ImGuiColorTextEdit/`). Built-in syntax
 * highlighting for Lua, C, C++; custom definitions for Python, YAML, JSON,
 * Markdown, and XML/SVG (see `codeEditor::LanguageDefinitions`).
 *
 * The document owns its own undo stack via the embedded `TextEditor` (the
 * document-level `UndoManager` stays empty — editor undo is separate from the
 * scene's global undo stack).
 */
class CodeEditorDocument final : public Document {
public:
	CodeEditorDocument(const CodeEditorDocument&) = delete;
	CodeEditorDocument(CodeEditorDocument&&) = delete;
	auto operator=(const CodeEditorDocument&) -> CodeEditorDocument& = delete;
	auto operator=(CodeEditorDocument&&) -> CodeEditorDocument& = delete;

	CodeEditorDocument();
	~CodeEditorDocument() override;

	// --- Document interface --------------------------------------------------

	[[nodiscard]] auto type() const -> DocumentType override { return DocumentType::Code; }
	[[nodiscard]] auto title() const -> std::string override;
	[[nodiscard]] auto filePath() const -> std::filesystem::path override { return m_path; }
	[[nodiscard]] auto isDirty() const -> bool override;

	void onAttach(EditorLayer* iEditor) override;
	void onDetach() override;
	void onUpdate(const core::Timestep& iTimeStep) override;
	void onEvent(event::Event& ioEvent) override;
	void onImGuiRender() override;

	auto save() -> bool override;
	auto saveAs(const std::filesystem::path& iPath) -> bool override;

	[[nodiscard]] auto undoManager() -> UndoManager& override { return m_undoManager; }
	[[nodiscard]] auto undoManager() const -> const UndoManager& override { return m_undoManager; }

	// --- Code-editor-specific API --------------------------------------------

	/// @brief Load the file at `iPath` into the editor. Returns false on I/O error.
	auto loadFromFile(const std::filesystem::path& iPath) -> bool;
	/// @brief Detected or explicitly-set language of this document.
	[[nodiscard]] auto language() const -> codeEditor::Language { return m_language; }

	/// @brief True while the user hasn't clicked the tab's close X.
	[[nodiscard]] auto isOpen() const -> bool { return m_pOpen; }
	/// @brief Reset the open flag (used by `EditorLayer` after a close is handled or cancelled).
	void setOpen(const bool iOpen) { m_pOpen = iOpen; }

private:
	std::filesystem::path m_path;
	codeEditor::Language m_language = codeEditor::Language::PlainText;
	// TextEditor is heap-allocated so the header doesn't need to include the
	// (heavy) vendored header — only the .cpp does.
	uniq<TextEditor> mp_editor;
	/// Unused (kept to satisfy the Document interface); code edits live in the TextEditor's own stack.
	UndoManager m_undoManager;
	/// Snapshot of the text at last save — used for the dirty indicator.
	std::string m_savedText;
	EditorLayer* mp_editorLayer = nullptr;
	/// ImGui close state — set to false by ImGui when the tab's close X is clicked.
	bool m_pOpen = true;
};

}// namespace owl::nest
