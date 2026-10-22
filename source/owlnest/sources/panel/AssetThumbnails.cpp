/**
 * @file AssetThumbnails.cpp
 * @author Silmaen
 * @date 11/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "AssetThumbnails.h"

#include "../document/viewer/MeshPreview.h"

#include <core/Log.h>
#include <core/Timestep.h>
#include <data/geometry/MeshLoader.h>
#include <gui/UiLayer.h>
#include <math/trigonometry.h>
#include <renderer/gpu/RenderCommand.h>
#include <scene/Entity.h>
#include <scene/PrefabSerializer.h>
#include <scene/SceneSerializer.h>
#include <scene/Tileset.h>
#include <scene/component/Transform.h>
#include <sound/Waveform.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

namespace owl::nest::panel {

namespace {

// Image and sound reads per frame, so a large folder never stalls a frame.
constexpr int g_readsPerFrame = 3;

auto writeTimeOf(const std::filesystem::path& iPath) -> std::filesystem::file_time_type {
	std::error_code error;
	const auto time = std::filesystem::last_write_time(iPath, error);
	return error ? std::filesystem::file_time_type{} : time;
}

// Orthographic camera framing every entity of the scene, seen from the front.
auto frameEverything(const shared<scene::Scene>& iScene) -> renderer::CameraEditor {
	math::vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
	math::vec2 high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
	bool any = false;
	for (const auto entity: iScene->registry.view<scene::component::Transform>()) {
		const math::Transform world = iScene->getWorldTransform(scene::Entity{entity, iScene.get()});
		const float halfX = std::abs(world.scale().x()) * 0.5f;
		const float halfY = std::abs(world.scale().y()) * 0.5f;
		low = {std::min(low.x(), world.translation().x() - halfX), std::min(low.y(), world.translation().y() - halfY)};
		high = {std::max(high.x(), world.translation().x() + halfX),
				std::max(high.y(), world.translation().y() + halfY)};
		any = true;
	}
	renderer::CameraEditor camera(30.f, 1.f, 0.1f, 1000.f);
	camera.setViewportSize({AssetThumbnails::renderSize, AssetThumbnails::renderSize});
	camera.setProjection(renderer::CameraEditor::Projection::Orthographic);
	if (any) {
		const math::vec2 size = high - low;
		camera.frame({(low.x() + high.x()) * 0.5f, (low.y() + high.y()) * 0.5f, 0.f},
					 0.5f * std::sqrt(size.x() * size.x() + size.y() * size.y()));
	}
	return camera;
}

}// namespace

AssetThumbnails::AssetThumbnails() = default;

AssetThumbnails::~AssetThumbnails() = default;

auto AssetThumbnails::getKind(const std::filesystem::path& iPath) -> Kind {
	const auto extension = iPath.extension().string();
	if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
		return Kind::Image;
	if (extension == ".owl")
		return Kind::Scene;
	if (extension == ".owlprefab")
		return Kind::Prefab;
	if (extension == ".owlhud")
		return Kind::Hud;
	if (extension == ".wav" || extension == ".ogg" || extension == ".flac" || extension == ".mp3")
		return Kind::Sound;
	if (extension == ".owltileset")
		return Kind::Tileset;
	if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb")
		return Kind::Mesh;
	return Kind::None;
}

auto AssetThumbnails::projectMesh(const data::geometry::StaticMesh& iMesh) -> std::vector<MeshTriangle> {
	const auto& triangles = iMesh.getTriangles();
	if (triangles.empty())
		return {};
	// Isometric view: a turn of 35 degrees around Y, then a tilt of 25 degrees towards the viewer.
	const float yaw = math::radians(35.f);
	const float pitch = math::radians(25.f);
	const auto view = [&](const math::vec3& iPoint) -> math::vec3 {
		const float x = iPoint.x() * std::cos(yaw) + iPoint.z() * std::sin(yaw);
		const float z0 = -iPoint.x() * std::sin(yaw) + iPoint.z() * std::cos(yaw);
		const float y = iPoint.y() * std::cos(pitch) - z0 * std::sin(pitch);
		const float z = iPoint.y() * std::sin(pitch) + z0 * std::cos(pitch);
		return {x, y, z};
	};
	struct Projected {
		std::array<math::vec3, 3> corners;
		float depth;
		float shade;
	};
	std::vector<Projected> projected;
	const size_t stride = std::max<size_t>(1, triangles.size() / maxMeshTriangles);
	projected.reserve(triangles.size() / stride + 1);
	math::vec2 low{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
	math::vec2 high{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
	for (size_t i = 0; i < triangles.size(); i += stride) {
		const auto& triangle = triangles[i];
		Projected item{};
		bool valid = true;
		for (uint8_t corner = 0; corner < 3; ++corner) {
			const auto* vertex = triangle.getVertex(corner);
			if (vertex == nullptr) {
				valid = false;
				break;
			}
			item.corners[corner] = view(vertex->getPosition());
			low = {std::min(low.x(), item.corners[corner].x()), std::min(low.y(), item.corners[corner].y())};
			high = {std::max(high.x(), item.corners[corner].x()), std::max(high.y(), item.corners[corner].y())};
		}
		if (!valid)
			continue;
		item.depth = (item.corners[0].z() + item.corners[1].z() + item.corners[2].z()) / 3.f;
		const math::vec3 normal = view(triangle.getNormal());
		// Light from the viewer: either side of a face is lit, the models need not be closed.
		item.shade = 0.3f + 0.7f * std::min(1.f, std::abs(normal.z()));
		projected.push_back(item);
	}
	if (projected.empty())
		return {};
	std::ranges::sort(projected, [](const Projected& iFirst, const Projected& iSecond) -> bool {
		return iFirst.depth < iSecond.depth;
	});
	const float extent = std::max({high.x() - low.x(), high.y() - low.y(), 1e-6f});
	const math::vec2 centre{(low.x() + high.x()) * 0.5f, (low.y() + high.y()) * 0.5f};
	constexpr float fill = 0.84f;
	std::vector<MeshTriangle> result;
	result.reserve(projected.size());
	for (const auto& item: projected) {
		MeshTriangle triangle{.corners = {}, .shade = item.shade};
		for (size_t corner = 0; corner < 3; ++corner)
			triangle.corners[corner] = {0.5f + (item.corners[corner].x() - centre.x()) / extent * fill,
										0.5f - (item.corners[corner].y() - centre.y()) / extent * fill};
		result.push_back(triangle);
	}
	return result;
}

void AssetThumbnails::beginFrame() {
	m_readBudget = g_readsPerFrame;
	m_requestedThisFrame = 0;
	m_readyThisFrame = 0;
}

auto AssetThumbnails::get(const std::filesystem::path& iPath) -> const Preview* {
	const Kind kind = getKind(iPath);
	if (kind == Kind::None)
		return nullptr;
	++m_requestedThisFrame;
	const auto key = iPath.string();
	const auto writeTime = writeTimeOf(iPath);
	auto [it, inserted] = m_entries.try_emplace(key);
	auto& entry = it->second;
	if (!inserted && entry.writeTime != writeTime) {
		// The file changed: make the preview again, reusing its framebuffer.
		if (entry.preview.has_value() && entry.preview->texture)
			gui::UiLayer::deferTextureRelease(std::move(entry.preview->texture));
		entry.preview.reset();
		entry.failed = false;
		inserted = true;
	}
	entry.writeTime = writeTime;
	if (entry.preview.has_value() || entry.failed) {
		++m_readyThisFrame;
		return entry.preview.has_value() ? &*entry.preview : nullptr;
	}
	switch (kind) {
		case Kind::Image:
			if (m_readBudget > 0) {
				--m_readBudget;
				entry.preview = Preview{.texture = renderer::gpu::Texture2D::create(iPath),
										.framebuffer = {},
										.waveform = {},
										.mesh = {}};
				entry.failed = !entry.preview->texture;
			}
			break;
		case Kind::Sound:
			if (m_readBudget > 0) {
				--m_readBudget;
				auto waveform = sound::readWaveform(iPath, waveformBuckets);
				entry.failed = waveform.empty();
				if (!entry.failed)
					entry.preview =
							Preview{.texture = {}, .framebuffer = {}, .waveform = std::move(waveform), .mesh = {}};
			}
			break;
		case Kind::Tileset:
			if (m_readBudget > 0) {
				--m_readBudget;
				scene::Tileset tileset;
				entry.failed = !tileset.loadFromFile(iPath) || !tileset.texture;
				if (!entry.failed)
					entry.preview = Preview{.texture = tileset.texture, .framebuffer = {}, .waveform = {}, .mesh = {}};
			}
			break;
		case Kind::Mesh:
			if (m_meshStyle != MeshStyle::Flat) {
				if (std::ranges::find(m_pending, iPath) == m_pending.end() &&
					(!m_job.has_value() || m_job->path != iPath))
					m_pending.push_back(iPath);
			} else if (m_readBudget > 0) {
				// A mesh file is the heaviest read: it takes the whole budget of the frame.
				m_readBudget = 0;
				const auto mesh = data::geometry::MeshLoader::loadStaticMesh(iPath);
				auto triangles = mesh ? projectMesh(*mesh) : std::vector<MeshTriangle>{};
				entry.failed = triangles.empty();
				if (!entry.failed)
					entry.preview =
							Preview{.texture = {}, .framebuffer = {}, .waveform = {}, .mesh = std::move(triangles)};
			}
			break;
		case Kind::Scene:
		case Kind::Prefab:
		case Kind::Hud:
			if (inserted || std::ranges::find(m_pending, iPath) == m_pending.end()) {
				if (!m_job.has_value() || m_job->path != iPath)
					m_pending.push_back(iPath);
			}
			break;
		case Kind::None:
			break;
	}
	return entry.preview.has_value() ? &*entry.preview : nullptr;
}

auto AssetThumbnails::takeFramebuffer() -> shared<renderer::gpu::Framebuffer> {
	if (m_framebuffers.size() < maxRendered) {
		using Format = renderer::gpu::AttachmentSpecification::Format;
		using Tiling = renderer::gpu::AttachmentSpecification::Tiling;
		// Same attachments as the viewport: the 2D pipelines are built for them.
		m_framebuffers.push_back(renderer::gpu::Framebuffer::create(
				{.size = {renderSize, renderSize},
				 .attachments = {{.format = Format::Surface, .tiling = Tiling::Optimal},
								 {.format = Format::RedInteger, .tiling = Tiling::Optimal},
								 {.format = Format::Depth32F, .tiling = Tiling::Optimal}},
				 .samples = 1,
				 .swapChainTarget = false,
				 .debugName = "thumbnail"}));
		return m_framebuffers.back();
	}
	// Reuse the framebuffer of the oldest render; its file goes back to "not made".
	Entry* oldest = nullptr;
	for (auto& entry: m_entries | std::views::values) {
		if (entry.preview.has_value() && entry.preview->framebuffer &&
			(oldest == nullptr || entry.renderOrder < oldest->renderOrder))
			oldest = &entry;
	}
	if (oldest == nullptr)
		return m_framebuffers.front();
	auto framebuffer = oldest->preview->framebuffer;
	oldest->preview.reset();
	return framebuffer;
}

auto AssetThumbnails::startJob() -> bool {
	while (!m_pending.empty()) {
		const auto path = m_pending.front();
		m_pending.pop_front();
		auto& entry = m_entries[path.string()];
		if (entry.preview.has_value() || entry.failed)
			continue;
		if (getKind(path) == Kind::Mesh) {
			const auto loadedMesh = data::geometry::MeshLoader::loadMesh(path);
			if (!loadedMesh.mesh) {
				OWL_WARN("AssetThumbnails: {} could not be loaded, it keeps its icon.", path.string())
				entry.failed = true;
				continue;
			}
			m_job = Job{.path = path,
						.scene = {},
						.mesh = mkShared<renderer::MeshResource>(path, loadedMesh),
						.camera = {},
						.framebuffer = takeFramebuffer(),
						.frames = 0,
						.start = std::chrono::steady_clock::now()};
			return true;
		}
		auto loaded = mkShared<scene::Scene>();
		bool ok = false;
		if (getKind(path) == Kind::Scene || getKind(path) == Kind::Hud) {
			ok = scene::SceneSerializer(loaded, scene::documentKindOf(path)).deserialize(path).has_value();
		} else {
			ok = static_cast<bool>(scene::PrefabSerializer::instantiate(path, loaded));
		}
		if (!ok) {
			OWL_WARN("AssetThumbnails: {} could not be loaded, it keeps its icon.", path.string())
			entry.failed = true;
			continue;
		}
		Job job{.path = path,
				.scene = loaded,
				.mesh = {},
				.camera = {},
				.framebuffer = {},
				.frames = 0,
				.start = std::chrono::steady_clock::now()};
		if (getKind(path) == Kind::Prefab || !loaded->getPrimaryCamera())
			job.camera = frameEverything(loaded);
		job.framebuffer = takeFramebuffer();
		m_job = std::move(job);
		return true;
	}
	return false;
}

void AssetThumbnails::onUpdate() {
	if (!m_job.has_value() && !startJob())
		return;
	auto& job = *m_job;
	if (job.mesh) {
		viewer::renderMeshPreview(*job.framebuffer, *job.mesh, {},
								  m_meshStyle == MeshStyle::Shaded ? viewer::MeshShading::Shaded
																   : viewer::MeshShading::Textured);
		finishJob();
		return;
	}
	job.framebuffer->bind();
	renderer::gpu::RenderCommand::setClearColor({0.07f, 0.075f, 0.09f, 1.f});
	renderer::gpu::RenderCommand::clear();
	job.framebuffer->clearAttachment(1, -1);
	job.scene->onViewportResize({renderSize, renderSize});
	if (job.camera.has_value())
		job.scene->onUpdateEditor(core::Timestep{}, *job.camera);
	else
		job.scene->onRenderRuntime();
	job.framebuffer->unbind();
	finishJob();
}

void AssetThumbnails::finishJob() {
	auto& job = *m_job;
	const std::chrono::duration<float> elapsed = std::chrono::steady_clock::now() - job.start;
	if (++job.frames < renderFrames || elapsed.count() < renderSeconds)
		return;
	auto& entry = m_entries[job.path.string()];
	entry.preview = Preview{.texture = {}, .framebuffer = job.framebuffer, .waveform = {}, .mesh = {}};
	entry.renderOrder = m_nextOrder++;
	m_job.reset();
}

void AssetThumbnails::setMeshStyle(const MeshStyle iStyle) {
	if (iStyle == m_meshStyle)
		return;
	m_meshStyle = iStyle;
	if (m_job.has_value() && m_job->mesh)
		m_job.reset();
	std::erase_if(m_pending, [](const std::filesystem::path& iPath) -> bool { return getKind(iPath) == Kind::Mesh; });
	for (auto& [key, entry]: m_entries) {
		if (getKind(key) == Kind::Mesh) {
			entry.preview.reset();
			entry.failed = false;
		}
	}
}

void AssetThumbnails::clear() {
	for (auto& entry: m_entries | std::views::values) {
		if (entry.preview.has_value() && entry.preview->texture)
			gui::UiLayer::deferTextureRelease(std::move(entry.preview->texture));
	}
	m_entries.clear();
	m_pending.clear();
	m_job.reset();
	m_framebuffers.clear();
}

}// namespace owl::nest::panel
