/**
 * @file ShadowCascades_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <renderer/ClipSpace.h>
#include <renderer/ShadowCascades.h>
#include <renderer/gpu/RenderCommand.h>

#include <cmath>
#include <cstdint>

using namespace owl;
using namespace owl::renderer;

namespace {
class ShadowCascadesTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
	}

	void TearDown() override {
		gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}
};

auto viewCamera() -> Camera {
	Camera camera{math::perspectiveReversedZ(0.8f, 16.f / 9.f, 0.1f, 100.f)};
	camera.setTransform(math::translate(math::identity<float, 4>(), math::vec3{0.f, 2.f, 0.f}));
	return camera;
}

auto project(const Camera& iCamera, const math::vec3& iPoint) -> math::vec3 {
	const math::vec4 clip = iCamera.getViewProjection() * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	return {clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w()};
}

}// namespace

TEST_F(ShadowCascadesTest, splitsTheShadowDistanceNearestFirst) {
	const auto cascades = computeShadowCascades(
			viewCamera(), {.lightDirection = {0.3f, -1.f, -0.2f}, .shadowDistance = 40.f, .cascadeCount = 4});
	ASSERT_EQ(cascades.count, 4u);
	float previous = 0.f;
	for (uint32_t index = 0; index < cascades.count; ++index) {
		EXPECT_GT(cascades.cascades[index].splitDepth, previous);
		EXPECT_GT(cascades.cascades[index].texelWorldSize, 0.f);
		previous = cascades.cascades[index].splitDepth;
	}
	EXPECT_NEAR(previous, 40.f, 1e-3f);
}

TEST_F(ShadowCascadesTest, aVisiblePointFallsInsideItsCascade) {
	const auto cascades = computeShadowCascades(
			viewCamera(), {.lightDirection = {0.3f, -1.f, -0.2f}, .shadowDistance = 40.f, .cascadeCount = 2});
	ASSERT_EQ(cascades.count, 2u);
	// 5 units in front of the camera, on the ground: inside the first cascade.
	const math::vec3 ndc = project(cascades.cascades[0].camera, {0.5f, 0.f, -5.f});
	EXPECT_LT(std::abs(ndc.x()), 1.f);
	EXPECT_LT(std::abs(ndc.y()), 1.f);
	EXPECT_GT(ndc.z(), renderer::g_farDepth);
	EXPECT_LT(ndc.z(), renderer::g_nearDepth);
	// A caster above it, towards the light, is nearer to the light: a larger reversed-Z depth.
	const math::vec3 caster =
			project(cascades.cascades[0].camera, math::vec3{0.5f, 0.f, -5.f} + math::vec3{-0.3f, 1.f, 0.2f});
	EXPECT_GT(caster.z(), ndc.z());
}

TEST_F(ShadowCascadesTest, theDepthRangeTurnsAStoredDepthIntoADistance) {
	const math::vec3 direction = math::vec3{0.3f, -1.f, -0.2f}.normalized();
	const auto cascades = computeShadowCascades(
			viewCamera(), {.lightDirection = direction, .shadowDistance = 40.f, .cascadeCount = 2});
	ASSERT_EQ(cascades.count, 2u);
	const auto& cascade = cascades.cascades[1];
	ASSERT_GT(cascade.depthRange, 0.f);
	const math::vec3 point{0.5f, 0.f, -12.f};
	const math::vec3 casterAbove = point - direction * 2.f;
	const float receiver = (1.f - project(cascade.camera, point).z()) * cascade.depthRange;
	const float caster = (1.f - project(cascade.camera, casterAbove).z()) * cascade.depthRange;
	EXPECT_NEAR(receiver - caster, 2.f, 1e-3f);
}

TEST_F(ShadowCascadesTest, aNullDirectionGivesNoCascade) {
	EXPECT_EQ(computeShadowCascades(viewCamera(), {.lightDirection = {0.f, 0.f, 0.f}}).count, 0u);
}

TEST_F(ShadowCascadesTest, theCascadesDoNotMoveWithASubTexelCameraShift) {
	Camera moved{math::perspectiveReversedZ(0.8f, 16.f / 9.f, 0.1f, 100.f)};
	moved.setTransform(math::translate(math::identity<float, 4>(), math::vec3{0.0001f, 2.f, 0.f}));
	const ShadowCascadeSettings settings{.lightDirection = {0.f, -1.f, 0.f}, .shadowDistance = 30.f, .cascadeCount = 1};
	const auto reference = computeShadowCascades(viewCamera(), settings);
	const auto shifted = computeShadowCascades(moved, settings);
	const math::vec3 a = project(reference.cascades[0].camera, {1.f, 0.f, -3.f});
	const math::vec3 b = project(shifted.cascades[0].camera, {1.f, 0.f, -3.f});
	EXPECT_NEAR(a.x(), b.x(), 1e-5f);
	EXPECT_NEAR(a.y(), b.y(), 1e-5f);
}
