/**
 * @file AnimatedSpriteRenderer.h
 * @author Silmaen
 * @date 09/04/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Serializer.h"
#include "math/Curve.h"
#include "renderer/gpu/Texture.h"

namespace owl::scene::component {
/**
 * @brief
 *  Struct for drawing an animated sprite from a spritesheet.
 *
 * The spritesheet is a texture atlas arranged as a regular grid of frames.
 * Frames are numbered in row-major order starting from the top-left (frame 0).
 */
struct OWL_API AnimatedSpriteRenderer {
	/// Sprite colour tint.
	math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
	/// Spritesheet texture.
	shared<renderer::gpu::Texture2D> texture = nullptr;
	/// Number of columns in the spritesheet grid.
	uint32_t columns = 1;
	/// Number of rows in the spritesheet grid.
	uint32_t rows = 1;
	/// First frame index (0-based, row-major order, inclusive).
	uint32_t firstFrame = 0;
	/// Last frame index (inclusive).
	uint32_t lastFrame = 0;
	/// Duration of each frame in seconds.
	float frameDuration = 0.1f;
	/// Whether the animation loops.
	bool loop = true;
	/**
	 * @brief
	 *  Optional raycast-only world size override (in cells, X = width, Y = height).
	 *
	 * Mirrors `SpriteRenderer::raycastSize`: when either component is `<= 0`
	 * (the default `{0, 0}`), the billboard falls back to
	 * `Transform.scale.xy`. Lets a designer keep one editor scale and tweak
	 * the first-person size independently.
	 */
	math::vec2 raycastSize{0.0f, 0.0f};
	/**
	 * @brief
	 *  Additional world Z-offset applied to the billboard in raycast view (cells).
	 *
	 * Added on top of `Transform.translation.z`. Positive raises the sprite,
	 * negative lowers it. `0` (default) means "use only `translation.z`",
	 * preserving the PR2 behaviour.
	 */
	float raycastZOffset = 0.0f;
	/**
	 * @brief
	 *  Optional speed-remap curve sampled by playback progress.
	 *
	 * Empty curve keeps playback at constant speed (default behaviour). When non-empty,
	 * each frame's `dt` is multiplied by `speedCurve.evaluate(progress)` where `progress`
	 * is the normalized position inside the animation range (`m_currentFrame` relative
	 * to `[firstFrame, lastFrame]`). Negative values stall, values > 1 fast-forward.
	 */
	math::Curve speedCurve;

	/// Accumulated time since last frame advance (runtime only, not serialized).
	float m_elapsedTime = 0.0f;
	/// Currently displayed frame index (runtime only, not serialized).
	uint32_t m_currentFrame = 0;
	/// Whether the animation is currently playing (runtime only, not serialized).
	bool m_playing = true;

	/**
	 * @brief
	 *  Get the class title.
	 * @return The class title.
	 */
	static auto name() -> const char* { return "Animated Sprite"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "AnimatedSpriteRenderer"; }

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
