/**
 * @file PixelArt.h
 * @author Silmaen
 * @date 01/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "gpu/Texture.h"
#include "math/vectors.h"

#include <cstdint>
#include <string>

namespace owl::renderer {

/**
 * @brief
 *  How a 3D layer is drawn as pixel art: at a low resolution, shown with nearest filtering, its colours reduced and
 *  its silhouettes outlined, so that 3D actors match pixel-art sprites and backdrops.
 */
struct OWL_API PixelArtSettings {
	/// Height of the low-resolution image, in texels (0 draws the layer at the resolution of the frame).
	uint32_t height = 0;
	/// Levels of each colour channel (below 2 keeps the colours).
	uint32_t colorLevels = 0;
	/// Colour of the outline around the silhouettes (sRGB as authored); an alpha of 0 draws none.
	math::vec4 outline{0.f, 0.f, 0.f, 0.f};

	/**
	 * @brief
	 *  Check if the layer is drawn as pixel art.
	 * @return True with a height.
	 */
	[[nodiscard]] auto isActive() const -> bool { return height > 0; }

	/**
	 * @brief
	 *  Get the size of the low-resolution image for a frame: `height` texels high, the width of the frame's aspect.
	 * @param[in] iFrameSize Size of the frame, in pixels.
	 * @return The size in texels, never larger than the frame nor smaller than one texel.
	 */
	[[nodiscard]] auto getTargetSize(const math::vec2ui& iFrameSize) const -> math::vec2ui;

	/**
	 * @brief
	 *  Read the settings from the configuration of a layer (`PixelHeight`, `PixelColorLevels`, `PixelOutline`).
	 * @param[in] iConfig YAML text of the configuration map.
	 * @param[in] iFallback Settings kept for the keys the configuration does not give.
	 * @return The settings.
	 */
	[[nodiscard]] static auto fromConfig(const std::string& iConfig, const PixelArtSettings& iFallback)
			-> PixelArtSettings;
};

/**
 * @brief
 *  The pixel-art composite: draws a layer rendered at a low resolution over the frame, texel by texel, with its
 *  depth, so that the layers drawn after it test against it as against the full-resolution meshes.
 */
class OWL_API PixelArtPass final {
public:
	/**
	 * @brief
	 *  Inputs of one composite.
	 */
	struct Inputs {
		/// Colour of the low-resolution layer.
		shared<gpu::Texture2D> color;
		/// Depth of the low-resolution layer (`RenderGraph::TargetFormat::Depth`), 0 where nothing was drawn.
		shared<gpu::Texture2D> depth;
		/// Size of the low-resolution layer, in texels.
		math::vec2ui size{1, 1};
		/// Size of the frame, in pixels.
		math::vec2ui frameSize{1, 1};
		/// Colour reduction and outline.
		PixelArtSettings settings;
	};

	/**
	 * @brief
	 *  Create the composite draw (after the render API).
	 */
	static void init();

	/**
	 * @brief
	 *  Release the composite draw.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Draw the low-resolution layer over the bound target, colour, entity id (-1) and depth.
	 * @param[in] iInputs The layer and its settings.
	 */
	static void draw(const Inputs& iInputs);
};

}// namespace owl::renderer
