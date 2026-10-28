/**
 * @file ContentBrowser.h
 * @author Silmaen
 * @date 10/01/2023
 * Copyright (c) 2023 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "../commands/CommandRegistry.h"
#include "AssetThumbnails.h"

#include <owlgui.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::nest::panel {
/**
 * @brief
 *  Class ContentBrowser.
 */
class ContentBrowser {
public:
	ContentBrowser(const ContentBrowser&) = delete;

	ContentBrowser(ContentBrowser&&) = delete;

	auto operator=(const ContentBrowser&) -> ContentBrowser& = delete;

	auto operator=(ContentBrowser&&) -> ContentBrowser& = delete;

	/**
	 * @brief
	 *  Default constructor.
	 */
	ContentBrowser();

	/**
	 * @brief
	 *  Destructor.
	 */
	~ContentBrowser() = default;

	/**
	 * @brief
	 *  The render command.
	 */
	void onImGuiRender();

	/**
	 * @brief
	 *  Render the pending scene or prefab preview; call once per frame before the viewports.
	 */
	void onUpdate() { m_previews.onUpdate(); }

	/**
	 * @brief
	 *  Previews of the folder shown that are ready, and to make in all.
	 * @return Ready and total counts.
	 */
	[[nodiscard]] auto getPreviewProgress() const -> std::pair<size_t, size_t> { return m_previews.getProgress(); }

	/**
	 * @brief
	 *  Show the folder of a file, the file selected, from the asset root that holds it.
	 * @param[in] iPath The file, absolute.
	 */
	void reveal(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Asset root shown, the base of the paths the browser drags.
	 * @return The root folder.
	 */
	[[nodiscard]] auto getRootPath() const -> const std::filesystem::path& { return m_currentRootPath; }

	/**
	 * @brief
	 *  Detach the content browser from the editor.
	 */
	void detach();

	/**
	 * @brief
	 *  Attach the content browser to the editor.
	 */
	void attach();

	/**
	 * @brief
	 *  Handle files dropped from the OS.
	 * @param[in] iPaths The dropped file paths.
	 */
	void handleFileDrop(const std::vector<std::filesystem::path>& iPaths);

	/**
	 * @brief
	 *  Set a callback invoked when a scene file is double-clicked.
	 * @param[in] iCallback Function taking the scene file path.
	 */
	void setSceneOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_sceneOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a code / text file is double-clicked
	 * (`.lua`, `.py`, `.cpp`, `.yml`, `.json`, `.md`, `.svg`, etc.).
	 * @param[in] iCallback Function taking the file path.
	 */
	void setCodeOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_codeOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a node-graph file (`.owlflow`) is double-clicked.
	 * @param[in] iCallback Function taking the node-graph file path.
	 */
	void setNodeGraphOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_nodeGraphOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when an animation clip (`.owlanim`) is double-clicked.
	 * @param[in] iCallback Function taking the animation clip file path.
	 */
	void setAnimationOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_animationOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a tilemap asset (`.owltilemap`) is double-clicked.
	 * @param[in] iCallback Function taking the tilemap asset file path.
	 */
	void setTilemapOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_tilemapOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a tileset asset (`.owltileset`) is double-clicked.
	 * @param[in] iCallback Function taking the tileset asset file path.
	 */
	void setTilesetOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_tilesetOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when an image (`.png`, `.jpg`, ...) is double-clicked.
	 * @param[in] iCallback Called with the image path.
	 */
	void setImageOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_imageOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a font (`.ttf`, `.otf` or its `.owlfont` sidecar) is double-clicked.
	 * @param[in] iCallback Function taking the font file path.
	 */
	void setFontOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_fontOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when the action map (`input_actions.yml`) is double-clicked.
	 * @param[in] iCallback Function taking the file path.
	 */
	void setActionMapOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_actionMapOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a material (`.owlmat`) is double-clicked.
	 * @param[in] iCallback Function taking the material file path.
	 */
	void setMaterialOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_materialOpenCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set the callback creating a material file ("New Material" of the folder menu); it runs `material.create` and
	 *  opens the file.
	 * @param[in] iCallback Function taking the path of the file to create.
	 */
	void setMaterialCreateCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_materialCreateCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a macro (`.owlmacro`) is double-clicked or run from its context menu.
	 * @param[in] iCallback Function taking the macro file path.
	 */
	void setMacroRunCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_macroRunCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked to turn a macro (`.owlmacro`) into an editor script, from its context menu.
	 * @param[in] iCallback Function taking the macro file path.
	 */
	void setMacroExportCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_macroExportCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked to run an editor script (a `.lua` under an `editor/` folder) from its context menu.
	 * @param[in] iCallback Function taking the script path.
	 */
	void setEditorScriptRunCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_editorScriptRunCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a mesh (`.gltf`, `.glb`, `.obj`) is double-clicked or placed from its context menu.
	 * @param[in] iCallback Function taking the mesh file path.
	 */
	void setMeshPlaceCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_meshPlaceCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Set a callback invoked when a mesh is opened from its context menu (in a mesh viewer).
	 * @param[in] iCallback Function taking the mesh file path.
	 */
	void setMeshOpenCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_meshOpenCallback = std::move(iCallback);
	}

