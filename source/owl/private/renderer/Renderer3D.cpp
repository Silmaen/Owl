/**
 * @file Renderer3D.cpp
 * @author Silmaen
 * @date 04/06/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Renderer3D.h"

#include "math/linAlgebra.h"
#include "math/matrixCreation.h"
#include "renderer/ClipSpace.h"
#include "renderer/Material.h"

#include "renderer/gpu/Framebuffer.h"
#include "renderer/gpu/RenderCommand.h"
#include "renderer/gpu/RendererDescriptors.h"
#include "renderer/gpu/StorageBuffer.h"
#include "renderer/gpu/UniformBuffer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace owl::renderer {

namespace {
constexpr const char* k_RendererKey = "Renderer3D";
constexpr const char* k_ShaderFolder = "renderer3D";
constexpr uint32_t k_MaxTextureSlots = 32;

// Identifier of this renderer's descriptor block, resolved once.
auto blockId() -> gpu::RendererId {
	static const gpu::RendererId sId = gpu::RendererDescriptors::getId(k_RendererKey);
	return sId;
}

struct SceneUbo {
	math::mat4 viewProjection = math::identity<float, 4>();
	math::mat4 model = math::identity<float, 4>();
	// xyz: direction of the sun of the layer; w: its pre-exposed illuminance, 0 when it does not light.
	math::vec4 sunDirection{-0.4f, -1.0f, -0.6f, 0.f};
	// rgb: editor ambient added to every lit surface; w: exposure of the emissive.
	math::vec4 ambient{0.f, 0.f, 0.f, 0.f};
	math::vec4 tint{1.f, 1.f, 1.f, 1.f};
	// x: entity id, y: linear target, z: shadow pass, w: first visible instance (-1 for a single draw).
	std::array<int32_t, 4> ids{-1, 0, 0, -1};
	// xyz: world position of the eye, origin of the camera-relative space the shaders work in.
	math::vec4 origin{0.f, 0.f, 0.f, 1.f};
};

static_assert(sizeof(SceneUbo) == 208, "SceneUbo must match the std140 layout of Scene3D in the renderer3D shaders.");

struct LightingUbo {
	// x: light count (0 lights with the sun of the layer), y: cascade count, z: index of the shadowed light.
	std::array<int32_t, 4> counts{0, 0, -1, 0};
	math::vec4 splitDepths{0.f, 0.f, 0.f, 0.f};
	math::vec4 texelWorldSizes{0.f, 0.f, 0.f, 0.f};
	math::vec4 viewDepthRow{0.f, 0.f, 0.f, 0.f};
	// x: normal-offset bias in texels, y: shadow-map size in texels.
	math::vec4 shadowParams{1.5f, 1024.f, 0.f, 0.f};
	// x: intensity, y / z: cosine / sine of the rotation about Y, w: number of prefiltered levels (0 none).
	math::vec4 environment{1.f, 1.f, 0.f, 0.f};
	std::array<math::vec4, 9> irradiance{};
	std::array<math::mat4, 4> cascades{};
	// xyz: direction towards the sun (rotated as the environment is looked up), w: angular radius of the discs (0 none).
	math::vec4 skySun{0.f, 1.f, 0.f, 0.f};
	// Pre-exposed radiances: x sun disc, y moon disc, z brightest stars.
	math::vec4 skyBodies{0.f, 0.f, 0.f, 0.f};
};

static_assert(sizeof(LightingUbo) == 528,
			  "LightingUbo must match the std140 layout of Lighting in the renderer3D shaders.");

struct MaterialUbo {
	math::vec4 baseColor{1.f, 1.f, 1.f, 1.f};
	// rgb: emitted colour (sRGB), w: alpha cutoff.
	math::vec4 emissive{0.f, 0.f, 0.f, 0.5f};
	// x metallic, y roughness, z normal scale, w occlusion strength.
	math::vec4 factors{0.f, 0.5f, 1.f, 1.f};
	// x clearcoat, y clearcoat roughness, z anisotropy, w anisotropy rotation.
	math::vec4 layers{0.f, 0.f, 0.f, 0.f};
	// rgb sheen colour (sRGB), w sheen roughness.
	math::vec4 sheen{0.f, 0.f, 0.f, 0.f};
	// x shading model, y alpha mode, z map mask, w toon bands.
	std::array<int32_t, 4> model{0, 0, 0, 3};
	// x toon rim, y emissive strength.
	math::vec4 toon{0.5f, 1.f, 0.f, 0.f};
	std::array<int32_t, 4> channels{2, 1, 0, 0};
};

static_assert(sizeof(MaterialUbo) == 128,
			  "MaterialUbo must match the std140 layout of MaterialData in the renderer3D shaders.");

constexpr uint32_t g_materialSlotEnd = 1 + g_materialMapCount;
constexpr uint32_t g_environmentSlot = 7;
constexpr uint32_t g_dfgSlot = 13;
constexpr uint32_t g_dfgSize = 64;
constexpr uint32_t g_instanceBinding = 5;
constexpr uint32_t g_visibleBinding = 6;

struct InternalData {
	shared<gpu::UniformBuffer> sceneUniformBuffer;
	shared<gpu::UniformBuffer> materialUniformBuffer;
	shared<gpu::Texture2D> dfgTexture;
	Renderer3D::MeshHandle skyMesh;
	EnvironmentLighting environment;
	MaterialUbo material;
	shared<gpu::UniformBuffer> lightingUniformBuffer;
	shared<gpu::StorageBuffer> lightBuffer;
	shared<gpu::Texture2D> whiteTexture;
	SceneUbo scene;
	LightingUbo lighting;
	// World view-projection of the camera of the scene (the uploaded one is camera-relative).
	math::mat4 viewProjection = math::identity<float, 4>();
	// Shadow cascades as the scene gives them (relative to their light camera) and the world view depth row.
	std::array<math::mat4, 4> cascades{};
	std::array<math::vec3, 4> cascadeOrigins{};
	math::vec4 worldViewDepthRow{0.f, 0.f, 0.f, 0.f};
	// Ambient radiance of the layer in nits (`setLighting`), the constant sky when the environment gives none.
	math::vec3 layerAmbient{4200.f, 4200.f, 4800.f};
	std::array<GpuLight, Renderer3D::g_maxLights> lights{};
	// Lights as the scene gives them, before the exposure.
	std::array<GpuLight, Renderer3D::g_maxLights> sceneLights{};
	// Exposure applied to every physical light value (1 / (1.2 * 2^EV100)).
	float exposure = Renderer3D::exposureOf(Renderer3D::g_defaultEv100);
	// Illuminance of the sun of the layer, in lux, and whether it lights a scene without light.
	float layerSunIlluminance = 100000.f;
	bool layerSun = true;
	// Display-referred light added in Edit mode.
	math::vec3 editorAmbient{0.f, 0.f, 0.f};
	std::array<shared<gpu::Texture2D>, 4> shadowMaps{};
	std::array<shared<gpu::Texture2D>, k_MaxTextureSlots> textureSlots;
	bool shadowPass = false;
};

shared<InternalData> g_Data;

void setIrradiance(const SphericalHarmonics& iHarmonics) {
	const auto coefficients = iHarmonics.toShaderCoefficients();
	for (size_t index = 0; index < coefficients.size(); ++index) {
		const auto& c = coefficients[index];
		g_Data->lighting.irradiance[index] = math::vec4{c.x(), c.y(), c.z(), 0.f};
	}
}

// Every physical light value pre-exposed: lights, sun, environment or constant sky, and the exposure of the emissive.
void refreshLighting() {
	auto& data = *g_Data;
	const float exposure = data.exposure;
	const auto& environment = data.environment;
	data.lighting.environment = math::vec4{0.f, std::cos(environment.rotation), std::sin(environment.rotation),
										   static_cast<float>(environment.levelCount)};
	const auto& bodies = environment.bodies;
	const float bodyScale = std::max(environment.intensity, 0.f) * exposure;
	data.lighting.skySun = math::vec4{bodies.sunDirection.x(), bodies.sunDirection.y(), bodies.sunDirection.z(),
									  environment.levelCount > 0 ? bodies.discRadius : 0.f};
	data.lighting.skyBodies = math::vec4{bodies.sunRadiance * bodyScale, bodies.moonRadiance * bodyScale,
										 environment.levelCount > 0 ? bodies.starRadiance * bodyScale : 0.f, 0.f};
	if (environment.levelCount > 0) {
		setIrradiance(environment.irradiance);
		data.lighting.environment.x() = std::max(environment.intensity, 0.f) * exposure;
	} else {
		const math::vec3 sky =
				environment.ambient ? *environment.ambient * std::max(environment.intensity, 0.f) : data.layerAmbient;
		setIrradiance(SphericalHarmonics::constant(sky));
		data.lighting.environment.x() = exposure;
	}
	data.scene.sunDirection.w() = data.layerSun ? data.layerSunIlluminance * exposure : 0.f;
	data.scene.ambient = math::vec4{data.editorAmbient.x(), data.editorAmbient.y(), data.editorAmbient.z(), exposure};
	const auto count = static_cast<size_t>(data.lighting.counts[0]);
	for (size_t index = 0; index < count; ++index) {
		data.lights[index] = data.sceneLights[index];
		data.lights[index].color = data.sceneLights[index].color * exposure;
	}
}

// Slots: 0 white, 1-6 material maps (or the draw's textures), 7-12 environment, 13 DFG, 28-31 shadows.
void bindMeshTextures(const std::span<const shared<gpu::Texture2D>> iTextures, const Material* iMaterial) {
	auto& slots = g_Data->textureSlots;
	std::ranges::fill(slots, g_Data->whiteTexture);
	if (iMaterial != nullptr) {
		for (size_t index = 0; index < g_materialMapCount; ++index) {
			if (const auto& texture = iMaterial->getTexture(static_cast<MaterialMap>(index)); texture)
				slots[1 + index] = texture;
		}
	} else {
		size_t count = 1;
		for (const auto& texture: iTextures) {
			if (texture && count < g_materialSlotEnd)
				slots[count++] = texture;
		}
	}
	if (g_Data->shadowPass) {
		gpu::RenderCommand::bindTextures(std::span{slots}.first(g_materialSlotEnd));
		return;
	}
	if (g_Data->dfgTexture)
		slots[g_dfgSlot] = g_Data->dfgTexture;
	for (uint32_t level = 0; level < g_Data->environment.levelCount; ++level) {
		if (const auto& texture = g_Data->environment.levels[level]; texture)
			slots[g_environmentSlot + level] = texture;
	}
	for (size_t index = 0; index < g_Data->shadowMaps.size(); ++index) {
		if (const auto& map = g_Data->shadowMaps[index]; map)
			slots[Renderer3D::g_shadowMapSlot + index] = map;
	}
	gpu::RenderCommand::bindTextures(slots);
}

void setMaterial(const Material* iMaterial, const bool iHasBaseTexture, const bool iBlended) {
	static const Material s_default;
	const Material& material = iMaterial != nullptr ? *iMaterial : s_default;
	const auto& p = material.getParameters();
	auto& ubo = g_Data->material;
	ubo.baseColor = p.baseColor;
	ubo.emissive = math::vec4{p.emissive.x(), p.emissive.y(), p.emissive.z(), p.alphaCutoff};
	ubo.factors = math::vec4{p.metallic, p.roughness, p.normalScale, p.occlusionStrength};
	ubo.layers = math::vec4{p.clearcoat, p.clearcoatRoughness, p.anisotropy, p.anisotropyRotation};
	ubo.sheen = math::vec4{p.sheenColor.x(), p.sheenColor.y(), p.sheenColor.z(), p.sheenRoughness};
	int32_t mask = 0;
	for (size_t index = 0; index < g_materialMapCount; ++index) {
		if (material.getTexture(static_cast<MaterialMap>(index)))
			mask |= 1 << index;
	}
	// Without a material the first texture of the draw is the base colour (voxel atlases pick theirs per vertex).
	if (iMaterial == nullptr && iHasBaseTexture)
		mask |= 1;
	const auto alphaMode = iMaterial == nullptr && iBlended ? AlphaMode::Blend : p.alphaMode;
	ubo.model = {static_cast<int32_t>(p.shadingModel), static_cast<int32_t>(alphaMode), mask,
				 static_cast<int32_t>(std::max(p.toonBands, 1u))};
	ubo.toon = math::vec4{p.toonRim, std::max(p.emissiveLuminance, 0.f), std::max(p.ior, 1.f),
						  std::max(p.clearcoatIor, 1.f)};
	ubo.channels = {static_cast<int32_t>(p.metallicChannel), static_cast<int32_t>(p.roughnessChannel),
					static_cast<int32_t>(p.occlusionChannel), static_cast<int32_t>(p.normalConvention)};
	g_Data->materialUniformBuffer->setData(&ubo, sizeof(MaterialUbo), 0);
}

void setDrawParams(const math::mat4& iModel, const MeshDrawParams& iParams, const bool iHasBaseTexture,
				   const int32_t iVisibleBase = -1) {
	g_Data->scene.ids[3] = iVisibleBase;
	g_Data->scene.model = iModel;
	g_Data->scene.tint = iParams.tint;
	g_Data->scene.ids[0] = iParams.entityId;
	g_Data->scene.ids[2] = g_Data->shadowPass ? 1 : 0;
	g_Data->lightBuffer->bind();
	g_Data->scene.ids[1] = gpu::Framebuffer::isLinearTargetBound() ? 1 : 0;
	g_Data->sceneUniformBuffer->setData(&g_Data->scene, sizeof(SceneUbo), 0);
	setMaterial(iParams.material, iHasBaseTexture, iParams.blended);
}
}// namespace

static_assert(sizeof(Mesh3DVertex) == 56, "Mesh3DVertex must stay tightly packed for direct VBO upload.");
static_assert(sizeof(MeshInstance) == 96, "MeshInstance must match the std430 layout of MeshInstance in mesh3d.");

void Renderer3D::init() {
	OWL_PROFILE_FUNCTION()

	if (g_Data) {
		OWL_CORE_WARN("Renderer3D already initiated.")
		g_Data.reset();
	}
	g_Data = mkShared<InternalData>();
	refreshLighting();

	const std::array<gpu::BindingDecl, 7> bindings{
			gpu::BindingDecl{.binding = 0,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::VertexFragment},
			gpu::BindingDecl{.binding = 1,
							 .type = gpu::BindingType::CombinedImageSampler,
							 .count = k_MaxTextureSlots,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 2,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 3,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 4,
							 .type = gpu::BindingType::UniformBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Fragment},
			gpu::BindingDecl{.binding = 5,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Vertex},
			gpu::BindingDecl{.binding = 6,
							 .type = gpu::BindingType::StorageBuffer,
							 .count = 1,
							 .stages = gpu::ShaderStage::Vertex},
	};
	gpu::RendererDescriptors::declare(k_RendererKey, bindings);
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	g_Data->whiteTexture =
			gpu::Texture2D::create(gpu::Texture2D::Specification{.size = {1, 1}, .format = gpu::ImageFormat::Rgba8});
	uint32_t whiteData = 0xffffffff;
	g_Data->whiteTexture->setData(&whiteData, sizeof(uint32_t));

	g_Data->sceneUniformBuffer = gpu::UniformBuffer::create(sizeof(SceneUbo), 0, k_RendererKey);
	g_Data->sceneUniformBuffer->bind();
	g_Data->lightingUniformBuffer = gpu::UniformBuffer::create(sizeof(LightingUbo), 3, k_RendererKey);
	g_Data->lightingUniformBuffer->bind();
	g_Data->materialUniformBuffer = gpu::UniformBuffer::create(sizeof(MaterialUbo), 4, k_RendererKey);
	g_Data->materialUniformBuffer->bind();
	{
		const auto table = computeDfgTable(g_dfgSize, 128);
		std::vector<float> pixels;
		pixels.reserve(table.size() * 4);
		for (const auto& value: table) pixels.insert(pixels.end(), {value.x(), value.y(), value.z(), 1.f});
		g_Data->dfgTexture = gpu::Texture2D::create(
				gpu::Texture2D::Specification{.size = {g_dfgSize, g_dfgSize}, .format = gpu::ImageFormat::Rgba32F});
		g_Data->dfgTexture->setData(pixels.data(), static_cast<uint32_t>(pixels.size() * sizeof(float)));
	}
	setIrradiance(SphericalHarmonics::constant(
			{g_Data->scene.ambient.x(), g_Data->scene.ambient.y(), g_Data->scene.ambient.z()}));
	{
		std::vector<uint32_t> quad{0, 1, 2, 2, 3, 0};
		g_Data->skyMesh = gpu::DrawData::create();
		g_Data->skyMesh->init({{"i_Position", gpu::ShaderDataType::Float3}}, k_ShaderFolder, quad, "sky",
							  {.blendMode = gpu::BlendMode::Opaque, .depthTest = false, .depthWrite = false});
		constexpr std::array<float, 12> corners{-1.f, -1.f, 0.f, 1.f, -1.f, 0.f, 1.f, 1.f, 0.f, -1.f, 1.f, 0.f};
		g_Data->skyMesh->setVertexData(corners.data(), static_cast<uint32_t>(corners.size() * sizeof(float)));
	}
	g_Data->lightBuffer = gpu::StorageBuffer::create(g_maxLights * sizeof(GpuLight), 2, k_RendererKey);
	g_Data->lightBuffer->setData(g_Data->lights.data(), static_cast<uint32_t>(sizeof(GpuLight)), 0);
}

void Renderer3D::shutdown() {
	OWL_PROFILE_FUNCTION()

	if (!g_Data) {
		OWL_CORE_WARN("Renderer3D already shutdown.")
		return;
	}
	g_Data->sceneUniformBuffer.reset();
	g_Data->lightingUniformBuffer.reset();
	g_Data->materialUniformBuffer.reset();
	g_Data->dfgTexture.reset();
	g_Data->skyMesh.reset();
	g_Data->environment = {};
	g_Data->lightBuffer.reset();
	g_Data->shadowMaps = {};
	g_Data->textureSlots = {};
	g_Data->whiteTexture.reset();
	g_Data.reset();
	gpu::RendererDescriptors::release(k_RendererKey);
}

void Renderer3D::beginScene(const Camera& iCamera) {
	OWL_PROFILE_FUNCTION()

	gpu::RenderCommand::beginBatch();
	auto& data = *g_Data;
	data.viewProjection = iCamera.getViewProjection();
	const math::vec4 eye = inverse(iCamera.getView()) * math::vec4{0.f, 0.f, 0.f, 1.f};
	const math::vec3 origin{eye.x(), eye.y(), eye.z()};
	data.scene.origin = math::vec4{origin.x(), origin.y(), origin.z(), 1.f};
	data.scene.viewProjection = iCamera.getProjection() * cameraRelativeView(iCamera.getView());
	if (data.shadowPass)
		return;
	// Only the small offset between the two eyes enters a matrix: a far-away scene keeps precise shadows.
	for (size_t index = 0; index < data.cascades.size(); ++index)
		data.lighting.cascades[index] =
				data.cascades[index] * math::translate(math::identity<float, 4>(), origin - data.cascadeOrigins[index]);
	const auto& row = data.worldViewDepthRow;
	data.lighting.viewDepthRow = math::vec4{
			row.x(), row.y(), row.z(), row.w() + row.x() * origin.x() + row.y() * origin.y() + row.z() * origin.z()};
	const auto lightCount = static_cast<size_t>(std::max(data.lighting.counts[0], 0));
	for (size_t index = 0; index < lightCount; ++index) {
		const auto& world = data.sceneLights[index].positionRange;
		data.lights[index].positionRange =
				math::vec4{world.x() - origin.x(), world.y() - origin.y(), world.z() - origin.z(), world.w()};
	}
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	data.lightingUniformBuffer->setData(&data.lighting, sizeof(LightingUbo), 0);
	const auto uploadCount = static_cast<uint32_t>(std::max<size_t>(lightCount, 1));
	data.lightBuffer->setData(data.lights.data(), uploadCount * static_cast<uint32_t>(sizeof(GpuLight)), 0);
}

void Renderer3D::endScene() {}

void Renderer3D::setLighting(const math::vec3& iSunDirection, const float iSunIlluminance, const math::vec3& iAmbient) {
	g_Data->scene.sunDirection = math::vec4{iSunDirection.x(), iSunDirection.y(), iSunDirection.z(), 0.f};
	g_Data->layerSunIlluminance = std::max(iSunIlluminance, 0.f);
	g_Data->layerAmbient = iAmbient;
	refreshLighting();
}

void Renderer3D::setLayerSun(const bool iEnabled) {
	if (!g_Data)
		return;
	g_Data->layerSun = iEnabled;
	refreshLighting();
}

auto Renderer3D::isLayerSunEnabled() -> bool { return !g_Data || g_Data->layerSun; }

void Renderer3D::setEditorAmbient(const math::vec3& iRadiance) {
	if (!g_Data)
		return;
	g_Data->editorAmbient = iRadiance;
	refreshLighting();
}

void Renderer3D::setExposure(const float iExposure) {
	if (!g_Data)
		return;
	g_Data->exposure = std::max(iExposure, 0.f);
	refreshLighting();
}

auto Renderer3D::getExposure() -> float { return g_Data ? g_Data->exposure : exposureOf(g_defaultEv100); }

auto Renderer3D::getSurfaceLight(const math::vec3& iNormal) -> math::vec3 {
	if (!g_Data)
		return math::vec3{1.f, 1.f, 1.f};
	const auto& data = *g_Data;
	const auto& environment = data.environment;
	math::vec3 total{0.f, 0.f, 0.f};
	if (environment.levelCount > 0) {
		total = environment.irradiance.evaluate(iNormal) * std::max(environment.intensity, 0.f);
	} else {
		total = environment.ambient ? *environment.ambient * std::max(environment.intensity, 0.f) : data.layerAmbient;
	}
	const auto lambert = [&iNormal](const math::vec3& iTravel) -> float {
		const float length = iTravel.norm();
		return length > 0.f ? std::max(-(iNormal * iTravel) / length, 0.f) / std::numbers::pi_v<float> : 0.f;
	};
	const auto count = static_cast<size_t>(data.lighting.counts[0]);
	for (size_t index = 0; index < count; ++index) {
		const auto& light = data.sceneLights[index];
		if (static_cast<int>(light.directionType.w()) != 0)
			continue;
		const math::vec3 travel{light.directionType.x(), light.directionType.y(), light.directionType.z()};
		total += math::vec3{light.color.x(), light.color.y(), light.color.z()} * lambert(travel);
	}
	if (data.layerSun) {
		const math::vec3 travel{data.scene.sunDirection.x(), data.scene.sunDirection.y(), data.scene.sunDirection.z()};
		total += math::vec3{1.f, 1.f, 1.f} * (data.layerSunIlluminance * lambert(travel));
	}
	return total * data.exposure;
}

auto Renderer3D::exposureOf(const float iEv100) -> float { return 1.f / (1.2f * std::exp2(iEv100)); }

void Renderer3D::drawSky() {
	if (!g_Data || g_Data->environment.levelCount == 0 || !g_Data->skyMesh)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	g_Data->lightingUniformBuffer->setData(&g_Data->lighting, sizeof(LightingUbo), 0);
	drawMesh(g_Data->skyMesh, inverse(g_Data->scene.viewProjection), {}, {});
}

void Renderer3D::setEnvironment(const EnvironmentLighting& iEnvironment) {
	if (!g_Data)
		return;
	g_Data->environment = iEnvironment;
	g_Data->environment.levelCount = std::min<uint32_t>(iEnvironment.levelCount, 6);
	refreshLighting();
}

void Renderer3D::setSceneLights(const std::span<const GpuLight> iLights) {
	if (!g_Data)
		return;
	const auto count = std::min(iLights.size(), static_cast<size_t>(g_maxLights));
	std::copy_n(iLights.begin(), count, g_Data->sceneLights.begin());
	g_Data->lighting.counts[0] = static_cast<int32_t>(count);
	refreshLighting();
}

void Renderer3D::setShadow(const ShadowSettings& iShadow) {
	if (!g_Data)
		return;
	auto& lighting = g_Data->lighting;
	const uint32_t count = std::min(iShadow.cascadeCount, static_cast<uint32_t>(lighting.cascades.size()));
	lighting.counts[1] = static_cast<int32_t>(count);
	lighting.counts[2] = count > 0 ? iShadow.lightIndex : -1;
	lighting.splitDepths = iShadow.splitDepths;
	lighting.texelWorldSizes = iShadow.texelWorldSizes;
	g_Data->worldViewDepthRow = iShadow.viewDepthRow;
	lighting.shadowParams = {iShadow.bias, static_cast<float>(iShadow.resolution), 0.f, 0.f};
	g_Data->cascades = iShadow.relativeViewProjections;
	g_Data->cascadeOrigins = iShadow.origins;
	g_Data->shadowMaps = iShadow.maps;
}

void Renderer3D::setShadowPass(const bool iShadowPass) {
	if (g_Data)
		g_Data->shadowPass = iShadowPass;
}

auto Renderer3D::isShadowPass() -> bool { return g_Data && g_Data->shadowPass; }

auto Renderer3D::getViewProjection() -> math::mat4 {
	return g_Data ? g_Data->viewProjection : math::identity<float, 4>();
}

auto Renderer3D::getWhiteTexture() -> shared<gpu::Texture2D> { return g_Data ? g_Data->whiteTexture : nullptr; }

auto Renderer3D::createMesh(std::span<const Mesh3DVertex> iVertices, std::span<const uint32_t> iIndices,
							const std::string& iShaderName, const gpu::PipelineState& iState) -> MeshHandle {
	auto draw = gpu::DrawData::create();
	std::vector<uint32_t> indices{iIndices.begin(), iIndices.end()};
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	draw->init({{"i_Position", gpu::ShaderDataType::Float3},
				{"i_Normal", gpu::ShaderDataType::Float3},
				{"i_Uv", gpu::ShaderDataType::Float2},
				{"i_TexIndex", gpu::ShaderDataType::Int},
				{"i_TileRect", gpu::ShaderDataType::Float4},
				{"i_Ao", gpu::ShaderDataType::Float}},
			   k_ShaderFolder, indices, iShaderName, iState);
	draw->setVertexData(iVertices.data(), static_cast<uint32_t>(iVertices.size() * sizeof(Mesh3DVertex)));
	return draw;
}

void Renderer3D::drawMesh(const MeshHandle& iMesh, const math::mat4& iModel,
						  std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (!iMesh || iMesh->getIndexCount() == 0)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	setDrawParams(iModel, iParams, !iTextures.empty() && iTextures.front());
	bindMeshTextures(iTextures, iParams.material);
	gpu::RenderCommand::drawData(iMesh, iMesh->getIndexCount());
}

void Renderer3D::drawMeshes(std::span<const MeshHandle> iMeshes, const math::mat4& iModel,
							std::span<const shared<gpu::Texture2D>> iTextures, const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (iMeshes.empty())
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};

	setDrawParams(iModel, iParams, !iTextures.empty() && iTextures.front());
	bindMeshTextures(iTextures, iParams.material);
	for (const auto& mesh: iMeshes) {
		if (mesh && mesh->getIndexCount() > 0)
			gpu::RenderCommand::drawData(mesh, mesh->getIndexCount());
	}
}

namespace {
void bindInstances(const MeshInstanceBuffers& iBuffers, const std::span<const shared<gpu::Texture2D>> iTextures,
				   const MeshDrawParams& iParams) {
	MeshDrawParams params = iParams;
	params.tint = math::vec4{1.f, 1.f, 1.f, 1.f};
	params.entityId = -1;
	setDrawParams(math::identity<float, 4>(), params, !iTextures.empty() && iTextures.front(),
				  static_cast<int32_t>(iBuffers.visibleBase));
	iBuffers.instances->bind(g_instanceBinding);
	iBuffers.visible->bind(g_visibleBinding);
	bindMeshTextures(iTextures, iParams.material);
}
}// namespace

void Renderer3D::drawMeshInstanced(const MeshHandle& iMesh, const MeshInstanceBuffers& iBuffers,
								   const uint32_t iInstanceCount,
								   const std::span<const shared<gpu::Texture2D>> iTextures,
								   const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (!iMesh || iMesh->getIndexCount() == 0 || iInstanceCount == 0 || !iBuffers.instances || !iBuffers.visible)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	bindInstances(iBuffers, iTextures, iParams);
	gpu::RenderCommand::drawDataInstanced(iMesh, iMesh->getIndexCount(), iInstanceCount);
}

void Renderer3D::drawMeshIndirect(const MeshHandle& iMesh, const MeshInstanceBuffers& iBuffers,
								  const shared<gpu::StorageBuffer>& iCommands, const uint32_t iCommandIndex,
								  const std::span<const shared<gpu::Texture2D>> iTextures,
								  const MeshDrawParams& iParams) {
	OWL_PROFILE_FUNCTION()

	if (!iMesh || iMesh->getIndexCount() == 0 || !iCommands || !iBuffers.instances || !iBuffers.visible)
		return;
	const gpu::RendererDescriptors::ScopedActive scoped{blockId()};
	bindInstances(iBuffers, iTextures, iParams);
	gpu::RenderCommand::drawIndexedIndirectAt(iMesh, iCommands, iCommandIndex);
}

}// namespace owl::renderer
