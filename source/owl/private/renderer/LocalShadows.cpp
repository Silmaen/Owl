/**
 * @file LocalShadows.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "renderer/LocalShadows.h"

#include "math/matrixCreation.h"
#include "renderer/ClipSpace.h"
#include "renderer/gpu/RenderCommand.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <tuple>
#include <vector>

namespace owl::renderer {

namespace {

// Look direction and up vector of each cube face, the convention every graphics API samples cubes with.
struct FaceAxes {
	math::vec3 forward;
	math::vec3 up;
};

constexpr std::array<FaceAxes, 6> g_faceAxes{{{.forward = {1.f, 0.f, 0.f}, .up = {0.f, -1.f, 0.f}},
											  {.forward = {-1.f, 0.f, 0.f}, .up = {0.f, -1.f, 0.f}},
											  {.forward = {0.f, 1.f, 0.f}, .up = {0.f, 0.f, 1.f}},
											  {.forward = {0.f, -1.f, 0.f}, .up = {0.f, 0.f, -1.f}},
											  {.forward = {0.f, 0.f, 1.f}, .up = {0.f, -1.f, 0.f}},
											  {.forward = {0.f, 0.f, -1.f}, .up = {0.f, -1.f, 0.f}}}};

auto cameraWorld(const math::vec3& iPosition, const math::vec3& iForward, const math::vec3& iUp) -> math::mat4 {
	const math::vec3 right = (iForward ^ iUp).normalized();
	const math::vec3 up = right ^ iForward;
	math::mat4 world = math::identity<float, 4>();
	for (uint32_t row = 0; row < 3; ++row) {
		world(row, 0) = right[row];
		world(row, 1) = up[row];
		world(row, 2) = -iForward[row];
		world(row, 3) = iPosition[row];
	}
	return world;
}

auto farPlane(const float iRange) -> float { return std::max(iRange, 2.f * g_localShadowNear); }

}// namespace

auto getLocalShadowBudget() -> LocalShadowBudget {
	return gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::OpenGL ? g_openGlLocalShadowBudget
																		: g_vulkanLocalShadowBudget;
}

auto computePointShadowFaces(const math::vec3& iPosition, const float iRange) -> PointShadowFaces {
	PointShadowFaces result;
	const math::mat4 projection =
			math::perspectiveReversedZ(std::numbers::pi_v<float> / 2.f, 1.f, g_localShadowNear, farPlane(iRange));
	for (size_t face = 0; face < g_faceAxes.size(); ++face) {
		result.cameras[face] = Camera{projection};
		result.cameras[face].setTransform(cameraWorld(iPosition, g_faceAxes[face].forward, g_faceAxes[face].up));
	}
	result.depth = math::vec2{-projection(2, 2), projection(2, 3)};
	return result;
}

auto computeSpotShadowCamera(const math::vec3& iPosition, const math::vec3& iDirection, const float iOuterAngle,
							 const float iRange) -> Camera {
	constexpr float degree = std::numbers::pi_v<float> / 180.f;
	const float halfAngle = std::clamp(iOuterAngle, degree, 85.f * degree);
	const math::vec3 forward = iDirection.normSq() > 1e-8f ? iDirection.normalized() : math::vec3{0.f, 0.f, -1.f};
	const math::vec3 worldUp = std::abs(forward.y()) > 0.99f ? math::vec3{1.f, 0.f, 0.f} : math::vec3{0.f, 1.f, 0.f};
	Camera camera{
			toActiveClipSpace(math::perspectiveReversedZ(2.f * halfAngle, 1.f, g_localShadowNear, farPlane(iRange)))};
	camera.setTransform(cameraWorld(iPosition, forward, worldUp));
	return camera;
}

void rankShadowCandidates(std::vector<ShadowCandidate>& ioCandidates, const uint32_t iBudget) {
	const auto byImportance = [](const ShadowCandidate& iFirst, const ShadowCandidate& iSecond) -> bool {
		return std::tuple{iFirst.distance, -iFirst.intensity, iFirst.lightIndex} <
			   std::tuple{iSecond.distance, -iSecond.intensity, iSecond.lightIndex};
	};
	const auto kept = std::min<size_t>(iBudget, ioCandidates.size());
	std::ranges::partial_sort(ioCandidates, ioCandidates.begin() + static_cast<std::ptrdiff_t>(kept), byImportance);
	ioCandidates.resize(kept);
}

auto distanceToLightRange(const math::vec3& iCamera, const math::vec3& iLight, const float iRange) -> float {
	return std::max((iCamera - iLight).norm() - std::max(iRange, 0.f), 0.f);
}

}// namespace owl::renderer
