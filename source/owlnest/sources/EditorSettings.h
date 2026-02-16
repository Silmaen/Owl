/**
 * @file EditorSettings.h
 * @author Silmaen
 * @date 16/02/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest {

/**
 * @brief Structure holding editor-specific settings persisted across sessions.
 */
struct EditorSettings {
	/// Whether the stats panel is visible.
	bool showStats = true;

	/**
	 * @brief Load settings from a YAML file.
	 * @param[in] iFile The file to load.
	 */
	void loadFromFile(const std::filesystem::path& iFile);

	/**
	 * @brief Save settings to a YAML file.
	 * @param[in] iFile The file to save.
	 */
	void saveToFile(const std::filesystem::path& iFile) const;
};

}// namespace owl::nest
