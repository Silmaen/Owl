/**
 * @file OutlinePass.h
 * @author Silmaen
 * @date 23/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "math/vectors.h"
#include "renderer/gpu/Texture.h"

namespace owl::renderer {

/**
 * @brief
 *  Inputs of one outline: the silhouette to outline, the scene it stands in, how it is drawn.
 */
struct OutlineInputs {
	/// Depth (reversed-Z, `g_farDepth` where empty) of the outlined meshes alone: their silhouette.
	shared<gpu::Texture2D> mask;
	/// Depth of the whole 3D scene, to dim the edges behind other geometry; null draws every edge in full.
	shared<gpu::Texture2D> sceneDepth;
	/// Colour of the outline; its alpha scales the whole line.
	math::vec4 color{1.f, 0.8f, 0.35f, 1.f};
	/// Opacity of the edges hidden behind other geometry, against the visible ones (1).
	float hiddenOpacity = 0.35f;
	/// Width of the line, in pixels.
	float width = 2.f;
	/// Size of the frame, in pixels.
	math::vec2ui frameSize{1, 1};
};

/**
 * @brief
 *  Draws the outline of a silhouette over the bound target: the pixels just outside the mask, in full where the
 *  outlined meshes are in front, dimmed where other geometry hides them.
 */
class OWL_API OutlinePass final {
public:
	/**
	 * @brief
	 *  Create the full-screen draw and its uniform buffer.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the draw and its uniform buffer.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Draw an outline over the bound target, alpha blended.
	 * @param[in] iInputs The silhouette, the scene depth and the look of the line.
	 */
	static void draw(const OutlineInputs& iInputs);
};

}// namespace owl::renderer