	/// Signature of the callback running a registry command: true when it succeeded.
	using CommandCallback = std::function<bool(const std::string& iName, const commands::CommandArgs& iArgs)>;

	/**
	 * @brief
	 *  Set the callback running the file gestures (rename, move, delete, cook) as registry commands.
	 * @param[in] iCallback The callback.
	 */
	void setCommandCallback(CommandCallback iCallback) { m_commandCallback = std::move(iCallback); }

	/**
	 * @brief
	 *  Set the callback showing the import settings of an asset.
	 * @param[in] iCallback Called with the asset path.
	 */
	void setImportSettingsCallback(std::function<void(const std::filesystem::path&)> iCallback) {
		m_importSettingsCallback = std::move(iCallback);
	}

	/**
	 * @brief
	 *  Get the file or folder last clicked.
	 * @return Its path, empty when nothing is selected.
	 */
	[[nodiscard]] auto getSelectedPath() const -> const std::filesystem::path& { return m_selectedPath; }

private:
	/// The actual folder
	std::filesystem::path m_currentPath;
	/// Project root folder — back-navigation cannot go above this path.
	std::filesystem::path m_currentRootPath;
	/// Currently selected/right-clicked item (empty = background)
	std::filesystem::path m_selectedPath;
	/// Rename state
	bool m_renaming = false;
	/// Edit buffer for the inline rename input.
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
	/// Text typed in the search field; filters the current folder by name.
	std::string m_searchFilter;
	/// Asset family shown alone (`Texture`, `Scene`...), empty for every file.
	std::string m_kindFilter;
	/// Show a list of rows instead of a grid of tiles.
	bool m_listView = false;
	/// Side of a grid tile, in pixels (the thumbnail size).
	float m_tileSize = 88.f;
	/// Previews of the files: images, rendered scenes and prefabs, sound waveforms.
	AssetThumbnails m_previews;
	/// Sub-folders of the folders shown in the tree, by path; cleared after each scan.
	std::unordered_map<std::string, std::vector<std::filesystem::path>> m_folderCache;
	/// Folder the tree last expanded its ancestors for.
	std::filesystem::path m_treeSyncedPath;

	/**
	 * @brief
	 *  Open a file with the editor matching its type, or enter a folder.
	 * @param[in] iEntry The folder or file.
	 */
	void openEntry(const std::filesystem::directory_entry& iEntry);

	/**
	 * @brief
	 *  Kick off an asynchronous scan of the given path.
	 */
	void requestScan(const std::filesystem::path& iPath);

	/**
	 * @brief
	 *  Render the folder tree on the left: every asset root and its sub-folders.
	 */
	void renderFolderTree();

	/**
	 * @brief
	 *  Render one folder of the tree and, when expanded, its sub-folders.
	 * @param[in] iPath The folder.
	 * @param[in] iRoot The asset root it belongs to.
	 * @param[in] iLabel Name shown.
	 * @param[in] iIsRoot Whether it is an asset root.
	 */
	void renderFolderRow(const std::filesystem::path& iPath, const std::filesystem::path& iRoot,
						 const std::string& iLabel, bool iIsRoot);

	/**
	 * @brief
	 *  Sub-folders of a folder, read once and cached until the next scan.
	 * @param[in] iPath The folder.
	 * @return The sub-folders, sorted, hidden ones left out.
	 */
	auto getSubFolders(const std::filesystem::path& iPath) -> const std::vector<std::filesystem::path>&;

