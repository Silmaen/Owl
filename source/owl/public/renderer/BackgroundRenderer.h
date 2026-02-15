/**
 * @file BackgroundRenderer.h
 * @author Silmaen
 * @date 02/15/26
 * Copyright © 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Texture.h"
#include "math/matrices.h"

namespace owl::renderer {

/**
 * @brief Static renderer for drawing fullscreen background / skybox.
 *
 * Background rendering is deferred: drawBackground() stores pending data,
 * and the actual GPU drawing happens during Renderer2D::flush() to share
 * the same render pass (critical for Vulkan's DONT_CARE loadOp).
 */
class OWL_API BackgroundRenderer {
public:
	BackgroundRenderer() = default;
	BackgroundRenderer(const BackgroundRenderer&) = delete;
	BackgroundRenderer(BackgroundRenderer&&) = delete;
	auto operator=(const BackgroundRenderer&) -> BackgroundRenderer& = delete;
	auto operator=(BackgroundRenderer&&) -> BackgroundRenderer& = delete;

	/**
	 * @brief Destructor.
	 */
	~BackgroundRenderer() = default;

	/**
	 * @brief Initialize the background renderer.
	 */
	static void init();

	/**
	 * @brief Terminate the background renderer.
	 */
	static void shutdown();

	/**
	 * @brief Data needed to draw the background.
	 */
	struct BackgroundData {
		/// 0=SolidColor, 1=Gradient, 2=Texture, 3=Skybox.
		int mode = 0;
		/// Main color / bottom color for gradient.
		math::vec4 color{0.2f, 0.3f, 0.8f, 1.0f};
		/// Top color for gradient.
		math::vec4 topColor{0.8f, 0.9f, 1.0f, 1.0f};
		/// Inverse of the camera view rotation matrix (for skybox).
		math::mat4 inverseViewRotation = math::identity<float, 4>();
		/// Texture (background or equirectangular for skybox).
		shared<Texture2D> texture = nullptr;
	};

	/**
	 * @brief Store background data for deferred rendering.
	 * @param[in] iData The background data.
	 */
	static void drawBackground(const BackgroundData& iData);

	/**
	 * @brief Check if there is pending background data to draw.
	 * @return True if drawBackground() was called this frame.
	 */
	static auto hasPending() -> bool;

	/**
	 * @brief Get the pending background texture (for registering in Renderer2D's texture slots).
	 * @return The texture, or nullptr if not applicable.
	 */
	static auto getPendingTexture() -> shared<Texture2D>;

	/**
	 * @brief Flush pending background draw commands within an active batch.
	 * @param[in] iTexIndex The texture slot index assigned by Renderer2D.
	 *
	 * Must be called between beginBatch/endBatch (inside Renderer2D::flush).
	 * Clears pending state after drawing.
	 */
	static void flushPending(float iTexIndex);
};

}// namespace owl::renderer
