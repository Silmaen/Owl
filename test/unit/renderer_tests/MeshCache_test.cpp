/**
 * @file MeshCache_test.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <data/geometry/MeshLoader.h>
#include <renderer/CameraOrtho.h>
#include <renderer/MeshCache.h>
#include <renderer/RenderLayerFactory.h>
#include <renderer/Renderer3D.h>
#include <renderer/RendererMesh.h>
#include <renderer/RendererMeshLayer.h>
#include <renderer/gpu/RenderCommand.h>

#include <filesystem>

using namespace owl;
using namespace owl::renderer;

namespace {

auto cubePath() -> std::filesystem::path {
	return test::getTestAssetsPath("render_tests") / "meshes" / "textured_cube.glb";
}

auto meshFixture(const char* iName) -> std::filesystem::path { return test::getTestAssetsPath("mesh_tests") / iName; }

// Null backend with Renderer3D up, as the scenes of a headless application draw.
class MeshCacheTest : public testing::Test {
protected:
	void SetUp() override {
		core::Log::init(core::Log::Level::Off);
		gpu::RenderCommand::create(gpu::RenderAPI::Type::Null);
		gpu::RenderCommand::init();
		Renderer3D::init();
	}

	void TearDown() override {
		m_cache.clear();
		Renderer3D::shutdown();
		gpu::RenderCommand::invalidate();
		core::Log::invalidate();
	}

	/// Cache under test.
	MeshCache m_cache;
};

}// namespace

TEST_F(MeshCacheTest, geometrySharesTheIdenticalCorners) {
	const auto loaded = data::geometry::MeshLoader::loadMesh(cubePath());
	ASSERT_NE(loaded.mesh, nullptr);
	const auto geometry = MeshGeometry::fromStaticMesh(*loaded.mesh);
	// Four corners per face: the faces do not share a corner (their normals differ).
	EXPECT_EQ(geometry.vertices.size(), 24u);
	EXPECT_EQ(geometry.indices.size(), 36u);
	EXPECT_EQ(geometry.boundsMin, (math::vec3{-0.5f, -0.5f, -0.5f}));
	EXPECT_EQ(geometry.boundsMax, (math::vec3{0.5f, 0.5f, 0.5f}));
	for (const auto& vertex: geometry.vertices) EXPECT_EQ(vertex.textureIndex, 1u);
	EXPECT_EQ(geometry.vertices.front().normal, (math::vec3{0.f, 0.f, 1.f}));
}

TEST_F(MeshCacheTest, geometryWithoutNormalsTakesTheFaceNormal) {
	const auto mesh = data::geometry::MeshLoader::loadStaticMesh(meshFixture("multiple_triangle.obj"));
	ASSERT_NE(mesh, nullptr);
	const auto geometry = MeshGeometry::fromStaticMesh(*mesh);
	// Two coplanar triangles over four positions, no UV: one vertex per position.
	EXPECT_EQ(geometry.vertices.size(), 4u);
	EXPECT_EQ(geometry.indices.size(), 6u);
	for (const auto& vertex: geometry.vertices) {
		EXPECT_EQ(vertex.normal, (math::vec3{0.f, 0.f, 1.f}));
		EXPECT_EQ(vertex.uv, (math::vec2{0.f, 0.f}));
	}
	EXPECT_TRUE(MeshGeometry::fromStaticMesh(data::geometry::StaticMesh{}).isEmpty());
}

TEST_F(MeshCacheTest, raycastFindsTheNearestFace) {
	const auto loaded = data::geometry::MeshLoader::loadMesh(cubePath());
	const auto geometry = MeshGeometry::fromStaticMesh(*loaded.mesh);
	const auto front = geometry.raycast({0.1f, 0.2f, 5.f}, {0.f, 0.f, -2.f});
	ASSERT_TRUE(front.has_value());
	EXPECT_NEAR(front->x(), 0.1f, 1e-5f);
	EXPECT_NEAR(front->y(), 0.2f, 1e-5f);
	EXPECT_NEAR(front->z(), 0.5f, 1e-5f);
	const auto inside = geometry.raycast({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f});
	ASSERT_TRUE(inside.has_value());
	EXPECT_NEAR(inside->x(), 0.5f, 1e-5f);
	EXPECT_FALSE(geometry.raycast({2.f, 2.f, 5.f}, {0.f, 0.f, -1.f}).has_value());
	EXPECT_FALSE(geometry.raycast({0.f, 0.f, 5.f}, {0.f, 0.f, 1.f}).has_value());
}

TEST_F(MeshCacheTest, entitiesOfOneFileShareOneResource) {
	const auto first = m_cache.get(cubePath());
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(m_cache.get(cubePath()), first);
	EXPECT_EQ(m_cache.find(cubePath()), first);
	EXPECT_EQ(m_cache.size(), 1u);
	EXPECT_EQ(first->getFile(), cubePath());
	EXPECT_EQ(first->getGeometry().indices.size(), 36u);
	ASSERT_NE(first->getBaseColorTexture(), nullptr);
	EXPECT_EQ(first->getBaseColorTexture()->getSize(), (math::vec2ui{8, 8}));
	EXPECT_EQ(first->getBaseColorFactor(), (math::vec4{1.f, 1.f, 1.f, 1.f}));
}

TEST_F(MeshCacheTest, missingFilesAreRememberedUntilInvalidated) {
	EXPECT_EQ(m_cache.get("meshes/missing.glb"), nullptr);
	EXPECT_EQ(m_cache.get("textures/crate.png"), nullptr);
	EXPECT_EQ(m_cache.get({}), nullptr);
	EXPECT_EQ(m_cache.size(), 2u);
	EXPECT_EQ(m_cache.find("meshes/missing.glb"), nullptr);
	EXPECT_EQ(m_cache.invalidate("/project/assets/meshes/missing.glb"), 1u);
	EXPECT_EQ(m_cache.size(), 1u);
}

TEST_F(MeshCacheTest, invalidatingAFileMakesItsHoldersReload) {
	const auto first = m_cache.get(cubePath());
	ASSERT_NE(first, nullptr);
	EXPECT_FALSE(first->isStale());
	EXPECT_EQ(m_cache.invalidate(meshFixture("multiple_triangle.glb")), 0u);
	EXPECT_EQ(m_cache.invalidate(cubePath()), 1u);
	EXPECT_TRUE(first->isStale());
	EXPECT_EQ(m_cache.find(cubePath()), nullptr);
	const auto second = m_cache.get(cubePath());
	ASSERT_NE(second, nullptr);
	EXPECT_NE(second, first);
	EXPECT_FALSE(second->isStale());
	m_cache.clear();
	EXPECT_TRUE(second->isStale());
	EXPECT_EQ(m_cache.size(), 0u);
}

TEST_F(MeshCacheTest, gpuMeshesAreUploadedOncePerPipelineState) {
	const auto resource = m_cache.get(cubePath());
	ASSERT_NE(resource, nullptr);
	const auto opaque = resource->getMesh(false);
	ASSERT_NE(opaque, nullptr);
	EXPECT_EQ(resource->getMesh(false), opaque);
	EXPECT_EQ(opaque->getPipelineState(), Renderer3D::opaqueMeshState);
	const auto blended = resource->getMesh(true);
	ASSERT_NE(blended, nullptr);
	EXPECT_NE(blended, opaque);
	EXPECT_EQ(blended->getPipelineState(), Renderer3D::transparentMeshState);
}

TEST_F(MeshCacheTest, rendererMeshDrawsOpaqueAtOnceAndBlendedAtTheEnd) {
	const auto resource = m_cache.get(cubePath());
	ASSERT_NE(resource, nullptr);
	const CameraOrtho camera{-1.f, 1.f, -1.f, 1.f};
	RendererMesh::beginScene(camera, {});
	const auto model = math::identity<float, 4>();
	RendererMesh::drawMesh(*resource, model, nullptr, {.entityId = 3}, false);
	RendererMesh::drawMesh(*resource, model, resource->getBaseColorTexture(), {.entityId = 4}, true);
	RendererMesh::drawMesh(*resource, model, nullptr, {.tint = {1.f, 1.f, 1.f, 0.5f}, .entityId = 5}, true);
	EXPECT_EQ(RendererMesh::getStatistics().opaqueDrawCount, 1u);
	EXPECT_EQ(RendererMesh::getStatistics().transparentDrawCount, 0u);
	RendererMesh::endScene();
	EXPECT_EQ(RendererMesh::getStatistics().opaqueDrawCount, 1u);
	EXPECT_EQ(RendererMesh::getStatistics().transparentDrawCount, 2u);
	EXPECT_NE(Renderer3D::getWhiteTexture(), nullptr);
}

TEST(RendererMeshLayer, registersCreatesAndReadsItsLighting) {
	core::Log::init(core::Log::Level::Off);
	RendererMeshLayer::registerWithFactory();
	EXPECT_TRUE(RenderLayerFactory::hasType("RendererMesh"));
	const auto layer = RenderLayerFactory::create("RendererMesh", "mesh_world");
	ASSERT_NE(layer, nullptr);
	EXPECT_STREQ(layer->getTypeKey(), "RendererMesh");
	EXPECT_EQ(layer->getName(), "mesh_world");
	EXPECT_FALSE(layer->drawsOwnEntities());
	auto* meshLayer = dynamic_cast<RendererMeshLayer*>(layer.get());
	ASSERT_NE(meshLayer, nullptr);
	meshLayer->applyConfig("SunDirection: [0, -1, 0]\nAmbient: [0.5, 0.5, 0.5]");
	EXPECT_EQ(meshLayer->getConfig().sunDirection, (math::vec3{0.f, -1.f, 0.f}));
	EXPECT_EQ(meshLayer->getConfig().ambient, (math::vec3{0.5f, 0.5f, 0.5f}));
	meshLayer->applyConfig("not: [a map");
	EXPECT_EQ(meshLayer->getConfig().ambient, (math::vec3{0.5f, 0.5f, 0.5f}));
	core::Log::invalidate();
}
