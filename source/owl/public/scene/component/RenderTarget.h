/**
 * @file RenderTarget.h
 * @author Silmaen
 * @date 22/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "math/vectors.h"

#include <algorithm>
#include <string>
#include <vector>

namespace owl::renderer::gpu {
class Framebuffer;
}// namespace owl::renderer::gpu

namespace owl::scene::component {

/**
 * @brief
 *  Draws the view of the entity's camera into an off-screen image, shown by a HUD `UiImage` naming it: a minimap, a
 *  rear-view mirror, a security monitor.
 *
 * Each frame, before the main view, the scene renders the world through the `Camera` of the entity (which is not the
 * primary camera) into an image of `size` pixels, the overlays, the HUD and the `hiddenLayers` of the renderer stack
 * left out. A `UiImage` whose `renderTarget` is `targetName` displays it.
 */
struct OWL_API RenderTarget {
	/// Whether the image is drawn each frame; off keeps the last image.
	bool enabled = true;
	/// Name the `UiImage` of the HUD refers to.
	std::string targetName = "minimap";
	/// Size of the image, in pixels.
	math::vec2ui size{256, 256};
	/// Colour of the image where nothing is drawn.
	math::vec4 clearColor{0.f, 0.f, 0.f, 1.f};
	/// Names of the layers of the renderer stack the image leaves out (the markers drawn over the world).
	std::vector<std::string> hiddenLayers;

	/// Runtime: the image drawn into, made on the first frame.
	shared<renderer::gpu::Framebuffer> m_framebuffer;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Render Target"; }

	/**
	 * @brief
	 *  Whether the image draws a layer of the renderer stack.
	 * @param[in] iLayer The name of the layer.
	 * @return False when the layer is one of `hiddenLayers`.
	 */
	[[nodiscard]] auto drawsLayer(const std::string& iLayer) const -> bool {
		return std::ranges::find(hiddenLayers, iLayer) == hiddenLayers.end();
	}

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "RenderTarget"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);
};

}// namespace owl::scene::component
