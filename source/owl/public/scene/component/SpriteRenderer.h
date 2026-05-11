/**
 * @file SpriteRenderer.h
 * @author Silmaen
 * @date 22/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "renderer/gpu/Texture.h"

namespace owl::scene::component {
/**
 * @brief
 *  Struct for draw a quad sprite.
 */
struct OWL_API SpriteRenderer {
	/// Sprite colour.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Sprite's texture.
	shared<renderer::gpu::Texture2D> texture = nullptr;
	/// Texture tiling factor (X and Y).
	math::vec2 tilingFactor{1.0f, 1.0f};
	/**
	 * @brief
	 *  Optional raycast-only world size override (in cells, X = width, Y = height).
	 *
	 * Only consulted when the sprite is rendered through `RendererRaycast` as a
	 * billboard. When either component is `<= 0` (the default `{0, 0}`), the
	 * billboard falls back to `Transform.scale.xy`. Setting this lets a level
	 * designer keep an entity at its 2D editor scale (top-down preview) while
	 * adjusting how big it appears in first-person.
	 */
	math::vec2 raycastSize{0.0f, 0.0f};
	/**
	 * @brief
	 *  Additional world Z-offset applied to the billboard in raycast view (cells).
	 *
	 * Added on top of `Transform.translation.z` when the raycast renderer
	 * computes the on-screen vertical centre. Positive values raise the
	 * sprite (lamps, ceiling decals); negative values lower it (floor
	 * stains, half-buried props). `0` is "use the entity's `translation.z`"
	 * as before — backward-compatible with scenes authored against PR2.
	 */
	float raycastZOffset = 0.0f;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Sprite Renderer"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "SpriteRenderer"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from YAML node.
	 * @param iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
