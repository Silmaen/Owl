/**
 * @file FileDialog.h
 * @author Silmaen
 * @date 27/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <filesystem>

/**
 * @brief Namespace for utility functions
 */
namespace owl::core::utils {

/**
 * @brief class For displaying a file dialog.
 */
class OWL_API FileDialog {
public:
	/**
	 * @brief Open a file dialog to search for an existing file.
	 * @param[in] iFilter Filter to apply during the search.
	 * @return A valid file path or null if canceled.
	 */
	static auto openFile(const std::string& iFilter) -> std::filesystem::path;

	/**
	 * @brief Open a file dialog to define a file to create.
	 * @param[in] iFilter Filter to apply during the search.
	 * @return A valid file path or null if canceled.
	 */
	static auto saveFile(const std::string& iFilter) -> std::filesystem::path;
};

}// namespace owl::core::utils
