/**
 * @file RendererMesh.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererMesh.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace owl::renderer {

namespace {

struct QueuedMesh {
	Renderer3D::MeshHandle mesh;
	math::mat4 model;
	shared<gpu::Texture2D> texture;
	MeshDrawParams params;
	float distanceSq = 0.f;
};

struct FrameData {
	math::vec3 cameraPosition{0.f, 0.f, 0.f};
	std::vector<QueuedMesh> transparent;
	RendererMesh::Statistics stats;
};

auto frameData() -> FrameData& {
	static FrameData data;
	return data;
}

void draw(const Renderer3D::MeshHandle& iMesh, const math::mat4& iModel, const shared<gpu::Texture2D>& iTexture,
		  const MeshDrawParams& iParams) {
	const std::array<shared<gpu::Texture2D>, 1> textures{iTexture ? iTexture : Renderer3D::getWhiteTexture()};
	Renderer3D::drawMesh(iMesh, iModel, textures, iParams);
}

}// namespace

void RendererMesh::beginScene(const Camera& iCamera, const MeshLayerConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	Renderer3D::beginScene(iCamera);
	Renderer3D::setLighting(iConfig.sunDirection, iConfig.ambient);
	const math::vec4 worldPos = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	auto& data = frameData();
	data.cameraPosition = math::vec3{worldPos.x(), worldPos.y(), worldPos.z()};
	data.transparent.clear();
	data.stats = {};
}

void RendererMesh::endScene() {
	OWL_PROFILE_FUNCTION()

	auto& data = frameData();
	std::ranges::sort(data.transparent,
					  [](const QueuedMesh& iA, const QueuedMesh& iB) -> bool { return iA.distanceSq > iB.distanceSq; });
	for (const auto& queued: data.transparent) draw(queued.mesh, queued.model, queued.texture, queued.params);
	data.stats.transparentDrawCount = static_cast<uint32_t>(data.transparent.size());
	data.transparent.clear();
	Renderer3D::endScene();
}

void RendererMesh::drawMesh(MeshResource& ioMesh, const math::mat4& iModel, const shared<gpu::Texture2D>& iTexture,
							const MeshDrawParams& iParams, const bool iTransparent) {
	if (iTransparent && Renderer3D::isShadowPass())
		return;
	const auto& mesh = ioMesh.getMesh(iTransparent);
	if (!mesh)
		return;
	auto& data = frameData();
	if (!iTransparent) {
		draw(mesh, iModel, iTexture, iParams);
		++data.stats.opaqueDrawCount;
		return;
	}
	const auto& geometry = ioMesh.getGeometry();
	const math::vec3 centre = (geometry.boundsMin + geometry.boundsMax) * 0.5f;
	const math::vec4 world = iModel * math::vec4{centre.x(), centre.y(), centre.z(), 1.f};
	const math::vec3 delta = math::vec3{world.x(), world.y(), world.z()} - data.cameraPosition;
	data.transparent.push_back(QueuedMesh{.mesh = mesh,
										  .model = iModel,
										  .texture = iTexture,
										  .params = iParams,
										  .distanceSq = delta.normSq()});
}

auto RendererMesh::getStatistics() -> Statistics { return frameData().stats; }

}// namespace owl::renderer
