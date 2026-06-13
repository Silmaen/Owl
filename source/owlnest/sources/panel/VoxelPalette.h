/**
 * @file VoxelPalette.h
 * @author Silmaen
 * @date 12/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <owl.h>

#include "../VoxelBrush.h"

#include <filesystem>

namespace owl::nest::panel {

/**
 * @brief
 *  Dockable panel driving the voxel brush: pick a paint block, the eraser, or a saved structure to stamp.
 *
 * Mirrors the tilemap `TilePalette` UX. The block list comes from the active
 * scene's first `VoxelWorld` registry; structures are `.owlvoxstruct` files in
 * the project's `structures/` directory (saved from the current world's
 * non-air bounding box). All selection writes into a shared `VoxelBrush` the
 * viewport reads.
 */
class VoxelPalette final {
public:
	VoxelPalette(const VoxelPalette&) = delete;
	VoxelPalette(VoxelPalette&&) = delete;
	auto operator=(const VoxelPalette&) -> VoxelPalette& = delete;
	auto operator=(VoxelPalette&&) -> VoxelPalette& = delete;

	VoxelPalette() = default;
	~VoxelPalette() = default;

	/**
	 * @brief
	 *  Make the window visible (toggled from the editor menu).
	 */
	void open() { m_visible = true; }

	/**
	 * @brief
	 *  Set the active scene whose `VoxelWorld` registry feeds the block list.
	 * @param[in] iScene The active scene (may be null).
	 */
	void setScene(const shared<scene::Scene>& iScene) { m_scene = iScene; }

	/**
	 * @brief
	 *  Set the project directory; structures live under its `structures/` subfolder.
	 * @param[in] iDirectory The project directory.
	 */
	void setProjectDirectory(const std::filesystem::path& iDirectory) { m_projectDirectory = iDirectory; }

	/**
	 * @brief
	 *  Render the panel and update the shared brush state.
	 * @param[in,out] ioBrush The shared voxel-brush state the viewport consumes.
	 */
	void onImGuiRender(VoxelBrush& ioBrush);

private:
	/**
	 * @brief
	 *  Directory holding `.owlvoxstruct` files (`<project>/structures`).
	 * @return The structures directory path.
	 */
	[[nodiscard]] auto structureDirectory() const -> std::filesystem::path;

	/**
	 * @brief
	 *  Capture the active world's non-air bounding box and write it as a named structure file.
	 * @param[in] iName The structure file name (without extension).
	 */
	void saveStructure(const std::string& iName) const;

	/// Active scene (shared with the owning document; may be null).
	shared<scene::Scene> m_scene;
	/// Project directory (root for the `structures/` folder).
	std::filesystem::path m_projectDirectory;
	/// Whether the panel window is visible.
	bool m_visible = true;
	/// Text buffer for the structure-save name field.
	std::string m_saveName = "structure";
};

}// namespace owl::nest::panel
