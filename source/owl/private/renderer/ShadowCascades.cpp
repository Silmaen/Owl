/**
 * @file ShadowCascades.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/ShadowCascades.h"

#include "math/matrixCreation.h"
#include "renderer/gpu/RenderCommand.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace owl::renderer {

namespace {
// Distance the light camera is pulled back behind each cascade, so off-screen casters still cast.
constexpr float g_minCasterPullBack = 20.f;

auto toVec3(const math::vec4& iPoint) -> math::vec3 {
	return {iPoint.x() / iPoint.w(), iPoint.y() / iPoint.w(), iPoint.z() / iPoint.w()};
}

auto lightProjection(const float iRadius, const float iFar) -> math::mat4 {
	math::mat4 projection = math::ortho(-iRadius, iRadius, -iRadius, iRadius, 0.f, iFar);
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan) {
		auto biasMatrix = math::identity<float, 4>();
		biasMatrix(2, 2) = 0.5f;
		biasMatrix(2, 3) = 0.5f;
		projection = biasMatrix * projection;
		projection(1, 1) *= -1.f;
	}
	return projection;
}

}// namespace

auto computeShadowCascades(const Camera& iCamera, const ShadowCascadeSettings& iSettings) -> ShadowCascades {
	ShadowCascades result;
	if (iSettings.lightDirection.normSq() < 1e-8f)
		return result;
	const math::vec3 forward = iSettings.lightDirection.normalized();
	const math::vec3 worldUp = std::abs(forward.y()) > 0.99f ? math::vec3{1.f, 0.f, 0.f} : math::vec3{0.f, 1.f, 0.f};
	const math::vec3 right = (forward ^ worldUp).normalized();
	const math::vec3 up = right ^ forward;

	// The frustum corners at the near and far planes, from the inverse view-projection.
	const bool vulkan = gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Vulkan;
	const float nearNdc = vulkan ? 0.f : -1.f;
	const math::mat4 inverseViewProjection = inverse(iCamera.getViewProjection());
	std::array<math::vec3, 4> nearCorners{};
	std::array<math::vec3, 4> farCorners{};
	for (uint32_t corner = 0; corner < 4; ++corner) {
		const float x = (corner & 1u) != 0 ? 1.f : -1.f;
		const float y = (corner & 2u) != 0 ? 1.f : -1.f;
		nearCorners[corner] = toVec3(inverseViewProjection * math::vec4{x, y, nearNdc, 1.f});
		farCorners[corner] = toVec3(inverseViewProjection * math::vec4{x, y, 1.f, 1.f});
	}
	const math::vec4 viewAxis = iCamera.getView().row(2);
	const auto viewDepth = [&viewAxis](const math::vec3& iPoint) -> float {
		return -(viewAxis.x() * iPoint.x() + viewAxis.y() * iPoint.y() + viewAxis.z() * iPoint.z() + viewAxis.w());
	};
	const float nearDepth = viewDepth(nearCorners[0]);
	const float farDepth = viewDepth(farCorners[0]);
	if (farDepth <= nearDepth)
		return result;
	const float maxDepth = std::clamp(iSettings.shadowDistance, nearDepth + 0.01f, farDepth);
	const uint32_t count = std::clamp(iSettings.cascadeCount, 1u, g_maxShadowCascades);
	const float safeNear = std::max(nearDepth, 0.01f);
	const auto resolution = static_cast<float>(std::max(iSettings.resolution, 1u));

	float sliceStart = nearDepth;
	for (uint32_t index = 0; index < count; ++index) {
		const float fraction = static_cast<float>(index + 1) / static_cast<float>(count);
		const float uniformSplit = nearDepth + (maxDepth - nearDepth) * fraction;
		const float logSplit = safeNear * std::pow(maxDepth / safeNear, fraction);
		const float sliceEnd = iSettings.splitBlend * logSplit + (1.f - iSettings.splitBlend) * uniformSplit;

		// Slice corners: the depth is linear along each corner ray, from the near to the far plane.
		std::array<math::vec3, 8> slice{};
		const float startT = (sliceStart - nearDepth) / (farDepth - nearDepth);
		const float endT = (sliceEnd - nearDepth) / (farDepth - nearDepth);
		math::vec3 centre{0.f, 0.f, 0.f};
		for (uint32_t corner = 0; corner < 4; ++corner) {
			const math::vec3 ray = farCorners[corner] - nearCorners[corner];
			slice[corner] = nearCorners[corner] + ray * startT;
			slice[corner + 4] = nearCorners[corner] + ray * endT;
		}
		for (const auto& point: slice) centre += point;
		centre = centre * (1.f / 8.f);
		float radius = 0.f;
		for (const auto& point: slice) radius = std::max(radius, (point - centre).norm());
		radius = std::ceil(radius * 16.f) / 16.f;

		// Snap the centre to the texel grid of the light view.
		const float texel = 2.f * radius / resolution;
		const float alongRight = centre * right;
		const float alongUp = centre * up;
		centre += right * (std::floor(alongRight / texel) * texel - alongRight) +
				  up * (std::floor(alongUp / texel) * texel - alongUp);

		const float pullBack = std::max(radius, g_minCasterPullBack);
		const math::vec3 eye = centre - forward * (radius + pullBack);
		math::mat4 lightWorld = math::identity<float, 4>();
		for (uint32_t row = 0; row < 3; ++row) {
			lightWorld(row, 0) = right[row];
			lightWorld(row, 1) = up[row];
			lightWorld(row, 2) = -forward[row];
			lightWorld(row, 3) = eye[row];
		}
		auto& cascade = result.cascades[index];
		cascade.camera = Camera{lightProjection(radius, 2.f * radius + pullBack)};
		cascade.camera.setTransform(lightWorld);
		cascade.splitDepth = sliceEnd;
		cascade.texelWorldSize = texel;
		sliceStart = sliceEnd;
	}
	result.count = count;
	return result;
}

}// namespace owl::renderer
