/**
 * @file LocalShadows_test.cpp
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/linAlgebra.h>
#include <renderer/LocalShadows.h>
#include <renderer/Renderer3D.h>
#include <renderer/gpu/RenderCommand.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

using namespace owl;
using namespace owl::renderer;

namespace {

auto project(const Camera& iCamera, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 clip =
			iCamera.getProjection() * iCamera.getView() * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w()};
}

// Face coordinates of a direction in its cube face, as the graphics APIs sample cubes (OpenGL 4.6, table 8.19).
struct FaceCoordinates {
	float s;
	float t;
};

auto cubeCoordinates(const size_t iFace, const math::vec3& iDirection) -> FaceCoordinates {
	const float x = iDirection.x();
	const float y = iDirection.y();
	const float z = iDirection.z();
	switch (iFace) {
		case 0:
			return {-z / std::abs(x), -y / std::abs(x)};
		case 1:
			return {z / std::abs(x), -y / std::abs(x)};
		case 2:
			return {x / std::abs(y), z / std::abs(y)};
		case 3:
			return {x / std::abs(y), -z / std::abs(y)};
		case 4:
			return {x / std::abs(z), -y / std::abs(z)};
		default:
			return {-x / std::abs(z), -y / std::abs(z)};
	}
}

}// namespace

TEST(LocalShadows, OpenGlBudgetFitsThirtyTwoSamplers) {
	EXPECT_EQ(Renderer3D::g_spotShadowSlot + g_openGlLocalShadowBudget.spot + 2 * g_openGlLocalShadowBudget.point, 32u);
	EXPECT_EQ(g_maxLocalShadows, g_vulkanLocalShadowBudget.spot + g_vulkanLocalShadowBudget.point);
}

TEST(LocalShadows, NullBackendHasTheVulkanBudget) {
	core::Log::init(core::Log::Level::Off);
	gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	const auto budget = getLocalShadowBudget();
	EXPECT_EQ(budget.spot, g_vulkanLocalShadowBudget.spot);
	EXPECT_EQ(budget.point, g_vulkanLocalShadowBudget.point);
	gpu::RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(LocalShadows, PointFacesFollowTheCubeSamplingConvention) {
	const math::vec3 light{1.f, 2.f, -3.f};
	const auto faces = computePointShadowFaces(light, 8.f);
	const std::array<math::vec3, 6> offsets{math::vec3{2.f, 0.6f, -0.8f}, math::vec3{-2.f, -0.4f, 0.7f},
											math::vec3{0.5f, 2.f, 0.9f},  math::vec3{-0.3f, -2.f, 0.4f},
											math::vec3{0.8f, -0.5f, 2.f}, math::vec3{-0.6f, 0.7f, -2.f}};
	for (size_t face = 0; face < offsets.size(); ++face) {
		const auto ndc = project(faces.cameras[face], light + offsets[face]);
		const auto [s, t] = cubeCoordinates(face, offsets[face]);
		EXPECT_NEAR(ndc.x(), s, 1e-4f) << "face " << face;
		EXPECT_NEAR(ndc.y(), t, 1e-4f) << "face " << face;
	}
}

TEST(LocalShadows, PointFaceDepthIsRebuiltFromTheAxisDistance) {
	const math::vec3 light{0.f, 1.f, 0.f};
	const auto faces = computePointShadowFaces(light, 10.f);
	for (const float distance: {0.2f, 1.f, 4.f, 9.5f}) {
		const auto ndc = project(faces.cameras[3], light + math::vec3{0.3f, -distance, 0.1f});
		EXPECT_NEAR(ndc.z(), faces.depth.x() + faces.depth.y() / distance, 1e-5f) << "distance " << distance;
	}
	const auto nearPoint = project(faces.cameras[0], light + math::vec3{g_localShadowNear, 0.f, 0.f});
	EXPECT_NEAR(nearPoint.z(), 1.f, 1e-4f);
	const auto farPoint = project(faces.cameras[0], light + math::vec3{10.f, 0.f, 0.f});
	EXPECT_NEAR(farPoint.z(), 0.f, 1e-4f);
}

TEST(LocalShadows, SpotCameraCoversTheOuterCone) {
	core::Log::init(core::Log::Level::Off);
	gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	const math::vec3 light{0.f, 3.f, 0.f};
	const float outer = 0.5f;
	const auto camera = computeSpotShadowCamera(light, {0.f, -2.f, 0.f}, outer, 6.f);
	const auto centre = project(camera, {0.f, 0.f, 0.f});
	EXPECT_NEAR(centre.x(), 0.f, 1e-5f);
	EXPECT_NEAR(centre.y(), 0.f, 1e-5f);
	EXPECT_GT(centre.z(), 0.f);
	EXPECT_LT(centre.z(), 1.f);
	// A point on the rim of the cone lands on the border of the square map, along one of its axes.
	const auto edge = project(camera, {3.f * std::tan(outer), 0.f, 0.f});
	EXPECT_NEAR(std::max(std::abs(edge.x()), std::abs(edge.y())), 1.f, 1e-4f);
	EXPECT_NEAR(std::min(std::abs(edge.x()), std::abs(edge.y())), 0.f, 1e-4f);
	const auto vertical = computeSpotShadowCamera(light, {0.f, 1.f, 0.f}, 0.f, 6.f);
	EXPECT_GT(project(vertical, {0.f, 5.f, 0.f}).z(), 0.f);
	gpu::RenderCommand::invalidate();
	core::Log::invalidate();
}

TEST(LocalShadows, RankKeepsTheNearestThenTheStrongest) {
	std::vector<ShadowCandidate> candidates{{.lightIndex = 0, .distance = 5.f, .intensity = 100.f},
											{.lightIndex = 1, .distance = 0.f, .intensity = 10.f},
											{.lightIndex = 2, .distance = 0.f, .intensity = 50.f},
											{.lightIndex = 3, .distance = 2.f, .intensity = 1.f}};
	rankShadowCandidates(candidates, 3);
	ASSERT_EQ(candidates.size(), 3u);
	EXPECT_EQ(candidates[0].lightIndex, 2u);
	EXPECT_EQ(candidates[1].lightIndex, 1u);
	EXPECT_EQ(candidates[2].lightIndex, 3u);
	rankShadowCandidates(candidates, 8);
	EXPECT_EQ(candidates.size(), 3u);
	rankShadowCandidates(candidates, 0);
	EXPECT_TRUE(candidates.empty());
}

TEST(LocalShadows, DistanceToLightRangeIsZeroInside) {
	EXPECT_FLOAT_EQ(distanceToLightRange({0.f, 0.f, 0.f}, {3.f, 0.f, 0.f}, 5.f), 0.f);
	EXPECT_FLOAT_EQ(distanceToLightRange({0.f, 0.f, 0.f}, {0.f, 8.f, 0.f}, 5.f), 3.f);
	EXPECT_FLOAT_EQ(distanceToLightRange({0.f, 0.f, 0.f}, {0.f, 0.f, 2.f}, -1.f), 2.f);
}

TEST(LocalShadows, Renderer3DGivesEachShadowedLightItsEntry) {
	core::Log::init(core::Log::Level::Off);
	gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	gpu::RenderCommand::init();
	Renderer3D::init();
	const std::array<GpuLight, 3> lights{};
	Renderer3D::setSceneLights(lights);
	LocalShadowSettings shadows;
	shadows.spots.emplace_back().lightIndex = 2;
	shadows.points.emplace_back().lightIndex = 0;
	Renderer3D::setLocalShadows(shadows);
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(2), 0);
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(0), 1);
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(1), -1);
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(Renderer3D::g_maxLights), -1);

	LocalShadowSettings many;
	for (uint32_t index = 0; index < g_vulkanLocalShadowBudget.spot + 4; ++index)
		many.spots.emplace_back().lightIndex = index;
	Renderer3D::setLocalShadows(many);
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(g_vulkanLocalShadowBudget.spot - 1),
			  static_cast<int32_t>(g_vulkanLocalShadowBudget.spot - 1));
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(g_vulkanLocalShadowBudget.spot), -1);

	Renderer3D::setLocalShadows({});
	EXPECT_EQ(Renderer3D::getLocalShadowEntry(2), -1);
	Renderer3D::shutdown();
	gpu::RenderCommand::invalidate();
	core::Log::invalidate();
}
