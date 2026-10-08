/**
 * @file NewProjectDialog.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../ProjectTemplate.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace owl::nest::panel {

/**
 * @brief
 *  What the user asked for in the new-project dialogue.
 */
struct NewProjectRequest {
	/// Chosen template, or nothing for a bare project (no template installed).
	std::optional<ProjectTemplate> projectTemplate;
	/// Folder of the new project (parent folder / name).
	std::filesystem::path directory;
	/// Name of the new project.
	std::string name;
};

/**
 * @brief
 *  Modal asking for the name, the folder and the template of a new project.
 */
class NewProjectDialog final {
public:
	/**
	 * @brief
	 *  Show the dialogue.
	 * @param[in] iTemplates The available templates (may be empty).
	 * @param[in] iParentDirectory Folder proposed to hold the new project.
	 */
	void open(const std::vector<ProjectTemplate>& iTemplates, const std::filesystem::path& iParentDirectory);

	/**
	 * @brief
	 *  Check whether the dialogue is shown.
	 * @return True while open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_open; }

	/**
	 * @brief
	 *  Draw the dialogue.
	 * @return The request when the user clicked Create this frame.
	 */
	auto onImGuiRender() -> std::optional<NewProjectRequest>;

	/**
	 * @brief
	 *  Build the request from the current fields, as Create does.
	 * @return The request, or nothing when the name or the folder is empty.
	 */
	[[nodiscard]] auto makeRequest() const -> std::optional<NewProjectRequest>;

	/**
	 * @brief
	 *  Set the fields (the dialogue normally edits them).
	 * @param[in] iName Project name.
	 * @param[in] iParentDirectory Folder holding the project.
	 * @param[in] iTemplateIndex Index of the selected template.
	 */
	void setFields(const std::string& iName, const std::string& iParentDirectory, size_t iTemplateIndex);

private:
	/// Templates offered.
	std::vector<ProjectTemplate> m_templates;
	/// Project name being typed.
	std::string m_name;
	/// Parent folder being typed or picked.
	std::string m_parentDirectory;
	/// Index of the selected template.
	size_t m_selected = 0;
	/// True while the dialogue is shown.
	bool m_open = false;
	/// True until the ImGui popup is opened.
	bool m_openRequested = false;
};

}// namespace owl::nest::panel
