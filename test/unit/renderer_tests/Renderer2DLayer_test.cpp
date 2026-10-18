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
