/**
 * @file Canvas.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"

namespace owl::scene::component {
/**
 * @brief
 *  Canvas component — marks an entity as a UI root.
 *
 * Child entities of a Canvas are rendered in screen space using an orthographic
 * projection. The Canvas defines coordinate space and draw order.
 */
struct OWL_API Canvas {
	/// Coordinate space for the Canvas.
	enum struct Space : uint8_t {
		ScreenOverlay,///< Screen-space overlay (always on top of scene).
	};
	/// The coordinate space.
	Space space = Space::ScreenOverlay;
	/// Sort order — higher values render on top of lower values.
	int32_t sortOrder = 0;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Canvas"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Canvas"; }

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
