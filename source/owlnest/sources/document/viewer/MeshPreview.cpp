/**
 * @file MeshPreview.cpp
 * @author Silmaen
 * @date 21/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "MeshPreview.h"

#include <data/geometry/MeshLoader.h>
#include <math/linAlgebra.h>
#include <math/matrixCreation.h>
#include <renderer/ClipSpace.h>
#include <renderer/Renderer3D.h>
#include <renderer/RendererMesh.h>
#include <renderer/gpu/RenderCommand.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

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

auto sphereObj() -> std::string {
	constexpr uint32_t rings = 24;
	constexpr uint32_t segments = 48;
	std::string obj;
	for (uint32_t ring = 0; ring <= rings; ++ring) {
		const float theta = std::numbers::pi_v<float> * static_cast<float>(ring) / static_cast<float>(rings);
		for (uint32_t segment = 0; segment <= segments; ++segment) {
			const float phi =
					2.f * std::numbers::pi_v<float> * static_cast<float>(segment) / static_cast<float>(segments);
			const float x = std::sin(theta) * std::cos(phi);
			const float y = std::cos(theta);
			const float z = std::sin(theta) * std::sin(phi);
			obj += std::format("v {} {} {}\nvn {} {} {}\nvt {} {}\n", x, y, z, x, y, z,
							   static_cast<float>(segment) / static_cast<float>(segments),
							   1.f - static_cast<float>(ring) / static_cast<float>(rings));
		}
	}
	const auto index = [](const uint32_t iRing, const uint32_t iSegment) -> uint32_t {
		return iRing * (segments + 1) + iSegment + 1;
	};
	const auto corner = [](const uint32_t iIndex) -> std::string { return std::format("{0}/{0}/{0}", iIndex); };
	for (uint32_t ring = 0; ring < rings; ++ring) {
		for (uint32_t segment = 0; segment < segments; ++segment) {
			const auto a = corner(index(ring, segment));
			const auto b = corner(index(ring + 1, segment));
			const auto c = corner(index(ring + 1, segment + 1));
			const auto d = corner(index(ring, segment + 1));
			obj += std::format("f {} {} {}\nf {} {} {}\n", a, d, b, b, d, c);
		}
	}
	return obj;
}

auto cubeObj() -> std::string {
	struct Face {
		std::array<float, 3> normal;
		std::array<float, 3> u;
		std::array<float, 3> v;
	};
	constexpr float half = 0.8f;
	constexpr std::array<Face, 6> faces{{{{1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f, 0.f}},
										 {{-1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f}},
										 {{0.f, 1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, -1.f}},
										 {{0.f, -1.f, 0.f}, {1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}},
										 {{0.f, 0.f, 1.f}, {1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}},
										 {{0.f, 0.f, -1.f}, {-1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}}}};
	constexpr std::array<std::array<float, 2>, 4> corners{{{-1.f, -1.f}, {1.f, -1.f}, {1.f, 1.f}, {-1.f, 1.f}}};
	std::string obj;
	uint32_t base = 1;
	for (const auto& face: faces) {
		obj += std::format("vn {} {} {}\n", face.normal[0], face.normal[1], face.normal[2]);
		for (const auto& [cu, cv]: corners) {
			obj += std::format("v {} {} {}\nvt {} {}\n", half * (face.normal[0] + cu * face.u[0] + cv * face.v[0]),
							   half * (face.normal[1] + cu * face.u[1] + cv * face.v[1]),
							   half * (face.normal[2] + cu * face.u[2] + cv * face.v[2]), (cu + 1.f) * 0.5f,
							   (cv + 1.f) * 0.5f);
		}
		const auto normal = static_cast<uint32_t>(&face - faces.data()) + 1;
		obj += std::format("f {0}/{0}/{4} {1}/{1}/{4} {2}/{2}/{4}\nf {0}/{0}/{4} {2}/{2}/{4} {3}/{3}/{4}\n", base,
						   base + 1, base + 2, base + 3, normal);
		base += 4;
	}
	return obj;
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
	const auto projection = renderer::toClipSpace(
			math::perspectiveReversedZ(g_fov, std::max(iAspect, 0.01f),
									   std::max(distance - radius * 2.f, distance * 0.01f), distance + radius * 2.f),
			iVulkanClip ? renderer::ClipSpace::Vulkan : renderer::ClipSpace::OpenGl);
	renderer::Camera camera{projection};
	camera.setTransform(orbitTransform(iGeometry, iOrbit));
	return camera;
}

void renderMeshPreview(renderer::gpu::Framebuffer& ioTarget, renderer::MeshResource& ioMesh, const MeshOrbit& iOrbit,
					   const MeshShading iShading, const MeshPreviewOptions& iOptions) {
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
	const bool skinned = ioMesh.isSkinned() && iOptions.skin.size() == ioMesh.getSkeleton()->getBoneCount();
	if (skinned || iOptions.lod > 0) {
		const uint32_t first = skinned ? renderer::RendererMesh::pushSkin(iOptions.skin) : 0;
		for (size_t index = 0; index < ioMesh.getSubmeshes().size(); ++index) {
			const renderer::MeshDrawParams params{.tint = textured ? math::vec4{1.f, 1.f, 1.f, 1.f} : g_shadedTint,
												  .entityId = -1,
												  .material = textured ? ioMesh.getFileMaterial(index).get() : nullptr};
			if (skinned)
				renderer::RendererMesh::drawSkinnedSubmesh(ioMesh, index, iOptions.lod, math::identity<float, 4>(),
														   first, params);
			else
				renderer::RendererMesh::drawSubmesh(ioMesh, index, iOptions.lod, math::identity<float, 4>(), params);
		}
		renderer::RendererMesh::endScene();
		ioTarget.unbind();
		return;
	}
	renderer::RendererMesh::drawMesh(ioMesh, math::identity<float, 4>(),
									 textured ? ioMesh.getBaseColorTexture() : shared<renderer::gpu::Texture2D>{},
									 {.tint = textured ? ioMesh.getBaseColorFactor() : g_shadedTint, .entityId = -1},
									 textured && ioMesh.getBaseColorFactor().a() < 1.f);
	renderer::RendererMesh::endScene();
	ioTarget.unbind();
}

auto projectToCanvas(const math::mat4& iViewProjection, const math::vec3& iPoint, const math::vec2& iCanvasSize,
					 const bool iVulkanClip) -> std::optional<math::vec2> {
	const math::vec4 clip = iViewProjection * math::vec4{iPoint.x(), iPoint.y(), iPoint.z(), 1.f};
	if (clip.w() <= 1e-6f)
		return std::nullopt;
	const float x = clip.x() / clip.w();
	const float y = clip.y() / clip.w();
	const float down = iVulkanClip ? y * 0.5f + 0.5f : 0.5f - y * 0.5f;
	return math::vec2{(x * 0.5f + 0.5f) * iCanvasSize.x(), down * iCanvasSize.y()};
}

auto makePreviewShape(const PreviewShape iShape) -> shared<renderer::MeshResource> {
	const auto text = iShape == PreviewShape::Sphere ? sphereObj() : cubeObj();
	const std::vector<uint8_t> bytes(text.begin(), text.end());
	const auto loaded = data::geometry::MeshLoader::loadMeshFromMemory(
			std::span<const uint8_t>{bytes}, ".obj", iShape == PreviewShape::Sphere ? "sphere" : "cube");
	if (!loaded.mesh)
		return nullptr;
	return mkShared<renderer::MeshResource>(std::filesystem::path{}, loaded);
}

void renderMaterialPreview(renderer::gpu::Framebuffer& ioTarget, renderer::MeshResource& ioMesh,
						   const MeshOrbit& iOrbit, const renderer::Material& iMaterial) {
	const auto& size = ioTarget.getSpecification().size;
	const float aspect = size.y() > 0 ? static_cast<float>(size.x()) / static_cast<float>(size.y()) : 1.f;
	const bool vulkan = renderer::gpu::RenderCommand::getApi() == renderer::gpu::RenderAPI::Type::Vulkan;
	const auto camera = makeMeshPreviewCamera(ioMesh.getGeometry(), iOrbit, aspect, vulkan);
	const math::mat4 world = orbitTransform(ioMesh.getGeometry(), iOrbit);
	const math::vec4 back = world * math::vec4{0.f, 0.f, -1.f, 0.f};
	const math::vec3 sun = math::vec3{back.x() + 0.35f, back.y() - 0.8f, back.z()}.normalized();
	ioTarget.bind();
	renderer::gpu::RenderCommand::setClearColor(g_background);
	renderer::gpu::RenderCommand::clear();
	ioTarget.clearAttachment(1, -1);
	renderer::RendererMesh::beginScene(camera, {.sunDirection = sun, .ambient = {0.32f, 0.32f, 0.36f}});
	renderer::RendererMesh::drawSubmesh(ioMesh, 0, math::identity<float, 4>(),
										{.tint = {1.f, 1.f, 1.f, 1.f}, .entityId = -1, .material = &iMaterial});
	renderer::RendererMesh::endScene();
	ioTarget.unbind();
}

}// namespace owl::nest::viewer
