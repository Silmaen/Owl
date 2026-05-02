/**
 * @file RendererTag.h
 * @author Silmaen
 * @date 30/04/2026
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"

namespace owl::scene::component {

/**
 * @brief Routes an entity to a specific layer in the project's renderer stack.
 *
 * The string `rendererName` matches a `Name` from the project `RendererStack`.
 * An entity without this component is rendered by the **first** layer of the
 * active stack (the implicit `Renderer2D` in mono-renderer projects). An
 * unknown name causes the entity to be skipped with a one-shot warning per
 * scene activation.
 *
 * Per-entity routing is wired progressively as additional layer types land
 * (raycasting in v0.2.0, voxel in v0.2.1). For projects that use only
 * `Renderer2D` the component has no observable effect.
 */
struct OWL_API RendererTag {
	/// Name of the renderer instance from the project stack.
	std::string rendererName;

	/**
	 * @brief Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Renderer Tag"; }

	/**
	 * @brief Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "RendererTag"; }

	/**
	 * @brief Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
