/**
 * @file UiList.h
 * @author Silmaen
 * @date 16/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::scene::component {

/**
 * @brief
 *  What a row of a `UiList` shows: filled from Lua for the rows in view only.
 */
struct OWL_API UiListRow {
	/// Main text, left aligned.
	std::string text;
	/// Secondary text, right aligned.
	std::string detail;
	/// Icon drawn at the left of the row (none when null).
	shared<renderer::gpu::Texture2D> icon;
	/// Whether the row was filled (or asked for, which `UiList::onBindCallback` does once).
	bool bound = false;
};

/**
 * @brief
 *  HUD virtualised list: a model of `itemCount` rows of which only the ones in view are drawn and bound.
 *
 * The widget is a vertical scroll container (wheel, bar, focus) whose rows are fixed-height lines drawn by the engine.
 * The rows hold no entity: a script gives the count (`ui.list_set_count`) and fills the rows (`ui.list_set_row`), the
 * ones in view only, in the `onBindCallback` the engine calls for a row in view that nobody filled yet (it reads
 * `_list_entity` and `_row_index`, 1-based). A row not filled shows `Item N`. Selecting a row (click, or `ui_up` /
 * `ui_down` on the focused list) runs `onSelectCallback`, which reads `_list_entity` and `_selected_index`.
 */
struct OWL_API UiList {
	/// Number of rows of the model.
	uint32_t itemCount = 0;
	/// Height of a row, in reference pixels.
	float rowHeight = 28.f;
	/// Space between two rows, in reference pixels.
	float rowSpacing = 2.f;
	/// Selected row, 0 based (-1 for none).
	int32_t selectedIndex = -1;
	/// Distance the rows are scrolled by, in reference pixels (kept inside the content by the layout).
	float scrollOffset = 0.f;
	/// Scroll distance of one wheel notch, in reference pixels.
	float wheelStep = 56.f;
	/// Font size of the row texts, in reference pixels.
	float fontSize = 14.f;
	/// Colour of the rows.
	math::vec4 rowColor{0.16f, 0.17f, 0.21f, 0.95f};
	/// Colour of every other row.
	math::vec4 rowAltColor{0.19f, 0.20f, 0.25f, 0.95f};
	/// Colour of the row under the mouse.
	math::vec4 hoverColor{0.26f, 0.30f, 0.40f, 1.f};
	/// Colour of the selected row.
	math::vec4 selectedColor{0.20f, 0.36f, 0.62f, 1.f};
	/// Colour of the texts.
	math::vec4 textColor{1.f, 1.f, 1.f, 1.f};
	/// Whether the bar is drawn (and takes room) when the rows overflow.
	bool showScrollBar = true;
	/// Thickness of the bar, in reference pixels.
	float scrollBarWidth = 8.f;
	/// Colour of the bar track.
	math::vec4 trackColor{1.f, 1.f, 1.f, 0.08f};
	/// Colour of the bar thumb.
	math::vec4 thumbColor{1.f, 1.f, 1.f, 0.45f};
	/// Lua function called when the selection changes (reads `_list_entity`, `_selected_index`; 1-based).
	std::string onSelectCallback;
	/// Lua function called once for a row in view that was not filled (reads `_list_entity`, `_row_index`; 1-based).
	std::string onBindCallback;

	/// Rows filled so far, `itemCount` of them (runtime only).
	std::vector<UiListRow> rows;
	/// Row under the mouse, -1 for none (runtime only).
	int32_t hoverIndex = -1;
	/// Whether the next layout scrolls the selected row into view (runtime only).
	bool revealSelected = false;
	/// Textures loaded for the row icons, by path (runtime only).
	std::unordered_map<std::string, shared<renderer::gpu::Texture2D>> iconCache;

	/**
	 * @brief
	 *  Change the number of rows; the rows kept stay filled, a selection past the end is dropped.
	 * @param[in] iCount The new number of rows.
	 */
	void setCount(uint32_t iCount);

	/**
	 * @brief
	 *  Forget what the rows show, so the visible ones are bound again.
	 */
	void invalidateRows();

	/**
	 * @brief
	 *  Select a row.
	 * @param[in] iIndex The row, 0 based; anything out of range selects none.
	 * @return True when the selection changed.
	 */
	auto select(int64_t iIndex) -> bool;

	/**
	 * @brief
	 *  Get the content of a row, for drawing.
	 * @param[in] iIndex The row, 0 based.
	 * @return The row, or nullptr when it was never filled.
	 */
	[[nodiscard]] auto findRow(size_t iIndex) const -> const UiListRow*;

	/**
	 * @brief
	 *  Load an icon, once per path.
	 * @param[in] iPath The texture path as the scene files write it.
	 * @return The texture, null when the path is empty.
	 */
	auto loadIcon(const std::string& iPath) -> shared<renderer::gpu::Texture2D>;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI List"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiList"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