	/**
	 * @brief
	 *  Render the navigation/top toolbar.
	 */
	void renderTopBand();

	/**
	 * @brief
	 *  Render the family filter chips of the current folder, each with its count; hidden with a single family.
	 */
	void renderKindChips();

	/**
	 * @brief
	 *  Render the file/folder grid content.
	 */
	void renderContent();

	/**
	 * @brief
	 *  Render the right-click context menu popup.
	 */
	void renderContextMenu();

	/**
	 * @brief
	 *  Delete the currently selected file or folder.
	 */
	void deleteSelected();

	/**
	 * @brief
	 *  Create a new folder in the current directory.
	 */
	void createFolder();

	/**
	 * @brief
	 *  Ask for a new material file in the current directory (a free "New Material" name).
	 */
	void createMaterial();

	/**
	 * @brief
	 *  Create a starter visual script in the current folder and open it.
	 */
	void createVisualScript();

	/**
	 * @brief
	 *  Create a starter procedural generation graph in the current folder and open it.
	 */
	void createPcgGraph();

	/**
	 * @brief
	 *  Items of the context menu that create or import assets in the current folder.
	 */
	void renderCreateItems();

	/**
	 * @brief
	 *  Open a file dialog and import a file into the current directory.
	 */
	void importFiles();

	/**
	 * @brief
	 *  Open a folder dialog and import a folder into the current directory.
	 */
	void importFolder();

	/**
	 * @brief
	 *  Move or copy a file/folder to a destination directory.
	 * @param[in] iSource Source path.
	 * @param[in] iDestDir Destination directory.
	 */
	void moveItem(const std::filesystem::path& iSource, const std::filesystem::path& iDestDir);

	/**
	 * @brief
	 *  Run a file gesture through the command callback and rescan on success.
	 * @param[in] iName Command name.
	 * @param[in] iArgs Command arguments.
	 * @return True when the command ran.
	 */
	auto runCommand(const std::string& iName, const commands::CommandArgs& iArgs) -> bool;

	/// Callback for opening a scene file from the browser.
	std::function<void(const std::filesystem::path&)> m_sceneOpenCallback;
	std::function<void(const std::filesystem::path&)> m_codeOpenCallback;
	std::function<void(const std::filesystem::path&)> m_nodeGraphOpenCallback;
	std::function<void(const std::filesystem::path&)> m_animationOpenCallback;
	std::function<void(const std::filesystem::path&)> m_tilemapOpenCallback;
	std::function<void(const std::filesystem::path&)> m_tilesetOpenCallback;
	/// Called when an image is double-clicked.
	std::function<void(const std::filesystem::path&)> m_imageOpenCallback;
	/// Called when a font (`.ttf`, `.otf`, `.owlfont`) is double-clicked.
	std::function<void(const std::filesystem::path&)> m_fontOpenCallback;
	/// Called when the action map (`input_actions.yml`) is double-clicked.
	std::function<void(const std::filesystem::path&)> m_actionMapOpenCallback;
	/// Called when a material (`.owlmat`) is double-clicked.
	std::function<void(const std::filesystem::path&)> m_materialOpenCallback;
	/// Called with the path of a material file to create.
	std::function<void(const std::filesystem::path&)> m_materialCreateCallback;
	/// Called when a macro (`.owlmacro`) is double-clicked or run from its context menu.
	std::function<void(const std::filesystem::path&)> m_macroRunCallback;
	/// Called to export a macro to an editor script.
	std::function<void(const std::filesystem::path&)> m_macroExportCallback;
	/// Called to run an editor script.
	std::function<void(const std::filesystem::path&)> m_editorScriptRunCallback;
	/// Called when a mesh is double-clicked or placed from its context menu.
	std::function<void(const std::filesystem::path&)> m_meshPlaceCallback;
	/// Called when a mesh is opened from its context menu.
	std::function<void(const std::filesystem::path&)> m_meshOpenCallback;
	/// Runs the file gestures as registry commands.
	CommandCallback m_commandCallback;
	/// Called to show the import settings of an asset.
	std::function<void(const std::filesystem::path&)> m_importSettingsCallback;
};

}// namespace owl::nest::panel
