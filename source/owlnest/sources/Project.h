/**
 * @file Project.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {

/**
 * @brief Structure representing an Owl Nest project.
 */
struct Project {
	/// Project display name.
	std::string name;
	/// Relative path to the first scene.
	std::string firstScene;
	/// Absolute path to the project root directory.
	std::filesystem::path projectDirectory;

	/**
	 * @brief Load project configuration from a YAML file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save project configuration to a YAML file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;

	/**
	 * @brief Check if a project is currently loaded.
	 * @return True if a project directory is set.
	 */
	[[nodiscard]] auto isLoaded() const -> bool { return !projectDirectory.empty(); }
};

}// namespace owl::nest
