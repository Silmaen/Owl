/**
 * @file PbrTextures.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "PbrTextures.h"

#include "Buffer.h"
#include "Context.h"
#include "Texture.h"

#include <algorithm>
#include <array>
#include <bit>

namespace owl::renderer::gpu::webgpu {

namespace {

// The shadow budget of the targets other than Vulkan (`MAX_SPOT_SHADOWS`, `MAX_POINT_SHADOWS` of pbr.slangh).
constexpr uint32_t g_spots = 4;
constexpr uint32_t g_points = 4;
constexpr uint32_t g_probes = 4;
constexpr uint32_t g_faces = renderer::gpu::TextureCube::g_faceCount;
constexpr uint32_t g_cascades = 4;
// The 2D slots of the 3D renderer (`Renderer3D.cpp`).
constexpr uint32_t g_environmentSlot = 7;
constexpr uint32_t g_environmentLevels = 6;
constexpr uint32_t g_firstLookupSlot = 13;
constexpr uint32_t g_lookupSlots = 3;
constexpr uint32_t g_cascadeSlot = 16;
constexpr uint32_t g_spotSlot = 20;
// Table slot of the environment, then of the lookup tables.
constexpr uint32_t g_tableEnvironment = 7;
constexpr uint32_t g_tableLookups = 8;

auto toTexture(const std::span<const shared<renderer::gpu::Texture2D>> iTextures, const size_t iSlot)
		-> const Texture2D* {
	return iSlot < iTextures.size() ? dynamic_cast<const Texture2D*>(iTextures[iSlot].get()) : nullptr;
}

auto toCube(const std::span<const shared<renderer::gpu::TextureCube>> iCubes, const size_t iSlot)
		-> const TextureCube* {
	return iSlot < iCubes.size() ? dynamic_cast<const TextureCube*>(iCubes[iSlot].get()) : nullptr;
}

auto levelSize(const uint32_t iSize, const uint32_t iLevel) -> uint32_t { return std::max(iSize >> iLevel, 1U); }

// Copies one layer and level of a texture into a layer of an array, when the format matches and the texels fit.
void copyLayer(const wgpu::CommandEncoder& iEncoder, const wgpu::Texture& iSource, const uint32_t iSourceLayer,
			   const uint32_t iLevel, const wgpu::Texture& iArray, const uint32_t iArrayLayer) {
	if (iSource == nullptr || iSource.GetFormat() != iArray.GetFormat() || iLevel >= iSource.GetMipLevelCount() ||
		iLevel >= iArray.GetMipLevelCount())
		return;
	const uint32_t width = levelSize(iSource.GetWidth(), iLevel);
	const uint32_t height = levelSize(iSource.GetHeight(), iLevel);
	if (width > levelSize(iArray.GetWidth(), iLevel) || height > levelSize(iArray.GetHeight(), iLevel))
		return;
	const wgpu::TexelCopyTextureInfo source{.texture = iSource, .mipLevel = iLevel, .origin = {.z = iSourceLayer}};
	const wgpu::TexelCopyTextureInfo destination{.texture = iArray, .mipLevel = iLevel, .origin = {.z = iArrayLayer}};
	const wgpu::Extent3D extent{.width = width, .height = height, .depthOrArrayLayers = 1};
	iEncoder.CopyTextureToTexture(&source, &destination, &extent);
}

// Share of an array layer a texture of this size covers, and the size: `WebGpuCubeLayers` of pbr.slangh.
auto layerShare(const wgpu::Texture& iTexture, const wgpu::Texture& iArray) -> std::array<float, 4> {
	if (iTexture == nullptr)
		return {1.f, 1.f, 1.f, 0.f};
	const auto size = static_cast<float>(iTexture.GetWidth());
	return {size / static_cast<float>(iArray.GetWidth()),
			static_cast<float>(iTexture.GetHeight()) / static_cast<float>(iArray.GetHeight()), size, 0.f};
}

}// namespace

void PbrTextures::ensure(Array& ioArray, const uint32_t iWidth, const uint32_t iHeight, const uint32_t iLayers,
						 const uint32_t iLevels, const wgpu::TextureFormat iFormat) {
	const auto& texture = ioArray.texture;
	if (texture != nullptr && texture.GetWidth() == iWidth && texture.GetHeight() == iHeight &&
		texture.GetDepthOrArrayLayers() == iLayers && texture.GetMipLevelCount() == iLevels &&
		texture.GetFormat() == iFormat)
		return;
	const wgpu::TextureDescriptor desc{.label = "PBR array",
									   .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
									   .dimension = wgpu::TextureDimension::e2D,
									   .size = {.width = iWidth, .height = iHeight, .depthOrArrayLayers = iLayers},
									   .format = iFormat,
									   .mipLevelCount = iLevels};
	ioArray.texture = Context::get().device.CreateTexture(&desc);
	const wgpu::TextureViewDescriptor viewDesc{.dimension = wgpu::TextureViewDimension::e2DArray};
	ioArray.view = ioArray.texture.CreateView(&viewDesc);
	ioArray.sources.clear();
}

void PbrTextures::packEnvironment(const std::span<const shared<renderer::gpu::Texture2D>> iLevels) {
	std::vector<WGPUTexture> sources;
	for (size_t level = 0; level < iLevels.size(); ++level) {
		const auto* texture = toTexture(iLevels, level);
		sources.push_back(texture != nullptr ? texture->getTexture().Get() : nullptr);
	}
	if (sources == m_environment.sources && m_environment.texture != nullptr)
		return;
	const auto* base = toTexture(iLevels, 0);
	if (base == nullptr || base->getTexture() == nullptr) {
		m_environment = {};
		m_environment.sources = std::move(sources);
		return;
	}
	const auto& first = base->getTexture();
	// The levels that halve the one before: the white of an unused slot ends the chain.
	uint32_t levels = 1;
	const auto maxLevels = static_cast<uint32_t>(std::bit_width(std::max(first.GetWidth(), first.GetHeight())));
	while (levels < iLevels.size() && levels < maxLevels) {
		const auto* texture = toTexture(iLevels, levels);
		if (texture == nullptr || texture->getTexture() == nullptr ||
			texture->getTexture().GetFormat() != first.GetFormat() ||
			texture->getTexture().GetWidth() != levelSize(first.GetWidth(), levels) ||
			texture->getTexture().GetHeight() != levelSize(first.GetHeight(), levels))
			break;
		++levels;
	}
	const wgpu::TextureDescriptor desc{
			.label = "Environment",
			.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
			.dimension = wgpu::TextureDimension::e2D,
			.size = {.width = first.GetWidth(), .height = first.GetHeight(), .depthOrArrayLayers = 1},
			.format = first.GetFormat(),
			.mipLevelCount = levels};
	auto& context = Context::get();
	m_environment.texture = context.device.CreateTexture(&desc);
	m_environment.view = m_environment.texture.CreateView();
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	for (uint32_t level = 0; level < levels; ++level) {
		const auto& source = toTexture(iLevels, level)->getTexture();
		const wgpu::TexelCopyTextureInfo from{.texture = source};
		const wgpu::TexelCopyTextureInfo to{.texture = m_environment.texture, .mipLevel = level};
		const wgpu::Extent3D extent{.width = source.GetWidth(), .height = source.GetHeight(), .depthOrArrayLayers = 1};
		encoder.CopyTextureToTexture(&from, &to, &extent);
	}
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
	m_environment.sources = std::move(sources);
}

void PbrTextures::packArrays(const std::span<const shared<renderer::gpu::Texture2D>> iTextures,
							 const std::span<const shared<renderer::gpu::TextureCube>> iCubes) {
	// Largest layer of each array, its format from its first map.
	uint32_t cascadeWidth = 1;
	uint32_t cascadeHeight = 1;
	for (uint32_t index = 0; index < g_cascades; ++index)
		if (const auto* map = toTexture(iTextures, g_cascadeSlot + index); map && map->getTexture()) {
			cascadeWidth = std::max(cascadeWidth, map->getTexture().GetWidth());
			cascadeHeight = std::max(cascadeHeight, map->getTexture().GetHeight());
		}
	uint32_t localSize = 1;
	for (uint32_t index = 0; index < g_spots; ++index)
		if (const auto* map = toTexture(iTextures, g_spotSlot + index); map && map->getTexture())
			localSize = std::max({localSize, map->getTexture().GetWidth(), map->getTexture().GetHeight()});
	for (uint32_t index = 0; index < g_points; ++index)
		if (const auto* cube = toCube(iCubes, index); cube && cube->getTexture())
			localSize = std::max(localSize, cube->getTexture().GetWidth());
	uint32_t probeSize = 1;
	uint32_t probeLevels = 1;
	for (uint32_t index = 0; index < g_probes; ++index)
		if (const auto* cube = toCube(iCubes, g_points + index); cube && cube->getTexture()) {
			probeSize = std::max(probeSize, cube->getTexture().GetWidth());
			probeLevels = std::max(probeLevels, cube->getTexture().GetMipLevelCount());
		}
	// The shadow maps are `RedFloat` (R32F), the probes `Rgba16F`.
	ensure(m_cascades, cascadeWidth, cascadeHeight, g_cascades, 1, wgpu::TextureFormat::R32Float);
	ensure(m_local, localSize, localSize, g_spots + g_points * g_faces, 1, wgpu::TextureFormat::R32Float);
	ensure(m_probes, probeSize, probeSize, g_probes * g_faces, probeLevels, wgpu::TextureFormat::RGBA16Float);

	auto& context = Context::get();
	const wgpu::CommandEncoder encoder = context.device.CreateCommandEncoder();
	for (uint32_t index = 0; index < g_cascades; ++index)
		if (const auto* map = toTexture(iTextures, g_cascadeSlot + index); map)
			copyLayer(encoder, map->getTexture(), 0, 0, m_cascades.texture, index);
	for (uint32_t index = 0; index < g_spots; ++index)
		if (const auto* map = toTexture(iTextures, g_spotSlot + index); map)
			copyLayer(encoder, map->getTexture(), 0, 0, m_local.texture, index);
	std::array<std::array<float, 4>, size_t{g_points} + g_probes> layers{};
	for (uint32_t index = 0; index < g_points; ++index) {
		const auto* cube = toCube(iCubes, index);
		if (cube == nullptr)
			continue;
		for (uint32_t face = 0; face < g_faces; ++face)
			copyLayer(encoder, cube->getTexture(), face, 0, m_local.texture, g_spots + index * g_faces + face);
		layers[index] = layerShare(cube->getTexture(), m_local.texture);
	}
	for (uint32_t index = 0; index < g_probes; ++index) {
		const auto* cube = toCube(iCubes, g_points + index);
		if (cube == nullptr || cube->getTexture() == nullptr)
			continue;
		for (uint32_t level = 0; level < cube->getTexture().GetMipLevelCount(); ++level)
			for (uint32_t face = 0; face < g_faces; ++face)
				copyLayer(encoder, cube->getTexture(), face, level, m_probes.texture, index * g_faces + face);
		layers[g_points + index] = layerShare(cube->getTexture(), m_probes.texture);
	}
	const wgpu::CommandBuffer commands = encoder.Finish();
	context.queue.Submit(1, &commands);
	if (m_cubeLayers == nullptr)
		m_cubeLayers = makeBuffer(sizeof(layers), wgpu::BufferUsage::Uniform, "Cube layers");
	context.queue.WriteBuffer(m_cubeLayers, 0, layers.data(), sizeof(layers));
}

void PbrTextures::appendEntries(const std::span<const shared<renderer::gpu::Texture2D>> iTextures,
								const std::span<const shared<renderer::gpu::TextureCube>> iCubes, const uint64_t iFrame,
								std::vector<wgpu::BindGroupEntry>& oEntries) {
	auto& context = Context::get();
	if (iTextures.size() > g_environmentSlot)
		packEnvironment(iTextures.subspan(g_environmentSlot,
										  std::min<size_t>(g_environmentLevels, iTextures.size() - g_environmentSlot)));
	// One copy per frame (its maps are drawn before its first 3D draw), again when the renderer binds other maps.
	std::vector<const void*> packed;
	packed.reserve(iTextures.size() + iCubes.size());
	for (const auto& texture: iTextures) packed.push_back(texture.get());
	for (const auto& cube: iCubes) packed.push_back(cube.get());
	if (iFrame != m_frame || packed != m_packed || m_cubeLayers == nullptr) {
		packArrays(iTextures, iCubes);
		m_frame = iFrame;
		m_packed = std::move(packed);
	}
	if (m_probeSampler == nullptr) {
		const wgpu::SamplerDescriptor desc{.label = "Probes",
										   .magFilter = wgpu::FilterMode::Linear,
										   .minFilter = wgpu::FilterMode::Linear,
										   .mipmapFilter = wgpu::MipmapFilterMode::Linear};
		m_probeSampler = context.device.CreateSampler(&desc);
		const wgpu::SamplerDescriptor environmentDesc{.label = "Environment",
													  .addressModeU = wgpu::AddressMode::Repeat,
													  .magFilter = wgpu::FilterMode::Linear,
													  .minFilter = wgpu::FilterMode::Linear,
													  .mipmapFilter = wgpu::MipmapFilterMode::Nearest};
		m_environmentSampler = context.device.CreateSampler(&environmentDesc);
	}
	for (uint32_t slot = 0; slot < g_tableSlots; ++slot) {
		wgpu::TextureView view = context.whiteView;
		wgpu::Sampler sampler = context.defaultSampler;
		const Texture2D* texture = nullptr;
		if (slot < g_tableEnvironment)
			texture = toTexture(iTextures, slot);
		else if (slot >= g_tableLookups && slot < g_tableLookups + g_lookupSlots)
			texture = toTexture(iTextures, g_firstLookupSlot + slot - g_tableLookups);
		if (texture != nullptr && texture->getView() != nullptr) {
			view = texture->getView();
			sampler = texture->getSampler();
		}
		if (slot == g_tableEnvironment && m_environment.view != nullptr) {
			view = m_environment.view;
			sampler = m_environmentSampler;
		}
		oEntries.push_back({.binding = slot, .textureView = view});
		oEntries.push_back({.binding = g_textureSlots + slot, .sampler = sampler});
	}
	oEntries.push_back({.binding = g_cascadeBinding, .textureView = m_cascades.view});
	oEntries.push_back({.binding = g_localBinding, .textureView = m_local.view});
	oEntries.push_back({.binding = g_probeBinding, .textureView = m_probes.view});
	oEntries.push_back({.binding = g_probeSamplerBinding, .sampler = m_probeSampler});
	oEntries.push_back({.binding = g_cubeLayersBinding, .buffer = m_cubeLayers});
}

}// namespace owl::renderer::gpu::webgpu
