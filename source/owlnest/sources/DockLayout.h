/**
 * @file DockLayout.h
 * @author Silmaen
 * @date 10/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace owl::nest {

/**
 * @brief
 *  Arrangement of the editor panels around the documents.
 */
enum struct LayoutPreset : uint8_t {
	Default,///< Hierarchy over Properties on the right, Content Browser and Log under the documents.
	HierarchyLeft,///< Hierarchy in its own column left of the documents, Properties alone on the right.
};

/**
 * @brief
 *  Window names of the editor panels, as given to `ImGui::Begin` (only the part from `###` counts for docking).
 */
namespace panelWindow {
/// Scene hierarchy, retitled by the active document.
constexpr auto hierarchy = "###Hierarchy";
/// Entity properties, retitled by the active document.
constexpr auto properties = "###Properties";
/// Project asset browser.
constexpr auto contentBrowser = "###ContentBrowser";
/// Engine log.
constexpr auto log = "###Log";
/// Frame statistics.
constexpr auto stats = "###Stats";
/// Voxel brush palette, shown with a voxel world.
constexpr auto voxelPalette = "###VoxelPalette";
/// Tile brush palette, shown with a tilemap document.
constexpr auto tilePalette = "###TilePalette";
/// Per-scene settings.
constexpr auto sceneSettings = "###SceneSettings";
/// Bundled documentation.
constexpr auto help = "###Help";

/**
 * @brief
 *  Visible title of a panel: its glyph and label, then its window name so docking keeps finding it.
 * @param[in] iGlyph The `gui::icon` glyph of the panel.
 * @param[in] iLabel The label shown in the tab.
 * @param[in] iWindow One of the `panelWindow` names.
 * @return The string to give to `ImGui::Begin`.
 */
[[nodiscard]] auto title(std::string_view iGlyph, std::string_view iLabel, std::string_view iWindow) -> std::string;
}// namespace panelWindow

/**
 * @brief
 *  Builds the dock tree of Owl Nest and decides when it must be rebuilt.
 *
 * The tree is rebuilt on the first frame when the saved layout has no split (first launch, or a layout that only
 * holds floating windows), when the saved layout predates `version`, and on request (preset change, reset).
 */
class DockLayout final {
public:
	/// Layout generation; raising it rebuilds every saved layout once.
	static constexpr int version = 2;

	/**
	 * @brief
	 *  Name of a preset, as stored in the editor settings.
	 * @param[in] iPreset The preset.
	 * @return The stored name.
	 */
	[[nodiscard]] static auto getPresetName(LayoutPreset iPreset) -> std::string_view;

	/**
	 * @brief
	 *  Preset from its stored name.
	 * @param[in] iName The stored name.
	 * @return The preset, `LayoutPreset::Default` for an unknown name.
	 */
	[[nodiscard]] static auto presetFromName(std::string_view iName) -> LayoutPreset;

	/**
	 * @brief
	 *  Rebuild the dock tree: panels around a central node holding the given document windows.
	 * @param[in] iDockSpaceId The dockspace to rebuild.
	 * @param[in] iPreset The arrangement.
	 * @param[in] iDocumentWindows Names of the document windows to dock in the central node.
	 */
	static void build(uint32_t iDockSpaceId, LayoutPreset iPreset, const std::vector<std::string>& iDocumentWindows);

	/**
	 * @brief
	 *  Names of the windows docked in the dockspace that are not editor panels: the open documents.
	 * @param[in] iDockSpaceId The dockspace.
	 * @return The window names.
	 */
	[[nodiscard]] static auto collectDocumentWindows(uint32_t iDockSpaceId) -> std::vector<std::string>;

	/**
	 * @brief
	 *  Current preset.
	 * @return The preset.
	 */
	[[nodiscard]] auto getPreset() const -> LayoutPreset { return m_preset; }

	/**
	 * @brief
	 *  Set the preset applied by the next rebuild, without scheduling one.
	 * @param[in] iPreset The preset.
	 */
	void setPreset(const LayoutPreset iPreset) { m_preset = iPreset; }

	/**
	 * @brief
	 *  Switch to a preset and rebuild the dock tree on the next frame.
	 * @param[in] iPreset The preset.
	 */
	void request(LayoutPreset iPreset);

	/**
	 * @brief
	 *  Whether a rebuild waits for the next frame.
	 * @return True if a rebuild is pending.
	 */
	[[nodiscard]] auto isRebuildPending() const -> bool { return m_pending; }

	/**
	 * @brief
	 *  Called just before `ImGui::DockSpace`: rebuild the tree when needed.
	 * @param[in] iDockSpaceId The dockspace.
	 * @param[in,out] ioSavedVersion Layout generation stored in the settings, set to `version` on rebuild.
	 * @return True if the tree was rebuilt.
	 */
	auto onDockSpace(uint32_t iDockSpaceId, int& ioSavedVersion) -> bool;

private:
	/// Arrangement used by the next rebuild.
	LayoutPreset m_preset = LayoutPreset::Default;
	/// A rebuild was requested.
	bool m_pending = false;
	/// The first frame was handled (the saved layout is only checked once).
	bool m_checked = false;
};

}// namespace owl::nest
