/**
 * @file ProjectSettings.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../Project.h"

namespace owl::nest::panel {

/**
 * @brief Panel for editing project settings.
 */
class ProjectSettings final {
public:
	ProjectSettings(const ProjectSettings&) = default;
	ProjectSettings(ProjectSettings&&) = default;
	auto operator=(const ProjectSettings&) -> ProjectSettings& = default;
	auto operator=(ProjectSettings&&) -> ProjectSettings& = default;
	ProjectSettings() = default;
	~ProjectSettings() = default;

	/**
	 * @brief Open the modal dialog for editing the given project.
	 * @param[in] iProject The project to edit.
	 */
	void open(const Project& iProject);

	/**
	 * @brief Render the modal dialog.
	 */
	void onImGuiRender();

	/**
	 * @brief Check whether the project was modified and accepted.
	 * @return True if changes were applied.
	 */
	[[nodiscard]] auto hasResult() const -> bool { return m_hasResult; }

	/**
	 * @brief Consume the result.
	 * @return The edited project.
	 */
	auto consumeResult() -> Project;

private:
	/**
	 * @brief Scan the project directory for .owl scene files.
	 */
	void scanScenes();

	/// Local working copy.
	Project m_localProject;
	/// Name buffer for ImGui input.
	std::string m_nameBuffer;
	/// Selected index in the scene list.
	int m_selectedSceneIndex = -1;
	/// List of available scene paths (relative to project directory).
	std::vector<std::string> m_availableScenes;
	/// Whether the popup should be opened on the next render call.
	bool m_pendingOpen = false;
	/// Whether a result is ready to be consumed.
	bool m_hasResult = false;
};

}// namespace owl::nest::panel
