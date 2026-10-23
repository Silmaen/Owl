/**
 * @file UiImage.h
 * @author Silmaen
 * @date 10/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "renderer/gpu/Texture.h"

#include <array>
#include <cstdint>
#include <string>

namespace owl::scene::component {
/**
 * @brief
 *  UI image widget component.
 *
 * Renders a textured quad in screen space within a Canvas hierarchy.
 */
struct OWL_API UiImage {
	/// Texture to display.
	shared<renderer::gpu::Texture2D> texture = nullptr;
	/// Tint colour (multiplied with texture).
	math::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
	/// Name of a `RenderTarget` of the scene shown in place of the texture; empty for the texture.
	std::string renderTarget;
	/// Columns of the sprite sheet grid of the texture (1: the whole texture).
	uint32_t columns = 1;
	/// Rows of the sprite sheet grid of the texture.
	uint32_t rows = 1;
	/// Frame of the grid shown, row-major from the top left (0-based).
	uint32_t frame = 0;

	/**
	 * @brief
	 *  Texture coordinates of the shown frame.
	 * @return The four corners (bottom left, bottom right, top right, top left), as a `Quad2DData` takes them.
	 */
	[[nodiscard]] auto getFrameCoords() const -> std::array<math::vec2, 4>;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The display name of the component.
	 */
	static auto name() noexcept -> const char* { return "UI Image"; }

	/**
	 * @brief
	 *  Get the YAML key.
	 * @return The YAML serialization key.
	 */
	static auto key() noexcept -> const char* { return "UiImage"; }

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
