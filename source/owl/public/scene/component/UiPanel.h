/**
 * @file UiPanel.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/vectors.h"

namespace owl::scene::component {
/**
 * @brief
 *  UI panel widget component.
 *
 * Renders a coloured rectangle background. Can optionally lay out children
 * vertically or horizontally.
 */
struct OWL_API UiPanel {
	/// Background colour.
	math::vec4 backgroundColor{0.2f, 0.2f, 0.2f, 0.8f};
	/// Border colour.
	math::vec4 borderColor{1.0f, 1.0f, 1.0f, 1.0f};
	/// Border width in pixels (0 = no border).
	float borderWidth = 0.f;
	/// Layout mode for children.
	enum struct Layout : uint8_t {
		None,///< No automatic layout.
		Vertical,///< Stack children vertically.
		Horizontal,///< Stack children horizontally.
	};
	/// Current layout mode.
	Layout layout = Layout::None;
	/// Spacing between children in pixels (for Vertical/Horizontal layout).
	float spacing = 0.f;
	/// Padding inside the panel in pixels.
	float padding = 0.f;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Panel"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiPanel"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The serializer used as output.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param[in] iNode The serializer wrapping the source YAML node.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
