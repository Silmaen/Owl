/**
 * @file MeshDocument_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "document/MeshDocument.h"
#include "document/viewer/MeshPreview.h"
#include "testHelper.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <math/linAlgebra.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>

using namespace owl;
using namespace owl::nest;

namespace {
auto cubePath() -> std::filesystem::path {
	return test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
}

class MeshDocumentTest : public ::testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		m_document.onAttach(nullptr);
	}

	void TearDown() override {
		m_document.onDetach();
		core::Log::invalidate();
	}

	MeshDocument m_document;
};
}// namespace

TEST_F(MeshDocumentTest, readsTheMeshOfTheFile) {
	ASSERT_TRUE(m_document.loadFromFile(cubePath()));
	EXPECT_EQ(m_document.type(), DocumentType::Mesh);
	EXPECT_EQ(m_document.title(), "textured_cube.glb");
	EXPECT_EQ(m_document.filePath(), cubePath());
	EXPECT_FALSE(m_document.isDirty());
	EXPECT_FALSE(m_document.save());
	ASSERT_NE(m_document.getMesh(), nullptr);
	EXPECT_FALSE(m_document.getMesh()->getGeometry().isEmpty());
	EXPECT_TRUE(m_document.overridesGlobalPanels());
	// No canvas was drawn yet: nothing to render, nothing made.
	m_document.onUpdate(core::Timestep{});
}

TEST_F(MeshDocumentTest, refusesAFileWithoutMesh) {
	EXPECT_FALSE(m_document.loadFromFile(test::getTestAssetsPath("render_tests") / "missing.glb"));
	EXPECT_EQ(m_document.getMesh(), nullptr);
	EXPECT_EQ(m_document.title(), "Untitled");
}

TEST_F(MeshDocumentTest, shadingAndViewReset) {
	EXPECT_EQ(m_document.getShading(), viewer::MeshShading::Textured);
	m_document.setShading(viewer::MeshShading::Shaded);
	EXPECT_EQ(m_document.getShading(), viewer::MeshShading::Shaded);
	m_document.resetView();
	EXPECT_FLOAT_EQ(m_document.getOrbit().zoom, viewer::MeshOrbit{}.zoom);
	EXPECT_FLOAT_EQ(m_document.getOrbit().yaw, viewer::MeshOrbit{}.yaw);
}

TEST(MeshPreview, cameraKeepsTheWholeBoundsInView) {
	renderer::MeshGeometry geometry;
	geometry.boundsMin = {-1.f, 0.f, -0.5f};
	geometry.boundsMax = {3.f, 2.f, 0.5f};
	for (const bool vulkan: {false, true}) {
		for (const float yaw: {0.f, 1.f, 2.5f}) {
			const auto camera = viewer::makeMeshPreviewCamera(geometry, {.yaw = yaw, .pitch = -0.4f, .zoom = 1.f},
															  16.f / 9.f, vulkan);
			const auto& viewProjection = camera.getViewProjection();
			const math::vec4 centre = viewProjection * math::vec4{1.f, 1.f, 0.f, 1.f};
			EXPECT_NEAR(centre.x() / centre.w(), 0.f, 1e-4f);
			EXPECT_NEAR(centre.y() / centre.w(), 0.f, 1e-4f);
			for (int corner = 0; corner < 8; ++corner) {
				const math::vec4 point{(corner & 1) != 0 ? 3.f : -1.f, (corner & 2) != 0 ? 2.f : 0.f,
									   (corner & 4) != 0 ? 0.5f : -0.5f, 1.f};
				const math::vec4 clip = viewProjection * point;
				ASSERT_GT(clip.w(), 0.f);
				EXPECT_LE(std::abs(clip.x() / clip.w()), 1.f);
				EXPECT_LE(std::abs(clip.y() / clip.w()), 1.f);
				const float depth = clip.z() / clip.w();
				EXPECT_GE(depth, vulkan ? 0.f : -1.f);
				EXPECT_LE(depth, 1.f);
			}
		}
	}
}

TEST(MeshPreview, zoomMovesTheCameraAway) {
	renderer::MeshGeometry geometry;
	geometry.boundsMin = {-1.f, -1.f, -1.f};
	geometry.boundsMax = {1.f, 1.f, 1.f};
	const auto near = viewer::makeMeshPreviewCamera(geometry, {.yaw = 0.f, .pitch = 0.f, .zoom = 1.f}, 1.f, false);
	const auto far = viewer::makeMeshPreviewCamera(geometry, {.yaw = 0.f, .pitch = 0.f, .zoom = 2.f}, 1.f, false);
	const math::vec4 corner{1.f, 1.f, 1.f, 1.f};
	const math::vec4 nearClip = near.getViewProjection() * corner;
	const math::vec4 farClip = far.getViewProjection() * corner;
	EXPECT_LT(std::abs(farClip.x() / farClip.w()), std::abs(nearClip.x() / nearClip.w()));
}
