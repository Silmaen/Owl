/**
 * @file Renderer2DLayer_test.cpp
 * @author Silmaen
 * @date 18/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <math/matrices.h>
#include <math/vectors.h>
#include <renderer/CameraOrtho.h>
#include <renderer/Renderer2DLayer.h>
#include <renderer/utils/Billboards.h>

#include <cmath>
#include <cstddef>

using namespace owl;

namespace {

auto viewProjectionAt(const char* iConfig, const math::vec2ui& iViewport) -> math::mat4 {
	renderer::Renderer2DLayer layer{"overlay"};
	layer.applyConfig(iConfig);
	layer.setViewport(iViewport);
	const renderer::CameraOrtho camera{-1.f, 1.f, -1.f, 1.f};
	return layer.getEffectiveViewProjection(camera);
}

void expectNear(const math::mat4& iActual, const math::mat4& iExpected) {
	for (std::size_t row = 0; row < 4; ++row) {
		for (std::size_t col = 0; col < 4; ++col) { EXPECT_NEAR(iActual(row, col), iExpected(row, col), 1e-6f); }
	}
}

}// namespace

TEST(Renderer2DLayer, ScreenSpaceDefaultsToPixels) {
	renderer::Renderer2DLayer layer{"overlay"};
	layer.applyConfig("{Space: Screen}");
	EXPECT_EQ(layer.getSpace(), renderer::Renderer2DLayer::Space::Screen);
	EXPECT_FLOAT_EQ(layer.getReferenceHeight(), 0.f);
	const auto small = viewProjectionAt("{Space: Screen}", {1280, 720});
	const auto large = viewProjectionAt("{Space: Screen}", {2560, 1440});
	EXPECT_NEAR(large(0, 0) * 2.f, small(0, 0), 1e-6f);
	EXPECT_NEAR(large(1, 1) * 2.f, small(1, 1), 1e-6f);
}

TEST(Renderer2DLayer, ReferenceHeightScalesWithTheScreen) {
	const auto pixels720 = viewProjectionAt("{Space: Screen}", {1280, 720});
	const char* config = "{Space: Screen, ReferenceHeight: 720}";
	expectNear(viewProjectionAt(config, {1280, 720}), pixels720);
	expectNear(viewProjectionAt(config, {1920, 1080}), pixels720);
	expectNear(viewProjectionAt(config, {2560, 1440}), pixels720);
	// A wider screen keeps the height: the units grow to the right.
	const auto wide = viewProjectionAt(config, {3440, 1440});
	EXPECT_NEAR(wide(1, 1), pixels720(1, 1), 1e-6f);
	EXPECT_LT(wide(0, 0), pixels720(0, 0));
	renderer::Renderer2DLayer negative{"overlay"};
	negative.applyConfig("{Space: Screen, ReferenceHeight: -5}");
	EXPECT_FLOAT_EQ(negative.getReferenceHeight(), 0.f);
}

TEST(Renderer2DLayer, BillboardAndDepthTestAreReadFromTheConfig) {
	renderer::Renderer2DLayer layer{"sprites"};
	EXPECT_EQ(layer.getBillboard(), renderer::Renderer2DLayer::Billboard::None);
	EXPECT_FALSE(layer.isDepthTested());
	layer.applyConfig("{Billboard: upright, DepthTest: true}");
	EXPECT_EQ(layer.getBillboard(), renderer::Renderer2DLayer::Billboard::Upright);
	EXPECT_TRUE(layer.isDepthTested());
	layer.applyConfig("{Billboard: Full}");
	EXPECT_EQ(layer.getBillboard(), renderer::Renderer2DLayer::Billboard::Full);
	layer.applyConfig("{Billboard: sideways, DepthTest: false}");
	EXPECT_EQ(layer.getBillboard(), renderer::Renderer2DLayer::Billboard::None);
	EXPECT_FALSE(layer.isDepthTested());
}

TEST(Billboards, AnUprightSpriteTurnsAroundTheWorldUpAxisOnly) {
	// A camera turned by 90 degrees around Y and pitched down by 45 degrees.
	const math::mat4 cameraWorld =
			math::Transform{math::vec3{4.f, 3.f, 0.f}, math::vec3{-0.7853982f, 1.5707964f, 0.f}}();
	const auto upright =
			renderer::utils::makeBillboardFrame(renderer::Renderer2DLayer::Billboard::Upright, cameraWorld);
	EXPECT_NEAR(upright.up.y(), 1.f, 1e-5f);
	EXPECT_NEAR(upright.right.y(), 0.f, 1e-5f);
	EXPECT_NEAR(upright.right.z(), -1.f, 1e-5f);
	EXPECT_NEAR(upright.normal.x(), 1.f, 1e-5f);
	const auto full = renderer::utils::makeBillboardFrame(renderer::Renderer2DLayer::Billboard::Full, cameraWorld);
	EXPECT_NEAR(full.normal * full.forward, -1.f, 1e-5f);
	EXPECT_NEAR(full.up.y(), std::sqrt(0.5f), 1e-5f);
	// Looking straight down, the upright frame still has a heading.
	const math::mat4 topDown = math::Transform{math::vec3{0.f, 9.f, 0.f}, math::vec3{-1.5707964f, 0.f, 0.f}}();
	const auto above = renderer::utils::makeBillboardFrame(renderer::Renderer2DLayer::Billboard::Upright, topDown);
	EXPECT_NEAR(above.right.norm(), 1.f, 1e-5f);
}

TEST(Billboards, TheMatrixKeepsSizeAndRollAndTheKeySortsByDistance) {
	const renderer::utils::BillboardFrame frame;
	const auto matrix = renderer::utils::billboardMatrix(frame, {1.f, 2.f, 3.f}, {2.f, 0.5f, 1.f}, 1.5707964f);
	EXPECT_NEAR(matrix(0, 3), 1.f, 1e-6f);
	EXPECT_NEAR(matrix(2, 3), 3.f, 1e-6f);
	// A quarter turn: the sprite X axis points up, two units long; its Y axis points left, half a unit.
	EXPECT_NEAR(matrix(1, 0), 2.f, 1e-5f);
	EXPECT_NEAR(matrix(0, 1), -0.5f, 1e-5f);
	EXPECT_GT(renderer::utils::viewSortKey(frame, {0.f, 0.f, -1.f}),
			  renderer::utils::viewSortKey(frame, {0.f, 0.f, -5.f}));
}
