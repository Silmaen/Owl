/**
 * @file LocalShadows.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "Camera.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace owl::renderer {

/**
 * @brief
 *  How many shadows of point and spot lights a backend samples in one frame.
 */
struct LocalShadowBudget {
	/// Spot lights casting a shadow (one 2D map each).
	uint32_t spot = 0;
	/// Point lights casting a shadow (one cube each).
	uint32_t point = 0;
};

/// Budget of OpenGL: its fragment shaders sample 32 textures at most, the 2D slots and the cubes together.
constexpr LocalShadowBudget g_openGlLocalShadowBudget{.spot = 4, .point = 4};

/// Budget of Vulkan and of the null backend.
constexpr LocalShadowBudget g_vulkanLocalShadowBudget{.spot = 16, .point = 16};

/// Size of the table of local shadows the 3D shaders read: the largest budget, spot and point together.
constexpr uint32_t g_maxLocalShadows = g_vulkanLocalShadowBudget.spot + g_vulkanLocalShadowBudget.point;

/// Near plane of the shadow cameras of local lights, in world units.
constexpr float g_localShadowNear = 0.05f;

/**
 * @brief
 *  Get the local shadow budget of the active render API.
 * @return The budget.
 */
[[nodiscard]] OWL_API auto getLocalShadowBudget() -> LocalShadowBudget;

/**
 * @brief
 *  The six cameras rendering the cube shadow of a point light, and how a shader rebuilds their depth.
 */
struct PointShadowFaces {
	/// Camera of each face, in the cube order +X, -X, +Y, -Y, +Z, -Z (90 degree perspective, NDC Y up).
	std::array<Camera, 6> cameras;
	/**
	 * @brief
	 *  Depth a face stores for a point at `d` along its axis from the light: `depth.x() + depth.y() / d` (the
	 *  normalized reversed-Z depth of the face projection).
	 */
	math::vec2 depth{0.f, 0.f};
};

/**
 * @brief
 *  Compute the face cameras of the cube shadow of a point light.
 *
 * The projections keep NDC Y up on every backend: the rows of each face then match the cube sampling convention
 * once the face is copied into the cube (`gpu::Framebuffer::copyAttachmentToCube`).
 * @param[in] iPosition World position of the light.
 * @param[in] iRange Range of the light, the far plane of the faces.
 * @return The cameras and the depth reconstruction.
 */
[[nodiscard]] OWL_API auto computePointShadowFaces(const math::vec3& iPosition, float iRange) -> PointShadowFaces;

/**
 * @brief
 *  Compute the camera rendering the shadow map of a spot light: a square perspective covering its outer cone.
 * @param[in] iPosition World position of the light.
 * @param[in] iDirection Direction the light shines (normalized by the computation).
 * @param[in] iOuterAngle Half angle of the outer cone, in radians (clamped to 1..85 degrees).
 * @param[in] iRange Range of the light, the far plane of the camera.
 * @return The camera, in the active clip space.
 */
[[nodiscard]] OWL_API auto computeSpotShadowCamera(const math::vec3& iPosition, const math::vec3& iDirection,
												   float iOuterAngle, float iRange) -> Camera;

/**
 * @brief
 *  A light that may cast a shadow this frame.
 */
struct ShadowCandidate {
	/// Index of the light in the lights given to the renderer.
	uint32_t lightIndex = 0;
	/// Distance from the camera to the sphere the light reaches, 0 when the camera is inside it.
	float distance = 0.f;
	/// Strength of the light, for lights at the same distance.
	float intensity = 0.f;
};

/**
 * @brief
 *  Keep the lights that get a shadow: the nearest to the camera first, then the strongest, at most `iBudget`.
 * @param[in,out] ioCandidates The candidates, sorted by importance and cut to the budget.
 * @param[in] iBudget Number of shadows available.
 */
OWL_API void rankShadowCandidates(std::vector<ShadowCandidate>& ioCandidates, uint32_t iBudget);

/**
 * @brief
 *  Distance from a point to the sphere a light reaches.
 * @param[in] iCamera The point (the camera position).
 * @param[in] iLight Position of the light.
 * @param[in] iRange Range of the light.
 * @return The distance, 0 inside the sphere.
 */
[[nodiscard]] OWL_API auto distanceToLightRange(const math::vec3& iCamera, const math::vec3& iLight, float iRange)
		-> float;

}// namespace owl::renderer
