/**
 * @file UiLayout.h
 * @author Silmaen
 * @date 15/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::scene::component {
/**
 * @brief
 *  Automatic arrangement of the children of a HUD widget: a row, a column or a grid.
 *
 * The children carrying a `UiRect` are placed one after the other, in hierarchy order, inside the content area of the
 * widget (the whole rectangle, or the area under the title bar of a `UiWindow`), minus the padding. Each keeps the
 * `size` of its own `UiRect`; its anchors and offset no longer apply. Hidden children take no room. The widget itself
 * is not resized.
 */
struct OWL_API UiLayout {
	/// How the children are arranged.
	enum struct Arrangement : uint8_t {
		Row,///< Left to right on one line.
		Column,///< Top to bottom on one column.
		Grid,///< Left to right, `columns` per line, then the next line.
	};

	/// Placement of a child across the arrangement direction.
	enum struct CrossAlign : uint8_t {
		Start,///< Top of a row, left of a column or of a grid cell.
		Center,///< Centred.
		End,///< Bottom of a row, right of a column or of a grid cell.
	};

	/// How the children are arranged.
	Arrangement arrangement = Arrangement::Column;
	/// Placement of a child across the arrangement direction.
	CrossAlign crossAlign = CrossAlign::Start;
	/// Space between two children, in reference pixels.
	float spacing = 4.f;
	/// Space between the content area and the children, in reference pixels.
	float padding = 4.f;
	/// Number of cells per line of a grid (at least 1).
	uint32_t columns = 2;
	/// Size of a grid cell, in reference pixels; 0 on an axis takes the largest child on that axis.
	math::vec2 cellSize{0.f, 0.f};

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() noexcept -> const char* { return "UI Layout"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() noexcept -> const char* { return "UiLayout"; }

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
