/**
 * @file ProjectTemplate.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  A starting point for a new project: a folder of `engine_assets/project_templates/` holding a `template.yml`
 *  (name, description, order), an `owl_project.yml` and the scenes and assets the project starts with.
 */
struct ProjectTemplate {
	/// Folder name, stable identifier of the template.
	std::string id;
	/// Name shown in the new-project dialogue.
	std::string name;
	/// One-sentence description shown in the new-project dialogue.
	std::string description;
	/// Position in the list (ascending).
	int order = 0;
	/// Folder copied into the new project.
	std::filesystem::path directory;
};

/**
 * @brief
 *  Reason why a project could not be created from a template.
 */
enum struct ProjectTemplateError : uint8_t {
	MissingTemplate,///< The template folder or its `owl_project.yml` does not exist.
	DestinationNotEmpty,///< The destination folder exists and is not empty.
	CopyFailed,///< The template files could not be copied.
	InvalidProjectFile,///< The copied `owl_project.yml` cannot be read or written.
};

/**
 * @brief
 *  Human-readable description of a template error.
 * @param[in] iError The error.
 * @return A short sentence fragment.
 */
[[nodiscard]] auto describe(ProjectTemplateError iError) -> std::string_view;

/**
 * @brief
 *  List the templates of a folder, sorted by order then name.
 * @param[in] iRoot Folder holding one sub-folder per template.
 * @return The templates; folders without `template.yml` or `owl_project.yml` are skipped.
 */
[[nodiscard]] auto listProjectTemplates(const std::filesystem::path& iRoot) -> std::vector<ProjectTemplate>;

/**
 * @brief
 *  Create a project by copying a template and renaming it.
 * @param[in] iTemplate The template.
 * @param[in] iDestination Folder of the new project; created, must be absent or empty.
 * @param[in] iName Name written in the new `owl_project.yml`.
 * @return Nothing on success, the failure reason otherwise.
 */
[[nodiscard]] auto createProjectFromTemplate(const ProjectTemplate& iTemplate,
											 const std::filesystem::path& iDestination, const std::string& iName)
		-> expected<void, ProjectTemplateError>;

}// namespace owl::nest
