/**
 * @file RendererMesh.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererMesh.h"

#include "math/Frustum.h"
#include "renderer/Material.h"
#include "renderer/utils/MeshCullingPass.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {

constexpr uint32_t g_instanceBinding = 5;

struct QueuedMesh {
	Renderer3D::MeshHandle mesh;
	math::mat4 model;
	shared<gpu::Texture2D> texture;
	MeshDrawParams params;
	float distanceSq = 0.f;
	// First matrix of the palette of a skinned draw, -1 for a static one.
	int32_t firstMatrix = -1;
};

// An opaque skinned draw, drawn on its own after the instanced ones.
struct SkinnedDraw {
	Renderer3D::MeshHandle mesh;
	math::mat4 model;
	MeshDrawParams params;
	uint32_t firstMatrix = 0;
};

// An opaque draw waiting for the instanced flush of `endScene`.
struct OpaqueDraw {
	Renderer3D::MeshHandle mesh;
	const Material* material = nullptr;
	shared<gpu::Texture2D> texture;
	MeshInstance instance;
	math::box3f box;
};

struct FrameData {
	math::vec3 cameraPosition{0.f, 0.f, 0.f};
	std::vector<QueuedMesh> transparent;
	std::vector<OpaqueDraw> opaque;
	std::vector<uint32_t> order;
	std::vector<MeshInstance> instances;
	std::vector<utils::MeshCullingPass::Item> items;
	std::vector<utils::MeshCullingPass::Group> groups;
	std::vector<utils::MeshCullingPass::DrawCommand> commands;
	// First opaque draw (in `order`) of each group.
	std::vector<uint32_t> groupLeaders;
	shared<gpu::StorageBuffer> instanceBuffer;
	uint32_t instanceCapacity = 0;
	std::vector<math::mat4> skinMatrices;
	std::vector<SkinnedDraw> skinned;
	shared<gpu::StorageBuffer> skinBuffer;
	uint32_t skinCapacity = 0;
	utils::MeshCullingPass culling;
	RendererMesh::Statistics stats;
};

auto frameData() -> FrameData& {
	static FrameData data;
	return data;
}

void draw(const Renderer3D::MeshHandle& iMesh, const math::mat4& iModel, const shared<gpu::Texture2D>& iTexture,
		  const MeshDrawParams& iParams) {
	if (iParams.material != nullptr) {
		Renderer3D::drawMesh(iMesh, iModel, {}, iParams);
		return;
	}
	const std::array<shared<gpu::Texture2D>, 1> textures{iTexture ? iTexture : Renderer3D::getWhiteTexture()};
	Renderer3D::drawMesh(iMesh, iModel, textures, iParams);
}

auto sameGroup(const OpaqueDraw& iA, const OpaqueDraw& iB) -> bool {
	return iA.mesh == iB.mesh && iA.material == iB.material && iA.texture == iB.texture;
}

auto worldBox(const MeshResource& iMesh, const math::mat4& iModel) -> math::box3f {
	const auto& geometry = iMesh.getGeometry();
	return math::transformBox(iModel, math::box3f{geometry.boundsMin, geometry.boundsMax});
}

void queueOpaque(const Renderer3D::MeshHandle& iMesh, const MeshResource& iResource, const math::mat4& iModel,
				 const shared<gpu::Texture2D>& iTexture, const MeshDrawParams& iParams) {
	auto& data = frameData();
	data.opaque.push_back(
			OpaqueDraw{.mesh = iMesh,
					   .material = iParams.material,
					   .texture = iParams.material == nullptr ? iTexture : nullptr,
					   .instance = {.model = iModel, .tint = iParams.tint, .ids = {iParams.entityId, 0, 0, 0}},
					   .box = worldBox(iResource, iModel)});
}

// Groups the queued opaque draws by mesh, material and texture, culls them and draws one instanced call per group.
void flushOpaque() {
	OWL_PROFILE_FUNCTION()

	auto& data = frameData();
	if (data.opaque.empty())
		return;
	const auto count = static_cast<uint32_t>(data.opaque.size());
	data.order.resize(count);
	std::iota(data.order.begin(), data.order.end(), 0u);
	std::ranges::sort(data.order, [&data](const uint32_t iA, const uint32_t iB) -> bool {
		const auto& a = data.opaque[iA];
		const auto& b = data.opaque[iB];
		if (a.mesh != b.mesh)
			return a.mesh.get() < b.mesh.get();
		if (a.material != b.material)
			return a.material < b.material;
		return a.texture.get() < b.texture.get();
	});
	data.instances.clear();
	data.items.clear();
	data.groups.clear();
	data.commands.clear();
	data.groupLeaders.clear();
	for (uint32_t rank = 0; rank < count; ++rank) {
		const auto& draw = data.opaque[data.order[rank]];
		if (data.groupLeaders.empty() || !sameGroup(draw, data.opaque[data.groupLeaders.back()])) {
			data.groupLeaders.push_back(data.order[rank]);
			data.groups.push_back({.visibleBase = rank, .pad = {}});
			data.commands.push_back({.indexCount = draw.mesh->getIndexCount(),
									 .instanceCount = 0,
									 .firstIndex = 0,
									 .baseVertex = 0,
									 .baseInstance = 0});
		}
		data.instances.push_back(draw.instance);
		const auto& mn = draw.box.min();
		const auto& mx = draw.box.max();
		data.items.push_back({.boxMin = math::vec4{mn.x(), mn.y(), mn.z(), 0.f},
							  .boxMax = math::vec4{mx.x(), mx.y(), mx.z(), 0.f},
							  .group = static_cast<uint32_t>(data.groups.size() - 1),
							  .pad = {}});
	}
	if (count > data.instanceCapacity || !data.instanceBuffer) {
		data.instanceCapacity = std::max(count, data.instanceCapacity * 2);
		data.instanceBuffer = gpu::StorageBuffer::create(
				data.instanceCapacity * static_cast<uint32_t>(sizeof(MeshInstance)), g_instanceBinding, "Renderer3D");
	}
	if (!data.instanceBuffer)
		return;
	data.instanceBuffer->setData(data.instances.data(), count * static_cast<uint32_t>(sizeof(MeshInstance)), 0);
	const math::mat4 viewProjection = Renderer3D::getViewProjection();
	const bool occlusion = !Renderer3D::isShadowPass() && data.culling.hasPyramid(viewProjection);
	data.culling.cull(data.items, data.groups, data.commands, math::Frustum::fromViewProjection(viewProjection),
					  occlusion);
	if (occlusion)
		data.stats.occlusionLevelCount = data.culling.getPyramidLevelCount();
	if (!data.culling.getVisibleBuffer() || !data.culling.getCommandBuffer())
		return;
	const auto cpuCounts = data.culling.getCpuCounts();
	for (size_t group = 0; group < data.groups.size(); ++group) {
		const auto& leader = data.opaque[data.groupLeaders[group]];
		const MeshInstanceBuffers buffers{.instances = data.instanceBuffer,
										  .visible = data.culling.getVisibleBuffer(),
										  .visibleBase = data.groups[group].visibleBase};
		const MeshDrawParams params{.material = leader.material};
		const std::array<shared<gpu::Texture2D>, 1> textures{leader.texture ? leader.texture
																			: Renderer3D::getWhiteTexture()};
		const std::span<const shared<gpu::Texture2D>> drawTextures =
				leader.material == nullptr ? std::span<const shared<gpu::Texture2D>>{textures}
										   : std::span<const shared<gpu::Texture2D>>{};
		if (data.culling.isGpu()) {
			Renderer3D::drawMeshIndirect(leader.mesh, buffers, data.culling.getCommandBuffer(),
										 static_cast<uint32_t>(group), drawTextures, params);
		} else if (cpuCounts[group] > 0) {
			Renderer3D::drawMeshInstanced(leader.mesh, buffers, cpuCounts[group], drawTextures, params);
			data.stats.visibleInstanceCount += cpuCounts[group];
		}
	}
	data.stats.instancedBatchCount += static_cast<uint32_t>(data.groups.size());
	data.opaque.clear();
}

// Uploads the palettes of the pass in one buffer; false when there is none.
auto uploadSkin() -> bool {
	auto& data = frameData();
	if (data.skinMatrices.empty())
		return false;
	const auto count = static_cast<uint32_t>(data.skinMatrices.size());
	if (count > data.skinCapacity || !data.skinBuffer) {
		data.skinCapacity = std::max(count, data.skinCapacity * 2);
		data.skinBuffer = gpu::StorageBuffer::create(data.skinCapacity * static_cast<uint32_t>(sizeof(math::mat4)),
													 Renderer3D::g_skinBinding, "Renderer3D");
	}
	if (!data.skinBuffer)
		return false;
	data.skinBuffer->setData(data.skinMatrices.data(), count * static_cast<uint32_t>(sizeof(math::mat4)), 0);
	return true;
}

void flushSkinned() {
	OWL_PROFILE_FUNCTION()

	auto& data = frameData();
	for (const auto& draw: data.skinned)
		Renderer3D::drawSkinnedMesh(draw.mesh, draw.model, data.skinBuffer, draw.firstMatrix, {}, draw.params);
	data.stats.skinnedDrawCount += static_cast<uint32_t>(data.skinned.size());
	data.skinned.clear();
}

auto distanceSqTo(const MeshResource& iMesh, const math::mat4& iModel) -> float {
	const auto& geometry = iMesh.getGeometry();
	const math::vec3 centre = (geometry.boundsMin + geometry.boundsMax) * 0.5f;
	const math::vec4 world = iModel * math::vec4{centre.x(), centre.y(), centre.z(), 1.f};
	const math::vec3 delta = math::vec3{world.x(), world.y(), world.z()} - frameData().cameraPosition;
	return delta.normSq();
}

}// namespace

void RendererMesh::init() { frameData().culling.init(); }

void RendererMesh::shutdown() {
	auto& data = frameData();
	data.culling.shutdown();
	data.instanceBuffer.reset();
	data.instanceCapacity = 0;
	data.skinBuffer.reset();
	data.skinCapacity = 0;
	data.skinMatrices.clear();
	data.skinned.clear();
	data.opaque.clear();
	data.transparent.clear();
}

void RendererMesh::beginScene(const Camera& iCamera, const MeshLayerConfig& iConfig) {
	OWL_PROFILE_FUNCTION()

	Renderer3D::beginScene(iCamera);
	Renderer3D::setLighting(iConfig.sunDirection, iConfig.sunIlluminance, iConfig.ambient * iConfig.ambientLuminance);
	const math::vec4 worldPos = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	auto& data = frameData();
	data.cameraPosition = math::vec3{worldPos.x(), worldPos.y(), worldPos.z()};
	data.transparent.clear();
	data.opaque.clear();
	data.skinned.clear();
	data.skinMatrices.clear();
	data.stats = {};
}

void RendererMesh::endScene() {
	OWL_PROFILE_FUNCTION()

	flushOpaque();
	auto& data = frameData();
	const bool skin = uploadSkin();
	if (skin)
		flushSkinned();
	std::ranges::sort(data.transparent,
					  [](const QueuedMesh& iA, const QueuedMesh& iB) -> bool { return iA.distanceSq > iB.distanceSq; });
	for (const auto& queued: data.transparent) {
		if (queued.firstMatrix < 0)
			draw(queued.mesh, queued.model, queued.texture, queued.params);
		else if (skin)
			Renderer3D::drawSkinnedMesh(queued.mesh, queued.model, data.skinBuffer,
										static_cast<uint32_t>(queued.firstMatrix), {}, queued.params);
	}
	data.skinned.clear();
	data.skinMatrices.clear();
	data.stats.transparentDrawCount = static_cast<uint32_t>(data.transparent.size());
	data.transparent.clear();
	Renderer3D::endScene();
}

void RendererMesh::drawMesh(MeshResource& ioMesh, const math::mat4& iModel, const shared<gpu::Texture2D>& iTexture,
							const MeshDrawParams& iParams, const bool iTransparent) {
	MeshDrawParams params = iParams;
	params.blended = iTransparent;
	if (iTransparent && Renderer3D::isShadowPass())
		return;
	const auto& mesh = ioMesh.getMesh(iTransparent);
	if (!mesh)
		return;
	auto& data = frameData();
	if (!iTransparent) {
		queueOpaque(mesh, ioMesh, iModel, iTexture, params);
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
										  .params = params,
										  .distanceSq = delta.normSq(),
										  .firstMatrix = -1});
}

void RendererMesh::drawSubmesh(MeshResource& ioMesh, const size_t iSubmesh, const math::mat4& iModel,
							   const MeshDrawParams& iParams) {
	drawSubmesh(ioMesh, iSubmesh, 0, iModel, iParams);
}

auto RendererMesh::pushSkin(const std::span<const math::mat4> iMatrices) -> uint32_t {
	auto& matrices = frameData().skinMatrices;
	const auto first = static_cast<uint32_t>(matrices.size());
	matrices.insert(matrices.end(), iMatrices.begin(), iMatrices.end());
	return first;
}

void RendererMesh::drawSkinnedSubmesh(MeshResource& ioMesh, const size_t iSubmesh, const size_t iLod,
									  const math::mat4& iModel, const uint32_t iFirstMatrix,
									  const MeshDrawParams& iParams) {
	const bool blended = iParams.material != nullptr && iParams.material->isBlended();
	if (blended && Renderer3D::isShadowPass())
		return;
	const auto& mesh = ioMesh.getSubmeshMesh(iSubmesh, blended, iLod, true);
	if (!mesh)
		return;
	auto& data = frameData();
	if (iLod > 0)
		++data.stats.lodDrawCount;
	if (!blended) {
		data.skinned.push_back({.mesh = mesh, .model = iModel, .params = iParams, .firstMatrix = iFirstMatrix});
		return;
	}
	data.transparent.push_back(QueuedMesh{.mesh = mesh,
										  .model = iModel,
										  .texture = {},
										  .params = iParams,
										  .distanceSq = distanceSqTo(ioMesh, iModel),
										  .firstMatrix = static_cast<int32_t>(iFirstMatrix)});
}

void RendererMesh::drawSubmesh(MeshResource& ioMesh, const size_t iSubmesh, const size_t iLod, const math::mat4& iModel,
							   const MeshDrawParams& iParams) {
	const bool blended = iParams.material != nullptr && iParams.material->isBlended();
	if (blended && Renderer3D::isShadowPass())
		return;
	const auto& mesh = ioMesh.getSubmeshMesh(iSubmesh, blended, iLod, false);
	if (!mesh)
		return;
	auto& data = frameData();
	if (iLod > 0 && !Renderer3D::isShadowPass())
		++data.stats.lodDrawCount;
	if (!blended) {
		queueOpaque(mesh, ioMesh, iModel, nullptr, iParams);
		if (!Renderer3D::isShadowPass())
			++data.stats.opaqueDrawCount;
		return;
	}
	data.transparent.push_back(QueuedMesh{.mesh = mesh,
										  .model = iModel,
										  .texture = {},
										  .params = iParams,
										  .distanceSq = distanceSqTo(ioMesh, iModel),
										  .firstMatrix = -1});
}

void RendererMesh::buildOcclusion(const shared<gpu::Texture2D>& iDepth, const math::vec2ui& iSize,
								  const math::mat4& iViewProjection) {
	frameData().culling.buildPyramid(iDepth, iSize, iViewProjection);
}

void RendererMesh::clearOcclusion() { frameData().culling.clearPyramid(); }

auto RendererMesh::getStatistics() -> Statistics { return frameData().stats; }

}// namespace owl::renderer
