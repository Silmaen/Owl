/**
 * @file PlanetTerrain.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/PlanetTerrain.h"

#include "core/Macros.h"
#include "core/task/Scheduler.h"
#include "core/task/Task.h"
#include "renderer/gpu/RenderCommand.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <numbers>
#include <utility>

namespace owl::renderer {

namespace {

struct FaceBasis {
	math::vec3d normal;
	math::vec3d right;
	math::vec3d up;
};

// Right-handed per face (`right ^ up == normal`): a grid walked along right then up winds counter-clockwise outside.
constexpr std::array<FaceBasis, g_planetFaces> g_faces{
		FaceBasis{.normal = {1.0, 0.0, 0.0}, .right = {0.0, 0.0, -1.0}, .up = {0.0, 1.0, 0.0}},
		FaceBasis{.normal = {-1.0, 0.0, 0.0}, .right = {0.0, 0.0, 1.0}, .up = {0.0, 1.0, 0.0}},
		FaceBasis{.normal = {0.0, 1.0, 0.0}, .right = {1.0, 0.0, 0.0}, .up = {0.0, 0.0, -1.0}},
		FaceBasis{.normal = {0.0, -1.0, 0.0}, .right = {1.0, 0.0, 0.0}, .up = {0.0, 0.0, 1.0}},
		FaceBasis{.normal = {0.0, 0.0, 1.0}, .right = {1.0, 0.0, 0.0}, .up = {0.0, 1.0, 0.0}},
		FaceBasis{.normal = {0.0, 0.0, -1.0}, .right = {-1.0, 0.0, 0.0}, .up = {0.0, 1.0, 0.0}},
};

auto toFloat(const math::vec3d& iVector) -> math::vec3 {
	return {static_cast<float>(iVector.x()), static_cast<float>(iVector.y()), static_cast<float>(iVector.z())};
}

auto clampFinite(const float iValue, const float iMin, const float iMax, const float iFallback) -> float {
	return std::isfinite(iValue) ? std::clamp(iValue, iMin, iMax) : iFallback;
}

auto clampFinite(const double iValue, const double iMin, const double iMax, const double iFallback) -> double {
	return std::isfinite(iValue) ? std::clamp(iValue, iMin, iMax) : iFallback;
}

auto clampColor(const math::vec3& iColor) -> math::vec3 {
	return {clampFinite(iColor.x(), 0.f, 1.f, 0.f), clampFinite(iColor.y(), 0.f, 1.f, 0.f),
			clampFinite(iColor.z(), 0.f, 1.f, 0.f)};
}

auto lerp(const math::vec3& iA, const math::vec3& iB, const float iT) -> math::vec3 {
	return iA + (iB - iA) * std::clamp(iT, 0.f, 1.f);
}

auto packColor(const math::vec3& iColor) -> uint32_t {
	const auto channel = [](const float iValue) -> uint32_t {
		return static_cast<uint32_t>(std::lround(std::clamp(iValue, 0.f, 1.f) * 255.f));
	};
	return channel(iColor.x()) | channel(iColor.y()) << 8u | channel(iColor.z()) << 16u | 0xff000000u;
}

// Square of the distance from a point to the bounding sphere of a chunk, 0 inside it.
auto distanceToChunk(const PlanetSettings& iSettings, const PlanetChunkKey& iKey, const math::vec3d& iEye) -> double {
	const double size = getChunkSize(iKey, iSettings.radius);
	const double top =
			static_cast<double>(iSettings.heightScale) * (1.0 + static_cast<double>(iSettings.detailAmplitude));
	const math::vec3d center = getChunkDirection(iKey) * (iSettings.radius + top * 0.5);
	const double bound = size * 0.75 + top * 0.5;
	return std::max(0.0, (iEye - center).norm() - bound);
}

void selectNode(const PlanetSettings& iSettings, const math::vec3d& iEye, const PlanetChunkKey& iKey,
				std::vector<PlanetChunkKey>& oLeaves) {// NOLINT(misc-no-recursion)
	if (iKey.level >= iSettings.maxLevel ||
		distanceToChunk(iSettings, iKey, iEye) >=
				static_cast<double>(iSettings.lodFactor) * getChunkSize(iKey, iSettings.radius)) {
		oLeaves.push_back(iKey);
		return;
	}
	const auto level = static_cast<uint8_t>(iKey.level + 1);
	for (uint32_t child = 0; child < 4; ++child)
		selectNode(iSettings, iEye,
				   {.face = iKey.face, .level = level, .x = iKey.x * 2 + (child & 1u), .y = iKey.y * 2 + (child >> 1u)},
				   oLeaves);
}

// Corners of a chunk in face coordinates: (u0, v0, u1, v1).
auto chunkRect(const PlanetChunkKey& iKey) -> std::array<double, 4> {
	const double cells = std::ldexp(1.0, iKey.level);
	const double step = 2.0 / cells;
	return {-1.0 + step * static_cast<double>(iKey.x), -1.0 + step * static_cast<double>(iKey.y),
			-1.0 + step * static_cast<double>(iKey.x + 1), -1.0 + step * static_cast<double>(iKey.y + 1)};
}

auto isGpuBackend() -> bool {
	return gpu::RenderCommand::getApi() != gpu::RenderAPI::Type::Null && Renderer3D::isReady();
}

}// namespace

auto PlanetSettings::clamped() const -> PlanetSettings {
	PlanetSettings out = *this;
	const PlanetSettings defaults;
	out.radius = clampFinite(radius, 1.0, 1e12, defaults.radius);
	out.heightScale = clampFinite(heightScale, 0.f, 1e7f, defaults.heightScale);
	out.seaLevel = clampFinite(seaLevel, 0.f, 1.f, defaults.seaLevel);
	out.detailAmplitude = clampFinite(detailAmplitude, 0.f, 1.f, defaults.detailAmplitude);
	out.detailFrequency = clampFinite(detailFrequency, 0.f, 10000.f, defaults.detailFrequency);
	out.maxLevel = std::min(maxLevel, g_planetMaxLevel);
	out.lodFactor = clampFinite(lodFactor, 0.5f, 16.f, defaults.lodFactor);
	out.chunkResolution = std::clamp(chunkResolution, 2u, g_planetMaxChunkResolution);
	out.seaColor = clampColor(seaColor);
	out.lowColor = clampColor(lowColor);
	out.highColor = clampColor(highColor);
	out.peakColor = clampColor(peakColor);
	out.atmosphereHeight = clampFinite(atmosphereHeight, 0.f, 1e8f, defaults.atmosphereHeight);
	out.rayleighDepth = {clampFinite(rayleighDepth.x(), 0.f, 10.f, 0.f), clampFinite(rayleighDepth.y(), 0.f, 10.f, 0.f),
						 clampFinite(rayleighDepth.z(), 0.f, 10.f, 0.f)};
	out.mieDepth = clampFinite(mieDepth, 0.f, 10.f, defaults.mieDepth);
	out.scatteringIntensity = clampFinite(scatteringIntensity, 0.f, 1000.f, defaults.scatteringIntensity);
	out.scaledSpaceDistance = clampFinite(scaledSpaceDistance, 0.0, 1e15, 0.0);
	if (out.scaledSpaceDistance > 0.0)
		out.scaledSpaceDistance = std::max(out.scaledSpaceDistance, out.radius * 2.0);
	return out;
}

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wfloat-equal")
auto PlanetSettings::operator==(const PlanetSettings& iOther) const -> bool = default;
OWL_DIAG_POP

auto PlanetHeightMap::sampleGrid(const float iU, const float iV) const -> float {
	if (size.x() == 0 || size.y() == 0 || values.size() < static_cast<size_t>(size.x()) * size.y())
		return 0.5f;
	const float fx = std::clamp(iU, 0.f, 1.f) * static_cast<float>(size.x() - 1);
	const float fy = std::clamp(iV, 0.f, 1.f) * static_cast<float>(size.y() - 1);
	const auto x0 = static_cast<uint32_t>(fx);
	const auto y0 = static_cast<uint32_t>(fy);
	const uint32_t x1 = std::min(x0 + 1, size.x() - 1);
	const uint32_t y1 = std::min(y0 + 1, size.y() - 1);
	const float tx = fx - static_cast<float>(x0);
	const float ty = fy - static_cast<float>(y0);
	const auto at = [this](const uint32_t iX, const uint32_t iY) -> float {
		return values[static_cast<size_t>(iY) * size.x() + iX];
	};
	const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
	const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
	return top + (bottom - top) * ty;
}

auto PlanetHeightMap::sample(const math::vec3d& iDirection) const -> float {
	if (size.x() == 0 || size.y() == 0)
		return 0.5f;
	const auto x = static_cast<float>(iDirection.x());
	const auto y = static_cast<float>(iDirection.y());
	const auto z = static_cast<float>(iDirection.z());
	const auto weight = [](const float iValue) -> float {
		const float square = iValue * iValue;
		return square * square;
	};
	const float wx = weight(x);
	const float wy = weight(y);
	const float wz = weight(z);
	const float total = wx + wy + wz;
	if (total <= 0.f)
		return 0.5f;
	// Each projection is weighted out on its own plane: its two sides may take different windows of the map.
	const auto project = [this](const float iA, const float iB, const bool iBack) -> float {
		const float u = (iA + 1.f) * 0.5f;
		const float v = (iB + 1.f) * 0.5f;
		return iBack ? sampleGrid(1.f - v, u) : sampleGrid(u, v);
	};
	return (project(z, y, x < 0.f) * wx + project(x, z, y < 0.f) * wy + project(x, y, z < 0.f) * wz) / total;
}

PlanetSurface::PlanetSurface(const PlanetSettings& iSettings, PlanetHeightMap iHeights, const uint64_t iSeed)
	: m_settings{iSettings.clamped()}, m_heights{std::move(iHeights)}, m_seed{iSeed},
	  m_noise{static_cast<uint32_t>(iSeed ^ (iSeed >> 32u))} {}

auto PlanetSurface::getValue(const math::vec3d& iDirection) const -> float {
	float value = m_heights.sample(iDirection);
	if (m_settings.detailAmplitude > 0.f && m_settings.detailFrequency > 0.f) {
		const float frequency = m_settings.detailFrequency / std::numbers::pi_v<float>;
		value += m_settings.detailAmplitude * m_noise.fbm(static_cast<float>(iDirection.x()) * frequency,
														  static_cast<float>(iDirection.y()) * frequency,
														  static_cast<float>(iDirection.z()) * frequency, 4, 2.f, 0.5f);
	}
	return value;
}

auto PlanetSurface::getHeightOf(const float iValue) const -> double {
	return static_cast<double>(std::max(iValue - m_settings.seaLevel, 0.f) * m_settings.heightScale);
}

auto PlanetSurface::getSurfaceRadius(const math::vec3d& iDirection) const -> double {
	return m_settings.radius + getHeightOf(getValue(iDirection));
}

auto PlanetChunkKey::packed() const -> uint64_t {
	constexpr uint64_t mask = (uint64_t{1} << 28u) - 1;
	return static_cast<uint64_t>(face & 7u) << 61u | static_cast<uint64_t>(level & 31u) << 56u | (x & mask) << 28u |
		   (y & mask);
}

auto PlanetChunkKey::getParent() const -> PlanetChunkKey {
	if (level == 0)
		return *this;
	return {.face = face, .level = static_cast<uint8_t>(level - 1), .x = x / 2, .y = y / 2};
}

auto cubeToSphere(const uint8_t iFace, const double iU, const double iV) -> math::vec3d {
	const auto& [normal, right, up] = g_faces[iFace % g_planetFaces];
	const math::vec3d cube = normal + right * iU + up * iV;
	const double x2 = cube.x() * cube.x();
	const double y2 = cube.y() * cube.y();
	const double z2 = cube.z() * cube.z();
	const math::vec3d sphere{cube.x() * std::sqrt(std::max(0.0, 1.0 - y2 / 2.0 - z2 / 2.0 + y2 * z2 / 3.0)),
							 cube.y() * std::sqrt(std::max(0.0, 1.0 - x2 / 2.0 - z2 / 2.0 + x2 * z2 / 3.0)),
							 cube.z() * std::sqrt(std::max(0.0, 1.0 - x2 / 2.0 - y2 / 2.0 + x2 * y2 / 3.0))};
	const double length = sphere.norm();
	return length > 0.0 ? sphere * (1.0 / length) : normal;
}

auto getChunkDirection(const PlanetChunkKey& iKey) -> math::vec3d {
	const auto [u0, v0, u1, v1] = chunkRect(iKey);
	return cubeToSphere(iKey.face, (u0 + u1) * 0.5, (v0 + v1) * 0.5);
}

auto getChunkSize(const PlanetChunkKey& iKey, const double iRadius) -> double {
	return iRadius * std::numbers::pi / 2.0 / std::ldexp(1.0, iKey.level);
}

void selectPlanetChunks(const PlanetSettings& iSettings, const math::vec3d& iEye,
						std::vector<PlanetChunkKey>& oLeaves) {
	OWL_PROFILE_FUNCTION()

	oLeaves.clear();
	const auto settings = iSettings.clamped();
	for (uint8_t face = 0; face < g_planetFaces; ++face)
		selectNode(settings, iEye, {.face = face, .level = 0, .x = 0, .y = 0}, oLeaves);
}

auto buildPlanetChunk(const PlanetSurface& iSurface, const PlanetChunkKey& iKey) -> PlanetChunkMesh {
	const auto& settings = iSurface.getSettings();
	const uint32_t res = settings.chunkResolution;
	const uint32_t side = res + 1;
	const uint32_t padded = res + 3;
	const auto [u0, v0, u1, v1] = chunkRect(iKey);
	const double du = (u1 - u0) / static_cast<double>(res);
	const double dv = (v1 - v0) / static_cast<double>(res);
	PlanetChunkMesh mesh;
	mesh.key = iKey;
	mesh.center = getChunkDirection(iKey) * settings.radius;
	// Ground points on a grid one cell wider on every side, for the normals of the border vertices.
	std::vector<math::vec3d> points(static_cast<size_t>(padded) * padded);
	std::vector<float> values(points.size());
	double low = std::numeric_limits<double>::max();
	double high = 0.0;
	for (uint32_t j = 0; j < padded; ++j) {
		for (uint32_t i = 0; i < padded; ++i) {
			const math::vec3d direction = cubeToSphere(iKey.face, u0 + du * (static_cast<double>(i) - 1.0),
													   v0 + dv * (static_cast<double>(j) - 1.0));
			const size_t index = static_cast<size_t>(j) * padded + i;
			values[index] = iSurface.getValue(direction);
			const double radius = settings.radius + iSurface.getHeightOf(values[index]);
			points[index] = direction * radius;
			if (i > 0 && j > 0 && i <= side && j <= side) {
				low = std::min(low, radius);
				high = std::max(high, radius);
			}
		}
	}
	mesh.radiusRange = {low, high};
	const double size = getChunkSize(iKey, settings.radius);
	const double skirt = std::max(size * 0.08, 0.5) + static_cast<double>(settings.heightScale) * 0.05;
	mesh.vertices.reserve(static_cast<size_t>(side) * side + size_t{4} * side);
	const auto vertexAt = [&](const uint32_t iI, const uint32_t iJ, const double iDrop) -> Mesh3DVertex {
		const auto at = [&](const uint32_t iX, const uint32_t iY) -> const math::vec3d& {
			return points[static_cast<size_t>(iY) * padded + iX];
		};
		const uint32_t pi = iI + 1;
		const uint32_t pj = iJ + 1;
		const math::vec3d normal = ((at(pi + 1, pj) - at(pi - 1, pj)) ^ (at(pi, pj + 1) - at(pi, pj - 1))).normalized();
		const math::vec3d& ground = at(pi, pj);
		const math::vec3d position = iDrop > 0.0 ? ground - ground.normalized() * iDrop : ground;
		const float value = values[static_cast<size_t>(pj) * padded + pi];
		return {.position = toFloat(position - mesh.center),
				.normal = toFloat(normal),
				.uv = {std::clamp(value, 0.f, 1.f), 0.5f},
				.textureIndex = 1,
				.tileRect = {0.f, 0.f, 1.f, 1.f},
				.ao = 1.f};
	};
	for (uint32_t j = 0; j < side; ++j)
		for (uint32_t i = 0; i < side; ++i) mesh.vertices.push_back(vertexAt(i, j, 0.0));
	mesh.indices.reserve(size_t{6} * res * res + size_t{24} * res);
	const auto grid = [side](const uint32_t iI, const uint32_t iJ) -> uint32_t { return iJ * side + iI; };
	for (uint32_t j = 0; j < res; ++j) {
		for (uint32_t i = 0; i < res; ++i) {
			const uint32_t a = grid(i, j);
			const uint32_t b = grid(i + 1, j);
			const uint32_t c = grid(i + 1, j + 1);
			const uint32_t d = grid(i, j + 1);
			mesh.indices.insert(mesh.indices.end(), {a, b, c, a, c, d});
		}
	}
	// Skirts: each edge walked counter-clockwise around the chunk, its copy dropped below it.
	const std::array<std::pair<math::vec2ui, math::vec2i>, 4> edges{
			std::pair{math::vec2ui{0, 0}, math::vec2i{1, 0}}, std::pair{math::vec2ui{res, 0}, math::vec2i{0, 1}},
			std::pair{math::vec2ui{res, res}, math::vec2i{-1, 0}}, std::pair{math::vec2ui{0, res}, math::vec2i{0, -1}}};
	for (const auto& [start, step]: edges) {
		const auto first = static_cast<uint32_t>(mesh.vertices.size());
		for (uint32_t k = 0; k < side; ++k) {
			const auto i = static_cast<uint32_t>(static_cast<int32_t>(start.x()) + step.x() * static_cast<int32_t>(k));
			const auto j = static_cast<uint32_t>(static_cast<int32_t>(start.y()) + step.y() * static_cast<int32_t>(k));
			mesh.vertices.push_back(vertexAt(i, j, skirt));
		}
		for (uint32_t k = 0; k < res; ++k) {
			const auto edgeAt = [&](const uint32_t iK) -> uint32_t {
				const auto i =
						static_cast<uint32_t>(static_cast<int32_t>(start.x()) + step.x() * static_cast<int32_t>(iK));
				const auto j =
						static_cast<uint32_t>(static_cast<int32_t>(start.y()) + step.y() * static_cast<int32_t>(iK));
				return grid(i, j);
			};
			const uint32_t top0 = edgeAt(k);
			const uint32_t top1 = edgeAt(k + 1);
			const uint32_t bottom0 = first + k;
			const uint32_t bottom1 = first + k + 1;
			mesh.indices.insert(mesh.indices.end(), {top0, bottom0, bottom1, top0, bottom1, top1});
		}
	}
	return mesh;
}

auto buildPlanetColorRamp(const PlanetSettings& iSettings, const uint32_t iWidth) -> std::vector<uint32_t> {
	const auto settings = iSettings.clamped();
	std::vector<uint32_t> texels(iWidth);
	const float sea = settings.seaLevel;
	for (uint32_t index = 0; index < iWidth; ++index) {
		const float value = iWidth > 1 ? static_cast<float>(index) / static_cast<float>(iWidth - 1) : 0.f;
		math::vec3 color;
		if (value < sea) {
			color = lerp(settings.seaColor * 0.45f, settings.seaColor, sea > 0.f ? value / sea : 1.f);
		} else {
			const float land = sea < 1.f ? (value - sea) / (1.f - sea) : 0.f;
			if (land < 0.55f)
				color = lerp(settings.lowColor, settings.highColor, land / 0.55f);
			else if (land < 0.8f)
				color = settings.highColor;
			else
				color = lerp(settings.highColor, settings.peakColor, (land - 0.8f) / 0.12f);
		}
		texels[index] = packColor(color);
	}
	return texels;
}

auto placePlanet(const PlanetSettings& iSettings, const math::vec3d& iCenterFromEye) -> PlanetPlacement {
	const auto settings = iSettings.clamped();
	const double distance = iCenterFromEye.norm();
	if (settings.scaledSpaceDistance <= 0.0 || distance <= settings.scaledSpaceDistance)
		return {.center = iCenterFromEye, .scale = 1.0, .scaledSpace = false};
	const double scale = settings.scaledSpaceDistance / distance;
	return {.center = iCenterFromEye * scale, .scale = scale, .scaledSpace = true};
}

PlanetTerrain::PlanetTerrain() : m_inbox{mkShared<Inbox>()} {}

PlanetTerrain::~PlanetTerrain() = default;

void PlanetTerrain::setSurface(const shared<const PlanetSurface>& iSurface) {
	if (m_surface == iSurface)
		return;
	if (m_surface && iSurface && m_surface->getSettings() == iSurface->getSettings() &&
		m_surface->getSeed() == iSurface->getSeed() && m_surface->getHeights() == iSurface->getHeights()) {
		m_surface = iSurface;
		return;
	}
	m_surface = iSurface;
	m_cache.clear();
	m_pending.clear();
	m_inbox = mkShared<Inbox>();
	m_selection.clear();
	m_drawList.clear();
	m_colorRamp.reset();
}

void PlanetTerrain::collectBuilds() {
	std::vector<PlanetChunkMesh> done;
	{
		const std::lock_guard<std::mutex> lock{m_inbox->mutex};
		done.swap(m_inbox->done);
	}
	for (auto& mesh: done) {
		const uint64_t packed = mesh.key.packed();
		m_pending.erase(packed);
		m_cache[packed] = CachedChunk{.mesh = std::move(mesh), .gpuMesh = nullptr, .lastUsed = m_frame};
		++m_stats.built;
	}
}

void PlanetTerrain::requestBuild(const PlanetChunkKey& iKey, core::task::Scheduler* ioScheduler, uint32_t& ioBudget) {
	const uint64_t packed = iKey.packed();
	if (ioBudget == 0 || m_cache.contains(packed) || m_pending.contains(packed))
		return;
	--ioBudget;
	if (ioScheduler == nullptr) {
		m_cache[packed] =
				CachedChunk{.mesh = buildPlanetChunk(*m_surface, iKey), .gpuMesh = nullptr, .lastUsed = m_frame};
		++m_stats.built;
		return;
	}
	m_pending.emplace(packed, iKey);
	ioScheduler->pushTask(core::task::Task{[surface = m_surface, inbox = m_inbox, iKey]() -> void {
		auto mesh = buildPlanetChunk(*surface, iKey);
		const std::lock_guard<std::mutex> lock{inbox->mutex};
		inbox->done.push_back(std::move(mesh));
	}});
}

auto PlanetTerrain::drawChunkOf(const uint64_t iPacked) -> CachedChunk* {
	const auto found = m_cache.find(iPacked);
	if (found == m_cache.end())
		return nullptr;
	auto& chunk = found->second;
	if (!chunk.gpuMesh && isGpuBackend() && !chunk.mesh.vertices.empty()) {
		chunk.gpuMesh = Renderer3D::createMesh(chunk.mesh.vertices, chunk.mesh.indices);
		chunk.mesh.vertices = {};
		chunk.mesh.indices = {};
	}
	return &chunk;
}

void PlanetTerrain::update(const math::vec3d& iEye, const bool iScaledSpace, core::task::Scheduler* ioScheduler) {
	OWL_PROFILE_FUNCTION()

	++m_frame;
	collectBuilds();
	m_drawList.clear();
	m_drawnKeys.clear();
	if (!m_surface) {
		m_selection.clear();
		m_stats = {.selected = 0, .drawn = 0, .cached = 0, .pending = 0, .built = m_stats.built};
		return;
	}
	const auto& settings = m_surface->getSettings();
	if (!m_colorRamp && isGpuBackend()) {
		constexpr uint32_t width = 256;
		auto texels = buildPlanetColorRamp(settings, width);
		m_colorRamp = gpu::Texture2D::create(
				gpu::Texture2D::Specification{.size = {width, 1}, .format = gpu::ImageFormat::Rgba8});
		if (m_colorRamp)
			m_colorRamp->setData(texels.data(), static_cast<uint32_t>(texels.size() * sizeof(uint32_t)));
	}
	if (iScaledSpace) {
		m_selection.clear();
		for (uint8_t face = 0; face < g_planetFaces; ++face)
			m_selection.push_back({.face = face, .level = 0, .x = 0, .y = 0});
	} else {
		selectPlanetChunks(settings, iEye, m_selection);
		std::ranges::sort(m_selection, [&](const PlanetChunkKey& iA, const PlanetChunkKey& iB) -> bool {
			return (getChunkDirection(iA) * settings.radius - iEye).normSq() <
				   (getChunkDirection(iB) * settings.radius - iEye).normSq();
		});
	}
	uint32_t budget = ioScheduler != nullptr ? g_asyncBuildBudget : g_syncBuildBudget;
	// The roots first: a leaf always has a built ancestor to stand in for it.
	for (uint8_t face = 0; face < g_planetFaces; ++face) {
		uint32_t rootBudget = 1;
		requestBuild({.face = face, .level = 0, .x = 0, .y = 0}, ioScheduler, rootBudget);
	}
	for (const auto& leaf: m_selection) {
		requestBuild(leaf, ioScheduler, budget);
		PlanetChunkKey node = leaf;
		while (!m_cache.contains(node.packed()) && node.level > 0) node = node.getParent();
		if (!m_cache.contains(node.packed()))
			continue;
		if (const uint64_t packed = node.packed(); std::ranges::find(m_drawnKeys, packed) == m_drawnKeys.end())
			m_drawnKeys.push_back(packed);
	}
	// An ancestor standing in for a leaf hides the built leaves below it: they would overlap.
	const auto hasDrawnAncestor = [this](PlanetChunkKey iNode) -> bool {
		while (iNode.level > 0) {
			iNode = iNode.getParent();
			if (std::ranges::find(m_drawnKeys, iNode.packed()) != m_drawnKeys.end())
				return true;
		}
		return false;
	};
	for (const uint64_t packed: m_drawnKeys) {
		auto* chunk = drawChunkOf(packed);
		if (chunk == nullptr || hasDrawnAncestor(chunk->mesh.key))
			continue;
		chunk->lastUsed = m_frame;
		m_drawList.push_back({.mesh = chunk->gpuMesh, .center = chunk->mesh.center, .key = chunk->mesh.key});
	}
	std::erase_if(m_cache, [this](const auto& iEntry) -> bool {
		return iEntry.second.mesh.key.level > 0 && m_frame - iEntry.second.lastUsed > g_cacheFrames;
	});
	m_stats.selected = static_cast<uint32_t>(m_selection.size());
	m_stats.drawn = static_cast<uint32_t>(m_drawList.size());
	m_stats.cached = static_cast<uint32_t>(m_cache.size());
	m_stats.pending = static_cast<uint32_t>(m_pending.size());
}

void PlanetTerrain::waitForBuilds(core::task::Scheduler* ioScheduler) {
	if (ioScheduler != nullptr)
		ioScheduler->waitEmptyQueue();
	collectBuilds();
	m_stats.pending = static_cast<uint32_t>(m_pending.size());
}

}// namespace owl::renderer
