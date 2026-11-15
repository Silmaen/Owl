/**
 * @file ExtensionCommands.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../UndoCommand.h"

#include <filesystem>
#include <string>
#include <vector>

namespace owl::nest::commands {

class CommandRegistry;

/**
 * @brief
 *  Folder holding the extension packs of a project, one sub-folder per pack.
 * @param[in] iProjectDirectory The project folder.
 * @return `<project>/extensions`.
 */
[[nodiscard]] auto getExtensionsFolder(const std::filesystem::path& iProjectDirectory) -> std::filesystem::path;

/**
 * @brief
 *  File `extension.export` writes when it is given no output.
 * @param[in] iProjectDirectory The project folder.
 * @param[in] iId Id of the pack.
 * @return `<project>/exports/<id>.owlmod`.
 */
[[nodiscard]] auto getExtensionExportFile(const std::filesystem::path& iProjectDirectory, const std::string& iId)
		-> std::filesystem::path;

/**
 * @brief
 *  Enabled extension packs of a project: the packs of its `extensions` folder whose id is not disabled.
 * @param[in] iProjectDirectory The project folder.
 * @param[in] iDisabled Ids of the packs disabled in the project.
 * @return The pack folders, sorted.
 */
[[nodiscard]] auto getEnabledExtensionSources(const std::filesystem::path& iProjectDirectory,
											  const std::vector<std::string>& iDisabled)
		-> std::vector<std::filesystem::path>;

/**
 * @brief
 *  Change of the manifest of an extension pack: its text before and after, written back on undo and redo.
 *
 * The scene the command is recorded in is not touched: the command only rides its undo history.
 */
class SetExtensionManifestCommand final : public SceneUndoCommand {
public:
	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	SetExtensionManifestCommand(const SetExtensionManifestCommand&) = delete;

	/**
	 * @brief
	 *  Default move constructor.
	 */
	SetExtensionManifestCommand(SetExtensionManifestCommand&&) = default;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const SetExtensionManifestCommand&) -> SetExtensionManifestCommand& = delete;

	/**
	 * @brief
	 *  Default move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(SetExtensionManifestCommand&&) -> SetExtensionManifestCommand& = default;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iFile The manifest file (`extension.yml`).
	 * @param[in] iBefore Its text before the change.
	 * @param[in] iAfter Its text after the change.
	 */
	SetExtensionManifestCommand(std::filesystem::path iFile, std::string iBefore, std::string iAfter);

	/**
	 * @brief
	 *  Destructor.
	 */
	~SetExtensionManifestCommand() override;

	/**
	 * @brief
	 *  Write the manifest back as it was.
	 * @param[in,out] ioScene The scene of the history (unchanged).
	 */
	void undo(scene::Scene& ioScene) override;

	/**
	 * @brief
	 *  Write the changed manifest.
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
	/// The manifest file.
	std::filesystem::path m_file;
	/// Text before the change.
	std::string m_before;
	/// Text after the change.
	std::string m_after;
};

/**
 * @brief
 *  Register the extension pack commands: list, create, edit the manifest and export as `.owlmod`.
 * @param[in,out] ioRegistry The registry.
 */
void addExtensionCommands(CommandRegistry& ioRegistry);

}// namespace owl::nest::commands
