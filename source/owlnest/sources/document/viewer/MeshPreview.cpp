/**
 * @file MeshPreview.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MeshPreview.h"

#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <renderer/Renderer3D.h>
#include <renderer/RendererMesh.h>
#include <renderer/gpu/RenderCommand.h>

#include <algorithm>
#include <cmath>

namespace owl::nest::viewer {

namespace {
constexpr float g_fov = 0.6981317f;
constexpr math::vec4 g_background{0.07f, 0.075f, 0.09f, 1.f};
constexpr math::vec4 g_shadedTint{0.78f, 0.78f, 0.8f, 1.f};

auto boundingRadius(const renderer::MeshGeometry& iGeometry) -> float {
	return std::max((iGeometry.boundsMax - iGeometry.boundsMin).norm() * 0.5f, 0.01f);
}

auto orbitDistance(const renderer::MeshGeometry& iGeometry, const MeshOrbit& iOrbit) -> float {
	return boundingRadius(iGeometry) / std::sin(g_fov * 0.5f) * std::max(iOrbit.zoom, 0.05f);
}

auto orbitTransform(const renderer::MeshGeometry& iGeometry, const MeshOrbit& iOrbit) -> math::mat4 {
	const math::vec3 centre = (iGeometry.boundsMin + iGeometry.boundsMax) * 0.5f;
	const float distance = orbitDistance(iGeometry, iOrbit);
	auto transform = math::translate(math::identity<float, 4>(), centre);
	transform = math::rotate(transform, iOrbit.yaw, math::vec3{0.f, 1.f, 0.f});
	transform = math::rotate(transform, iOrbit.pitch, math::vec3{1.f, 0.f, 0.f});
	return math::translate(transform, math::vec3{0.f, 0.f, distance});
}
}// namespace

auto makeMeshPreviewCamera(const renderer::MeshGeometry& iGeometry, const MeshOrbit& iOrbit, const float iAspect,
						   const bool iVulkanClip) -> renderer::Camera {
	const float radius = boundingRadius(iGeometry);
	const float distance = orbitDistance(iGeometry, iOrbit);
	auto projection = math::perspective(g_fov, std::max(iAspect, 0.01f),
										std::max(distance - radius * 2.f, distance * 0.01f), distance + radius * 2.f);
	if (iVulkanClip) {
		auto bias = math::identity<float, 4>();
		bias(2, 2) = 0.5f;
		bias(2, 3) = 0.5f;
		projection = bias * projection;
		projection(1, 1) *= -1.f;
	}
	renderer::Camera camera{projection};
	camera.setTransform(orbitTransform(iGeometry, iOrbit));
	return camera;
}

void renderMeshPreview(renderer::gpu::Framebuffer& ioTarget, renderer::MeshResource& ioMesh, const MeshOrbit& iOrbit,
					   const MeshShading iShading) {
	const auto& size = ioTarget.getSpecification().size;
	const float aspect = size.y() > 0 ? static_cast<float>(size.x()) / static_cast<float>(size.y()) : 1.f;
	const bool vulkan = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan;
	const auto camera = makeMeshPreviewCamera(ioMesh.getGeometry(), iOrbit, aspect, vulkan);
	const math::mat4 world = orbitTransform(ioMesh.getGeometry(), iOrbit);
	const math::vec4 back = world * math::vec4{0.f, 0.f, -1.f, 0.f};
	// The sun comes from above and behind the viewer's left shoulder, so the faces in view are lit.
	const math::vec3 sun = math::vec3{back.x() + 0.35f, back.y() - 0.8f, back.z()}.normalized();
	ioTarget.bind();
	renderer::gpu::RenderCommand::setClearColor(g_background);
	renderer::gpu::RenderCommand::clear();
	ioTarget.clearAttachment(1, -1);
	renderer::RendererMesh::beginScene(camera, {.sunDirection = sun, .ambient = {0.32f, 0.32f, 0.36f}});
	const bool textured = iShading == MeshShading::Textured;
	renderer::RendererMesh::drawMesh(ioMesh, math::identity<float, 4>(),
									 textured ? ioMesh.getBaseColorTexture() : shared<renderer::gpu::Texture2D>{},
									 {.tint = textured ? ioMesh.getBaseColorFactor() : g_shadedTint, .entityId = -1},
									 textured && ioMesh.getBaseColorFactor().a() < 1.f);
	renderer::RendererMesh::endScene();
	ioTarget.unbind();
}

}// namespace owl::nest::viewer
