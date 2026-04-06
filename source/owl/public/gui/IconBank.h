/**
 * @file IconBank.h
 * @author Silmaen
 * @date 10/03/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/Texture.h"

#include <unordered_map>

namespace owl::gui {

/**
 * @brief A texture atlas that packs multiple icons into a single GPU texture.
 *
 * Icons are loaded from individual image files, optionally resized to a uniform cell size,
 * and packed into a grid atlas. Each icon can be looked up by name to get the atlas texture ID
 * and UV coordinates for rendering with ImGui.
 */
class OWL_API IconBank {
public:
	/// Information needed to render a single icon from the atlas.
	struct IconInfo {
		/// The GPU texture ID of the atlas.
		uint64_t textureId{};
		/// UV coordinate of the top-left corner (with Y-flip for engine convention).
		math::vec2 uv0{0, 1};
		/// UV coordinate of the bottom-right corner (with Y-flip for engine convention).
		math::vec2 uv1{1, 0};
	};

	/**
	 * @brief Default constructor.
	 */
	IconBank() = default;

	/**
	 * @brief Build the atlas from a list of named icon file paths.
	 * @param[in] iIcons List of (name, file_path) pairs.
	 * @param[in] iCellSize Target cell size for each icon in the atlas (icons are resized to fit).
	 */
	void build(const std::vector<std::pair<std::string, std::filesystem::path>>& iIcons, uint32_t iCellSize = 128);

	/**
	 * @brief Look up an icon by name.
	 * @param[in] iName The icon name.
	 * @return The icon info, or nullopt if not found.
	 */
	[[nodiscard]] auto getIcon(const std::string& iName) const -> std::optional<IconInfo>;

	/**
	 * @brief Check if an icon exists in the bank.
	 * @param[in] iName The icon name.
	 * @return True if the icon is in the bank.
	 */
	[[nodiscard]] auto hasIcon(const std::string& iName) const -> bool;

	/**
	 * @brief Get the atlas texture.
	 * @return The atlas texture.
	 */
	[[nodiscard]] auto getAtlasTexture() const -> const shared<renderer::Texture2D>& { return m_atlas; }

	/**
	 * @brief Release the atlas texture and clear all entries.
	 */
	void clear();

	/**
	 * @brief Access the global icon bank instance.
	 * @return Reference to the global IconBank.
	 */
	static auto instance() -> IconBank&;

	/**
	 * @brief Render an ImGui menu item with an icon prefix.
	 * @param[in] iIconName Name of the icon in the bank.
	 * @param[in] iLabel Menu item label.
	 * @param[in] iShortcut Optional keyboard shortcut text.
	 * @param[in] iEnabled Whether the menu item is enabled.
	 * @return True if the menu item was clicked.
	 */
	auto menuItem(const std::string& iIconName, const char* iLabel, const char* iShortcut = nullptr,
				 bool iEnabled = true) const -> bool;

private:
	/// The atlas texture.
	shared<renderer::Texture2D> m_atlas;
	/// Mapping from icon name to UV coordinates {uv0, uv1}.
	std::unordered_map<std::string, std::pair<math::vec2, math::vec2>> m_uvMap;
};

}// namespace owl::gui
