/**
 * @file IconBank.h
 * @author Silmaen
 * @date 10/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <unordered_map>

namespace owl::gui {
/// Theme colours used for dynamic SVG icon rendering.
struct OWL_API IconThemeColors {
	/// Primary icon colour (replaces white `#ffffff` in SVGs).
	math::vec4 primary{1, 1, 1, 1};
	/// Secondary/accent icon colour (replaces fuchsia `#ff00ff` in SVGs).
	/// Defaults to the Owl Nest amber/gold accent used for inner highlights.
	math::vec4 secondary{1.0f, 0.78f, 0.15f, 1.0f};
};

/**
 * @brief
 *  A texture atlas that packs multiple icons into a single GPU texture.
 *
 * Icons are loaded from SVG or image files, optionally with theme colour substitution,
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
	 * @brief
	 *  Default constructor.
	 */
	IconBank() = default;

	/**
	 * @brief
	 *  Build the atlas from a list of named icon file paths.
	 *
	 * SVG files are rasterized via lunasvg with theme colour substitution.
	 * PNG/JPG files are loaded via stb_image (fallback).
	 *
	 * @param[in] iIcons List of (name, file_path) pairs (`.svg`, `.png`, or `.jpg`).
	 * @param[in] iCellSize Target cell size for each icon in the atlas.
	 * @param[in] iColors Theme colours for SVG rendering.
	 */
	void build(const std::vector<std::pair<std::string, std::filesystem::path>>& iIcons, uint32_t iCellSize = 64,
			   const IconThemeColors& iColors = {});

	/**
	 * @brief
	 *  Rebuild the atlas with new theme colours.
	 *
	 * Re-renders all SVG icons using the stored icon list and new colours.
	 * @param[in] iColors The new theme colours.
	 */
	void rebuild(const IconThemeColors& iColors);

	/**
	 * @brief
	 *  Look up an icon by name.
	 * @param[in] iName The icon name.
	 * @return The icon info, or nullopt if not found.
	 */
	[[nodiscard]] auto getIcon(const std::string& iName) const -> std::optional<IconInfo>;

	/**
	 * @brief
	 *  Check if an icon exists in the bank.
	 * @param[in] iName The icon name.
	 * @return True if the icon is in the bank.
	 */
	[[nodiscard]] auto hasIcon(const std::string& iName) const -> bool;

	/**
	 * @brief
	 *  Get the atlas texture.
	 * @return The atlas texture.
	 */
	[[nodiscard]] auto getAtlasTexture() const -> const shared<renderer::gpu::Texture2D>& { return m_atlas; }

	/**
	 * @brief
	 *  Release the atlas texture and clear all entries.
	 */
	void clear();

	/**
	 * @brief
	 *  Access the global icon bank instance.
	 * @return Reference to the global IconBank.
	 */
	static auto instance() -> IconBank&;

	/**
	 * @brief
	 *  Render an ImGui menu item with an icon prefix.
	 * @param[in] iIconName Name of the icon in the bank.
	 * @param[in] iLabel Menu item label.
	 * @param[in] iShortcut Optional keyboard shortcut text.
	 * @param[in] iEnabled Whether the menu item is enabled.
	 * @return True if the menu item was clicked.
	 */
	auto menuItem(const std::string& iIconName, const char* iLabel, const char* iShortcut = nullptr,
				  bool iEnabled = true) const -> bool;

	/**
	 * @brief
	 *  Render an ImGui button with an icon prefix before the label.
	 *
	 * Falls back to a regular button (no icon) when the icon is missing. The icon
	 * scales with the current font size.
	 * @param[in] iIconName Name of the icon in the bank.
	 * @param[in] iLabel Button label displayed after the icon.
	 * @param[in] iSize Size passed to `ImGui::Button` (pass `{0, 0}` for auto-fit).
	 * @return True if the button was clicked.
	 */
	auto iconButton(const std::string& iIconName, const char* iLabel, const math::vec2& iSize = {0.f, 0.f}) const
			-> bool;

private:
	/// The atlas texture.
	shared<renderer::gpu::Texture2D> m_atlas;
	/// Mapping from icon name to UV coordinates {uv0, uv1}.
	std::unordered_map<std::string, std::pair<math::vec2, math::vec2>> m_uvMap;
	/// Stored icon list for rebuild.
	std::vector<std::pair<std::string, std::filesystem::path>> m_iconList;
	/// Stored cell size for rebuild.
	uint32_t m_cellSize = 64;
};

}// namespace owl::gui
