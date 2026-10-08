/**
 * @file PipelineState_test.cpp
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "testHelper.h"

#include <renderer/Renderer3D.h>
#include <renderer/gpu/PipelineState.h>
#include <renderer/gpu/RenderCommand.h>
#include <renderer/gpu/null/DrawData.h>
#include <renderer/gpu/null/RenderAPI.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace owl;
using namespace owl::renderer;
using namespace owl::renderer::gpu;

TEST(PipelineState, defaultIsPainterOrdered2D) {
	constexpr PipelineState state;
	EXPECT_EQ(state.topology, PrimitiveTopology::Triangles);
	EXPECT_EQ(state.cullMode, CullMode::None);
	EXPECT_EQ(state.blendMode, BlendMode::Alpha);
	EXPECT_FALSE(state.depthTest);
	EXPECT_FALSE(state.depthWrite);
}

TEST(PipelineState, comparesEveryField) {
	constexpr PipelineState base;
	EXPECT_EQ(base, PipelineState{});
	EXPECT_NE(base, PipelineState{.topology = PrimitiveTopology::Lines});
	EXPECT_NE(base, PipelineState{.cullMode = CullMode::Back});
	EXPECT_NE(base, PipelineState{.blendMode = BlendMode::Opaque});
	EXPECT_NE(base, PipelineState{.depthTest = true});
	EXPECT_NE(base, PipelineState{.depthWrite = true});
}

TEST(PipelineState, meshStatesTestDepthAndOnlyOpaqueWritesIt) {
	EXPECT_TRUE(Renderer3D::opaqueMeshState.depthTest);
	EXPECT_TRUE(Renderer3D::opaqueMeshState.depthWrite);
	EXPECT_TRUE(Renderer3D::transparentMeshState.depthTest);
	EXPECT_FALSE(Renderer3D::transparentMeshState.depthWrite);
}

TEST(PipelineState, nullBackendRecordsTheStateOfEachDraw) {
	core::Log::init(core::Log::Level::Off);
	null::RenderAPI api;
	api.init();
	std::vector<uint32_t> indices{0, 1};
	const auto lines = mkShared<null::DrawData>();
	lines->init({{"i_EndpointIndex", ShaderDataType::Int}}, "renderer2D", indices, "line",
				{.topology = PrimitiveTopology::Lines});
	const auto mesh = mkShared<null::DrawData>();
	mesh->init({{"i_Position", ShaderDataType::Float3}}, "renderer3D", indices, "mesh3d",
			   Renderer3D::transparentMeshState);

	api.beginFrame();
	api.drawDataInstanced(lines, 2, 4);
	api.drawData(mesh, 0);
	ASSERT_EQ(api.getDrawnStates().size(), 2u);
	EXPECT_EQ(api.getDrawnStates()[0].topology, PrimitiveTopology::Lines);
	EXPECT_EQ(api.getDrawnStates()[1], Renderer3D::transparentMeshState);

	const std::array<shared<Texture2D>, 3> textures{};
	api.bindTextures(textures);
	EXPECT_EQ(api.getBoundTextureCount(), 3u);

	api.beginFrame();
	EXPECT_TRUE(api.getDrawnStates().empty());
	core::Log::invalidate();
}

TEST(PipelineState, renderer3DMeshesKeepTheirState) {
	core::Log::init(core::Log::Level::Off);
	RenderCommand::create(RenderAPI::Type::Null);
	RenderCommand::init();
	Renderer3D::init();
	const std::array<Mesh3DVertex, 3> vertices{};
	const std::array<uint32_t, 3> indices{0, 1, 2};
	const auto opaque = Renderer3D::createMesh(vertices, indices);
	const auto water = Renderer3D::createMesh(vertices, indices, "voxel", Renderer3D::transparentMeshState);
	ASSERT_TRUE(opaque);
	ASSERT_TRUE(water);
	EXPECT_EQ(opaque->getPipelineState(), Renderer3D::opaqueMeshState);
	EXPECT_EQ(water->getPipelineState(), Renderer3D::transparentMeshState);
	Renderer3D::shutdown();
	RenderCommand::invalidate();
	core::Log::invalidate();
}
