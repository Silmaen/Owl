/**
 * @file BrowserFilter.h
 * @author Silmaen
 * @date 02/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <app/Application.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace owl::nest::panel {

/// Where an asset root of the Content Browser comes from.
using AssetOrigin = app::Application::AssetOrigin;

/**
 * @brief
 *  Whether the Content Browser lists an asset root: the assets of Owl Nest itself only when every file shows.
 * @param[in] iOrigin Where the root comes from.
 * @param[in] iShowAll The *Show All Files* setting.
 * @return True when the root is listed.
 */
[[nodiscard]] auto isBrowsedRoot(AssetOrigin iOrigin, bool iShowAll) -> bool;

/**
 * @brief
 *  Whether the Content Browser hides a file or folder unless every file shows: the build products of the shaders
 *  (`spirv/` folders, `.spv` and `.spv.hash` files) and the logos of the engine assets.
 * @param[in] iPath Absolute path of the file or folder.
 * @param[in] iRoot The asset root holding it.
 * @param[in] iOrigin Where the root comes from.
 * @return True when it is hidden.
 */
[[nodiscard]] auto isHiddenEntry(const std::filesystem::path& iPath, const std::filesystem::path& iRoot,
								 AssetOrigin iOrigin) -> bool;

/**
 * @brief
 *  The files a game export packs, for the *Packaged* filter of the Content Browser.
 */
class PackagedFiles final {
public:
	/**
	 * @brief
	 *  Replace the files.
	 * @param[in] iFiles Absolute paths of the packed files.
	 */
	void assign(const std::vector<std::filesystem::path>& iFiles);

	/**
	 * @brief
	 *  Whether a file is packed.
	 * @param[in] iFile Absolute path of the file.
	 * @return True when the export packs it.
	 */
	[[nodiscard]] auto contains(const std::filesystem::path& iFile) const -> bool;

	/**
	 * @brief
	 *  Whether a folder holds a packed file, at any depth.
	 * @param[in] iFolder Absolute path of the folder.
	 * @return True when a packed file is under it.
	 */
	[[nodiscard]] auto holds(const std::filesystem::path& iFolder) const -> bool;

	/**
	 * @brief
	 *  Number of packed files.
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> size_t { return m_files.size(); }

private:
	/// Normalized generic paths of the packed files, sorted.
	std::vector<std::string> m_files;
};

}// namespace owl::nest::panel
