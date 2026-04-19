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
	ContentBrowser(const ContentBrowser&) = delete;
	ContentBrowser(ContentBrowser&&) = delete;
	auto operator=(const ContentBrowser&) -> ContentBrowser& = delete;
	auto operator=(ContentBrowser&&) -> ContentBrowser& = delete;
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
	void handleFileDrop(const std::vector<std::filesystem::path>& iPaths);

	/**
	 * @brief Set a callback invoked when a scene file is double-clicked.
	 * @param[in] iCallback Function taking the scene file path.
	 */
	void setSceneOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_sceneOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief Set a callback invoked when a code / text file is double-clicked
	 * (`.lua`, `.py`, `.cpp`, `.yml`, `.json`, `.md`, `.svg`, etc.).
	 * @param[in] iCallback Function taking the file path.
	 */
	void setCodeOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_codeOpenCallback = std::move(iCallback);
	}

private:
	/// The actual folder
	std::filesystem::path m_currentPath;
	std::filesystem::path m_currentRootPath;

	/// Currently selected/right-clicked item (empty = background)
	std::filesystem::path m_selectedPath;

	/// Rename state
	bool m_renaming = false;
	std::string m_renameBuffer;
	/// Delete confirmation state
	bool m_pendingDelete = false;

	/// Cached directory entries (populated asynchronously to avoid per-frame filesystem scans).
	std::vector<std::filesystem::directory_entry> m_cachedEntries;
	/// The path that produced the current cached entries.
	std::filesystem::path m_cachedPath;
	/// Incoming entries from a background scan, consumed on the main thread.
	shared<std::vector<std::filesystem::directory_entry>> m_pendingEntries;
	/// Path associated with the currently running background scan.
	std::filesystem::path m_pendingScanPath;
	/// True while a background scan is in progress.
	std::atomic<bool> m_scanInProgress = false;
	/// Request a rescan of the current directory on the next frame.
	bool m_rescanRequested = false;

	/// Kick off an asynchronous scan of the given path.
	void requestScan(const std::filesystem::path& iPath);

	/// Render the navigation/top toolbar.
	void renderTopBand();
	/// Render the file/folder grid content.
	void renderContent();
	/// Render the right-click context menu popup.
	void renderContextMenu();

	/// Delete the currently selected file or folder.
	void deleteSelected();
	/// Create a new folder in the current directory.
	void createFolder();
	/// Open a file dialog and import a file into the current directory.
	void importFiles();
	/// Open a folder dialog and import a folder into the current directory.
	void importFolder();

	/**
	 * @brief Move or copy a file/folder to a destination directory.
	 * @param[in] iSource Source path.
	 * @param[in] iDestDir Destination directory.
	 */
	void moveItem(const std::filesystem::path& iSource, const std::filesystem::path& iDestDir);

	/// Callback for opening a scene file from the browser.
	std::function<void(const std::filesystem::path&)> m_sceneOpenCallback;
	std::function<void(const std::filesystem::path&)> m_codeOpenCallback;
};

}// namespace owl::nest::panel
