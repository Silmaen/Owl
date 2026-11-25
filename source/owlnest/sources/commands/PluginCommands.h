/**
 * @file PluginCommands.h
 * @author Silmaen
 * @date 24/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <filesystem>
#include <optional>
#include <string>

namespace owl::nest::commands {

/**
 * @brief
 *  A document of a type an Owl Nest plugin declares, open in an editor tab: its text, edited by `plugin.document_set`
 *  through the undo history of the tab.
 */
class PluginDocumentEditor {
public:
	PluginDocumentEditor() = default;

	PluginDocumentEditor(const PluginDocumentEditor&) = default;

	PluginDocumentEditor(PluginDocumentEditor&&) = default;

	auto operator=(const PluginDocumentEditor&) -> PluginDocumentEditor& = default;

	auto operator=(PluginDocumentEditor&&) -> PluginDocumentEditor& = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	virtual ~PluginDocumentEditor();

	/**
	 * @brief
	 *  The text of the document as it is.
	 * @return The text.
	 */
	[[nodiscard]] virtual auto getText() const -> const std::string& = 0;

	/**
	 * @brief
	 *  Replace the text as one undoable step.
	 * @param[in] iText The text after the edit.
	 * @param[in] iDescription Name of the step in the history.
	 * @return False when nothing changes.
	 */
	virtual auto setText(const std::string& iText, const std::string& iDescription) -> bool = 0;
};

/**
 * @brief
 *  A text file written by a command (the asset an importer of an Owl Nest plugin produces): undo puts the previous
 *  content back, or removes the file when there was none.
 *
 * The scene the command is recorded in is not touched: the command only rides its undo history.
 */
class WriteTextFileCommand final : public SceneUndoCommand {
public:
	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	WriteTextFileCommand(const WriteTextFileCommand&) = delete;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	WriteTextFileCommand(WriteTextFileCommand&&) = default;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const WriteTextFileCommand&) -> WriteTextFileCommand& = delete;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(WriteTextFileCommand&&) -> WriteTextFileCommand& = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iFile The file written.
	 * @param[in] iBefore Its content before the command, nothing when the file did not exist.
	 * @param[in] iAfter Its content after the command.
	 */
	WriteTextFileCommand(std::filesystem::path iFile, std::optional<std::string> iBefore, std::string iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~WriteTextFileCommand() override;

	/**
	 * @brief
	 *  Put the previous content back, or remove the file.
	 * @param[in,out] ioScene The scene of the history (unchanged).
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Write the file.
	 * @param[in,out] ioScene The scene of the history (unchanged).
	 */
	void redo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Description.
	 * @return Human-readable description for menus and tooltips.
	 */
	[[nodiscard]] auto description() const -> std::string override;

private:
	/// The file written.
	std::filesystem::path m_file;
	/// Content before the command, nothing when the file did not exist.
	std::optional<std::string> m_before;
	/// Content after the command.
	std::string m_after;
};

}// namespace owl::nest::commands
