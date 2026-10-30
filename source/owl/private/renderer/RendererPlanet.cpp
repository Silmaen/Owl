/**
 * @file RendererPlanet.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/RendererPlanet.h"

#include "renderer/ClipSpace.h"
#include "renderer/Renderer3D.h"
#include "renderer/gpu/DrawData.h"
#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

namespace owl::renderer {

namespace {

constexpr auto g_rendererKey = "planet";
constexpr uint32_t g_shellRings = 32;
constexpr uint32_t g_shellSegments = 64;
// Scale heights of the Rayleigh and Mie densities, as fractions of the atmosphere thickness.
constexpr double g_rayleighScale = 0.1;
constexpr double g_mieScale = 0.015;

auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(g_rendererKey);
	return sId;
}

struct AtmosphereUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	// xyz centre of the planet from the eye, w radius of the sea surface (drawn scale).
	math::vec4 center{0.f, 0.f, 0.f, 1.f};
	// x radius of the shell, y Rayleigh scale height, z Mie scale height, w 1 on a linear target.
	math::vec4 shell{1.f, 1.f, 1.f, 0.f};
	// xyz Rayleigh coefficients at the sea surface (per drawn metre), w the Mie one.
	math::vec4 scattering{0.f, 0.f, 0.f, 0.f};
	// xyz unit direction toward the sun.
	math::vec4 toSun{0.f, 1.f, 0.f, 0.f};
	// xyz pre-exposed illuminance of the sun times the intensity of the planet.
	math::vec4 sunColor{1.f, 1.f, 1.f, 0.f};
	// x eye height above the shell, y eye height above the sea surface (drawn scale, from the CPU in double).
	math::vec4 heights{0.f, 0.f, 0.f, 0.f};
	// x entity id, y 1 when the eye is inside the shell.
	std::array<int32_t, 4> ids{-1, 0, 0, 0};
};

struct QueuedAtmosphere {
	AtmosphereUbo ubo;
	double distance = 0.0;
	bool inside = false;
};

struct InternalData {
	shared<gpu::UniformBuffer> uniforms;
	// Front faces of the shell (an eye outside), then its back faces (an eye inside).
	std::array<shared<gpu::DrawData>, 2> shells;
	uint32_t indexCount = 0;
};

struct FrameData {
	math::mat4 relativeViewProjection = math::identity<float, 4>();
	PlanetSun sun;
	std::vector<QueuedAtmosphere> atmospheres;
	RendererPlanet::Stats stats;
	bool open = false;
};

shared<InternalData> g_data;

auto frame() -> FrameData& {
	static FrameData sFrame;
	return sFrame;
}

auto modelOf(const math::vec3d& iTranslation, const double iScale) -> math::mat4 {
	math::mat4 model = math::identity<float, 4>();
	const auto scale = static_cast<float>(iScale);
	model(0, 0) = scale;
	model(1, 1) = scale;
	model(2, 2) = scale;
	model(0, 3) = static_cast<float>(iTranslation.x());
	model(1, 3) = static_cast<float>(iTranslation.y());
	model(2, 3) = static_cast<float>(iTranslation.z());
	return model;
}

auto makeShell(std::vector<uint32_t>& oIndices) -> std::vector<math::vec3> {
	std::vector<math::vec3> vertices;
	vertices.reserve(static_cast<size_t>(g_shellRings + 1) * (g_shellSegments + 1));
	for (uint32_t ring = 0; ring <= g_shellRings; ++ring) {
		const double theta = std::numbers::pi * static_cast<double>(ring) / g_shellRings;
		for (uint32_t segment = 0; segment <= g_shellSegments; ++segment) {
			const double phi = 2.0 * std::numbers::pi * static_cast<double>(segment) / g_shellSegments;
			vertices.emplace_back(static_cast<float>(std::sin(theta) * std::cos(phi)),
								  static_cast<float>(std::cos(theta)),
								  static_cast<float>(-std::sin(theta) * std::sin(phi)));
		}
	}
	// Counter-clockwise seen from outside.
	for (uint32_t ring = 0; ring < g_shellRings; ++ring) {
		for (uint32_t segment = 0; segment < g_shellSegments; ++segment) {
			const uint32_t a = ring * (g_shellSegments + 1) + segment;
			const uint32_t b = a + g_shellSegments + 1;
			oIndices.insert(oIndices.end(), {a, b, a + 1, a + 1, b, b + 1});
		}
	}
	return vertices;
}

void queueAtmosphere(const PlanetDrawParams& iParams) {
	const auto& settings = *iParams.settings;
	if (!settings.atmosphere || settings.atmosphereHeight <= 0.f)
		return;
	auto& data = frame();
	const double scale = iParams.placement.scale;
	const math::vec3d& center = iParams.placement.center;
	const double radius = settings.radius * scale;
	const auto height = static_cast<double>(settings.atmosphereHeight);
	const double shell = (settings.radius + height) * scale;
	const double distance = center.norm();
	const double rayleighHeight = height * g_rayleighScale;
	const double mieHeight = height * g_mieScale;
	const auto coefficient = [scale](const double iDepth, const double iHeight) -> float {
		return static_cast<float>(iDepth / iHeight / scale);
	};
	const float exposure = Renderer3D::getExposure() * settings.scatteringIntensity;
	QueuedAtmosphere queued;
	queued.distance = distance;
	queued.inside = distance < shell;
	auto& ubo = queued.ubo;
	ubo.viewProjection = data.relativeViewProjection;
	ubo.center = {static_cast<float>(center.x()), static_cast<float>(center.y()), static_cast<float>(center.z()),
				  static_cast<float>(radius)};
	ubo.shell = {static_cast<float>(shell), static_cast<float>(rayleighHeight * scale),
				 static_cast<float>(mieHeight * scale), gpu::Framebuffer::isLinearTargetBound() ? 1.f : 0.f};
	ubo.scattering = {coefficient(static_cast<double>(settings.rayleighDepth.x()), rayleighHeight),
					  coefficient(static_cast<double>(settings.rayleighDepth.y()), rayleighHeight),
					  coefficient(static_cast<double>(settings.rayleighDepth.z()), rayleighHeight),
					  coefficient(static_cast<double>(settings.mieDepth), mieHeight)};
	ubo.toSun = {data.sun.toSun.x(), data.sun.toSun.y(), data.sun.toSun.z(), 0.f};
	const math::vec3 color = data.sun.illuminance * exposure;
	ubo.sunColor = {color.x(), color.y(), color.z(), 0.f};
	ubo.heights = {static_cast<float>(distance - shell), static_cast<float>(distance - radius), 0.f, 0.f};
	ubo.ids = {iParams.entityId, queued.inside ? 1 : 0, 0, 0};
	data.atmospheres.push_back(queued);
}

}// namespace

void RendererPlanet::init() {
	OWL_PROFILE_FUNCTION()

	if (g_data)
		shutdown();
	if (gpu::RenderCommand::getApi() == gpu::RenderAPI::Type::Null)
		return;
	g_data = mkShared<InternalData>();
	const std::array<gpu::BindingDecl, 1> bindings{gpu::BindingDecl{.binding = 0,
																	.type = gpu::BindingType::UniformBuffer,
																	.count = 1,
																	.stages = gpu::ShaderStage::VertexFragment}};
	gpu::RendererDescriptors::declare(g_rendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_data->uniforms = gpu::UniformBuffer::create(static_cast<uint32_t>(sizeof(AtmosphereUbo)), 0, g_rendererKey);
	std::vector<uint32_t> indices;
	const auto vertices = makeShell(indices);
	g_data->indexCount = static_cast<uint32_t>(indices.size());
	for (const auto& [slot, cull]:
		 {std::pair{size_t{0}, gpu::CullMode::Back}, std::pair{size_t{1}, gpu::CullMode::Front}}) {
		auto draw = gpu::DrawData::create();
		auto copy = indices;
		draw->init({{"a_Position", gpu::ShaderDataType::Float3}}, g_rendererKey, copy, "planet_atmosphere",
				   {.cullMode = cull, .blendMode = gpu::BlendMode::Alpha, .depthTest = true, .depthWrite = false});
		draw->setVertexData(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(math::vec3)));
		g_data->shells[slot] = draw;
	}
}

void RendererPlanet::shutdown() {
	frame().atmospheres.clear();
	if (!g_data)
		return;
	g_data.reset();
	gpu::RendererDescriptors::release(g_rendererKey);
}

auto RendererPlanet::isReady() -> bool { return g_data != nullptr; }

void RendererPlanet::beginScene(const Camera& iCamera) {
	auto& data = frame();
	data.relativeViewProjection = iCamera.getProjection() * cameraRelativeView(iCamera.getView());
	data.atmospheres.clear();
	data.open = true;
}

void RendererPlanet::setSun(const PlanetSun& iSun) {
	auto& sun = frame().sun;
	sun = iSun;
	if (const float length = sun.toSun.norm(); length > 0.f)
		sun.toSun = sun.toSun * (1.f / length);
	else
		sun.toSun = {0.f, 1.f, 0.f};
}

auto RendererPlanet::getSun() -> const PlanetSun& { return frame().sun; }

void RendererPlanet::drawPlanet(const PlanetTerrain& iTerrain, const PlanetDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	auto& data = frame();
	if (!data.open || iParams.settings == nullptr)
		return;
	++data.stats.planets;
	if (iParams.placement.scaledSpace)
		++data.stats.scaled;
	const double scale = iParams.placement.scale;
	const math::vec3d origin = iParams.eye + iParams.placement.center;
	const auto& ramp = iTerrain.getColorRamp();
	const std::array<shared<gpu::Texture2D>, 1> textures{ramp ? ramp : Renderer3D::getWhiteTexture()};
	for (const auto& chunk: iTerrain.getDrawList()) {
		++data.stats.chunks;
		if (!chunk.mesh || !Renderer3D::isReady())
			continue;
		Renderer3D::drawMesh(chunk.mesh, modelOf(origin + chunk.center * scale, scale), textures,
							 {.tint = {1.f, 1.f, 1.f, 1.f}, .entityId = iParams.entityId});
	}
	queueAtmosphere(iParams);
}

void RendererPlanet::endScene() {
	OWL_PROFILE_FUNCTION()

	auto& data = frame();
	if (!data.open)
		return;
	data.open = false;
	if (!g_data || data.atmospheres.empty()) {
		data.stats.atmospheres += static_cast<uint32_t>(data.atmospheres.size());
		data.atmospheres.clear();
		return;
	}
	std::ranges::sort(data.atmospheres, [](const QueuedAtmosphere& iA, const QueuedAtmosphere& iB) -> bool {
		return iA.distance > iB.distance;
	});
	for (const auto& queued: data.atmospheres) {
		const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
		g_data->uniforms->setData(&queued.ubo, static_cast<uint32_t>(sizeof(AtmosphereUbo)), 0);
		g_data->uniforms->bind();
		gpu::RenderCommand::beginBatch();
		gpu::RenderCommand::drawData(g_data->shells[queued.inside ? 1 : 0], g_data->indexCount);
		gpu::RenderCommand::endBatch();
		++data.stats.atmospheres;
	}
	data.atmospheres.clear();
}

auto RendererPlanet::getStats() -> Stats { return frame().stats; }

void RendererPlanet::resetStats() { frame().stats = {}; }

}// namespace owl::renderer
