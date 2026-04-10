/**
 * @file UIPanel.h
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
 * @brief UI panel widget component.
 *
 * Renders a colored rectangle background. Can optionally lay out children
 * vertically or horizontally.
 */
struct OWL_API UIPanel {
	/// Background color.
	math::vec4 backgroundColor{0.2f, 0.2f, 0.2f, 0.8f};
	/// Border color.
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

	/// @brief Get the class title.
	static auto name() -> const char* { return "UI Panel"; }
	/// @brief Get the YAML key.
	static auto key() -> const char* { return "UIPanel"; }

	/// @brief Write this component to a YAML context.
	void serialize(const core::Serializer& iOut) const;
	/// @brief Read this component from YAML node.
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
