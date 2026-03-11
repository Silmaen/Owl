/**
 * @file ContentBrowser.h
 * @author Silmaen
 * @date 10/01/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

namespace owl::nest::panel {

/**
 * @brief Class ContentBrowser
 */
class ContentBrowser {
public:
	/**
	 * @brief Default copy constructor
	 */
	ContentBrowser(const ContentBrowser&) = default;
	/**
	 * @brief Default move constructor
	 */
	ContentBrowser(ContentBrowser&&) = default;
	/**
	 * @brief Default copy assignation
	 * @return this
	 */
	auto operator=(const ContentBrowser&) -> ContentBrowser& = default;
	/**
	 * @brief Default move assignation
	 * @return this
	 */
	auto operator=(ContentBrowser&&) -> ContentBrowser& = default;
	/**
	 * @brief Default constructor.
	 */
	ContentBrowser();
	/**
	 * @brief Destructor.
	 */
	~ContentBrowser() = default;

	/**
	 * @brief The render command
	 */
	void onImGuiRender();

	/**
	 * @brief Detach the content browser from the editor.
	 */
	void detach();

	/**
	 * @brief Attach the content browser to the editor.
	 */
	void attach();

	/**
	 * @brief Handle files dropped from the OS.
	 * @param[in] iPaths The dropped file paths.
	 */
	void handleFileDrop(const std::vector<std::filesystem::path>& iPaths) const;

private:
	/// The actual folder
	std::filesystem::path m_currentPath;
	std::filesystem::path m_currentRootPath;

	/// Currently selected/right-clicked item (empty = background)
	std::filesystem::path m_selectedPath;

	/// Rename state
	bool m_renaming = false;
	std::string m_renameBuffer;

	/// Render the navigation/top toolbar.
	void renderTopBand();
	/// Render the file/folder grid content.
	void renderContent();
	/// Render the right-click context menu popup.
	void renderContextMenu();

	/// Delete the currently selected file or folder.
	void deleteSelected();
	/// Create a new folder in the current directory.
	void createFolder() const;
	/// Open a file dialog and import a file into the current directory.
	void importFiles() const;
	/// Open a folder dialog and import a folder into the current directory.
	void importFolder() const;

	/**
	 * @brief Move or copy a file/folder to a destination directory.
	 * @param[in] iSource Source path.
	 * @param[in] iDestDir Destination directory.
	 */
	static void moveItem(const std::filesystem::path& iSource, const std::filesystem::path& iDestDir);
};

}// namespace owl::nest::panel
