/**
 * @file NewProjectDialog.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../ProjectTemplate.h"

#include <renderer/gpu/Texture.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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
 *  Why the name and the location of a new project cannot be used.
 */
enum struct NewProjectIssue : uint8_t {
	None,///< The project can be created.
	EmptyName,///< No name typed.
	InvalidName,///< The name is not a valid folder name (separator, reserved character, `.` or `..`).
	EmptyLocation,///< No location typed.
	MissingLocation,///< The location is not an existing folder.
	DestinationNotEmpty,///< The project folder exists and is not an empty folder.
};

/**
 * @brief
 *  Sentence telling the user what is wrong with the fields.
 * @param[in] iIssue The issue.
 * @return A short sentence, empty for `None`.
 */
[[nodiscard]] auto describe(NewProjectIssue iIssue) -> std::string_view;

/**
 * @brief
 *  Check a name and a location for a new project.
 * @param[in] iName Name of the project, also its folder name.
 * @param[in] iParentDirectory Folder that will hold the project folder.
 * @return The first issue found, `None` when the project can be created.
 */
[[nodiscard]] auto validateNewProject(const std::string& iName, const std::string& iParentDirectory) -> NewProjectIssue;

/**
 * @brief
 *  Index reached by moving a selection in a grid read row by row.
 * @param[in] iCurrent Current index.
 * @param[in] iCount Number of items.
 * @param[in] iColumns Items per row (at least 1).
 * @param[in] iDeltaX Columns to move (negative: left).
 * @param[in] iDeltaY Rows to move (negative: up).
 * @return The new index, clamped to the items; 0 when there is none.
 */
[[nodiscard]] auto moveInGrid(size_t iCurrent, size_t iCount, size_t iColumns, int iDeltaX, int iDeltaY) -> size_t;

/**
 * @brief
 *  Modal creating a project: a grid of template cards (preview, name, description, tags) filtered by tag, the
 *  details of the selected template, then the name and the location of the project.
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
	 * @return The request, or nothing when the fields do not pass `validateNewProject`.
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

	/**
	 * @brief
	 *  Issue of the current fields.
	 * @return What prevents the creation, `None` when Create is enabled.
	 */
	[[nodiscard]] auto getIssue() const -> NewProjectIssue { return validateNewProject(m_name, m_parentDirectory); }

	/**
	 * @brief
	 *  Index of the selected template in the list given to `open`.
	 * @return The index (equal to the template count when there is none).
	 */
	[[nodiscard]] auto getSelected() const -> size_t { return m_selected; }

	/**
	 * @brief
	 *  Every tag of the templates, in the order they first appear.
	 * @return The tags.
	 */
	[[nodiscard]] auto getTags() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Show only the templates carrying a tag; the selection moves to the first of them when it is hidden.
	 * @param[in] iTag The tag, empty for every template.
	 */
	void setTagFilter(const std::string& iTag);

	/**
	 * @brief
	 *  Templates shown by the current filter.
	 * @return Their indices in the list given to `open`.
	 */
	[[nodiscard]] auto getVisibleTemplates() const -> std::vector<size_t>;

	/**
	 * @brief
	 *  Move the selection among the shown templates, as the arrow keys do.
	 * @param[in] iColumns Cards per row of the grid.
	 * @param[in] iDeltaX Columns to move.
	 * @param[in] iDeltaY Rows to move.
	 */
	void moveSelection(size_t iColumns, int iDeltaX, int iDeltaY);

private:
	/**
	 * @brief
	 *  Draw the tag filter and the grid of cards.
	 * @param[in] iHeight Height of the template column.
	 */
	void renderTemplateGrid(float iHeight);

	/**
	 * @brief
	 *  Draw the preview, description and contents of a template.
	 * @param[in] iTemplate The selected template.
	 * @param[in] iWidth Width of the details column.
	 */
	void renderDetails(const ProjectTemplate& iTemplate, float iWidth);

	/**
	 * @brief
	 *  Draw the name and location fields with their check.
	 */
	void renderProjectFields();

	/**
	 * @brief
	 *  Texture of a template preview, loaded on first use.
	 * @param[in] iIndex Index of the template.
	 * @return The texture, null when the template has none or it cannot be read.
	 */
	auto getPreview(size_t iIndex) -> shared<renderer::gpu::Texture2D>;

	/// Templates offered.
	std::vector<ProjectTemplate> m_templates;
	/// Preview textures, by template index (null when none).
	std::map<size_t, shared<renderer::gpu::Texture2D>> m_previews;
	/// Project name being typed.
	std::string m_name;
	/// Parent folder being typed or picked.
	std::string m_parentDirectory;
	/// Tag the grid is filtered on, empty for all.
	std::string m_tagFilter;
	/// Index of the selected template.
	size_t m_selected = 0;
	/// Cards per row of the grid at the last frame, for the arrow keys.
	size_t m_columns = 1;
	/// True while the dialogue is shown.
	bool m_open = false;
	/// True until the ImGui popup is opened.
	bool m_openRequested = false;
	/// True when the selection changed by keyboard: the grid scrolls to it.
	bool m_scrollToSelection = false;
	/// True when Enter or a double-click on a card asked for the creation this frame.
	bool m_createRequested = false;
};

}// namespace owl::nest::panel
