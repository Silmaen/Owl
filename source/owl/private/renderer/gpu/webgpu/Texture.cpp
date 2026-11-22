/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 19/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include "Context.h"
#include "renderer/TextureDecoder.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <utility>
#include <vector>

namespace owl::renderer::gpu::webgpu {

namespace {

auto nextId() -> uint64_t {
	static std::atomic<uint64_t> s_next{1};
	return s_next++;
}

auto toFormat(const ImageFormat iFormat) -> wgpu::TextureFormat {
	switch (iFormat) {
		case ImageFormat::R8:
			return wgpu::TextureFormat::R8Unorm;
		case ImageFormat::Rgba32F:
			return wgpu::TextureFormat::RGBA32Float;
		case ImageFormat::R32F:
			return wgpu::TextureFormat::R32Float;
		case ImageFormat::Rgba16F:
			return wgpu::TextureFormat::RGBA16Float;
		case ImageFormat::None:
		case ImageFormat::Rgb8:// widened to Rgba8 on upload: WebGPU has no 3-channel format
		case ImageFormat::Rgba8:
		case ImageFormat::Bc7:// transcoded to Rgba8: the page asks for no compression feature
		case ImageFormat::Astc4x4:
			return wgpu::TextureFormat::RGBA8Unorm;
	}
	return wgpu::TextureFormat::RGBA8Unorm;
}

auto bytesPerTexel(const ImageFormat iFormat) -> uint32_t {
	switch (iFormat) {
		case ImageFormat::R8:
			return 1;
		case ImageFormat::Rgba32F:
			return 16;
		case ImageFormat::R32F:
			return 4;
		case ImageFormat::Rgba16F:
			return 8;
		case ImageFormat::None:
		case ImageFormat::Rgb8:
		case ImageFormat::Rgba8:
		case ImageFormat::Bc7:
		case ImageFormat::Astc4x4:
			return 4;
	}
	return 4;
}

// RGB pixels as RGBA, opaque.
auto widenRgb(const uint8_t* iPixels, const size_t iTexels) -> std::vector<uint8_t> {
	std::vector<uint8_t> rgba(iTexels * 4);
	for (size_t i = 0; i < iTexels; ++i) {
		rgba[i * 4] = iPixels[i * 3];
		rgba[i * 4 + 1] = iPixels[i * 3 + 1];
		rgba[i * 4 + 2] = iPixels[i * 3 + 2];
		rgba[i * 4 + 3] = 255;
	}
	return rgba;
}

// Writes one level of the layers of a texture, the layers one after the other in the data.
void writeLayers(const wgpu::Texture& iTexture, const uint32_t iLevel, const math::vec2ui iSize, const uint32_t iLayers,
				 const ImageFormat iFormat, const void* iData, const size_t iBytes) {
	const auto texels = size_t{iSize.x()} * iSize.y() * iLayers;
	std::vector<uint8_t> widened;
	const void* data = iData;
	size_t bytes = iBytes;
	if (iFormat == ImageFormat::Rgb8) {
		widened = widenRgb(static_cast<const uint8_t*>(iData), std::min(texels, iBytes / 3));
		data = widened.data();
		bytes = widened.size();
	}
	const wgpu::TexelCopyTextureInfo destination{.texture = iTexture, .mipLevel = iLevel};
	const wgpu::TexelCopyBufferLayout layout{.offset = 0,
											 .bytesPerRow = iSize.x() * bytesPerTexel(iFormat),
											 .rowsPerImage = iSize.y()};
	const wgpu::Extent3D extent{.width = iSize.x(), .height = iSize.y(), .depthOrArrayLayers = iLayers};
	Context::get().queue.WriteTexture(&destination, data, bytes, &layout, &extent);
}

void writeLevel(const wgpu::Texture& iTexture, const uint32_t iLevel, const math::vec2ui iSize,
				const ImageFormat iFormat, const void* iData, const size_t iBytes) {
	writeLayers(iTexture, iLevel, iSize, 1, iFormat, iData, iBytes);
}

}// namespace

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::gpu::Texture2D{std::move(iPath)}, m_id{nextId()} {
	auto decoded = decodeImageFile(m_path, 4);
	if (!decoded.valid) {
		OWL_CORE_WARN("WebGPU: Cannot load the texture '{}'.", m_path.string())
		m_specification.size = {0, 0};
		return;
	}
	m_specification.size = decoded.size;
	m_specification.format = ImageFormat::Rgba8;
	create();
	writeLevel(m_texture, 0, decoded.size, ImageFormat::Rgba8, decoded.pixels.data(), decoded.pixels.size());
}

Texture2D::Texture2D(const Specification& iSpecs) : renderer::gpu::Texture2D{iSpecs}, m_id{nextId()} { create(); }

Texture2D::Texture2D(wgpu::Texture iTexture, const Specification& iSpecs)
	: renderer::gpu::Texture2D{iSpecs}, m_texture{std::move(iTexture)}, m_id{nextId()} {
	m_view = m_texture.CreateView();
	createSampler();
}

void Texture2D::create() {
	if (m_specification.size.surface() == 0)
		return;
	const uint32_t levels = std::max(m_specification.levelCount, 1U);
	const wgpu::TextureDescriptor desc{
			.label = "Texture",
			// Copied from by the 3D layout (environment levels): `PbrTextures`.
			.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc |
					 wgpu::TextureUsage::RenderAttachment,
			.dimension = wgpu::TextureDimension::e2D,
			.size = {.width = m_specification.size.x(), .height = m_specification.size.y(), .depthOrArrayLayers = 1},
			.format = toFormat(m_specification.format),
			.mipLevelCount = levels};
	m_texture = Context::get().device.CreateTexture(&desc);
	m_view = m_texture.CreateView();
	createSampler();
}

void Texture2D::createSampler() {
	const bool nearest = m_specification.filterMode != FilterMode::Linear;
	const bool floatTexture =
			m_specification.format == ImageFormat::Rgba32F || m_specification.format == ImageFormat::R32F;
	// A 32-bit float texture is not filterable without a feature the page does not ask for.
	const auto filter = (nearest || floatTexture) ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear;
	const wgpu::SamplerDescriptor desc{.label = "Sampler",
									   .addressModeU = wgpu::AddressMode::Repeat,
									   .addressModeV = wgpu::AddressMode::Repeat,
									   .magFilter = filter,
									   .minFilter = floatTexture ? wgpu::FilterMode::Nearest : wgpu::FilterMode::Linear,
									   .mipmapFilter = floatTexture ? wgpu::MipmapFilterMode::Nearest
																	: wgpu::MipmapFilterMode::Linear};
	m_sampler = Context::get().device.CreateSampler(&desc);
}

void Texture2D::bind(const uint32_t iSlot) const {
	auto& context = Context::get();
	if (iSlot >= context.textureViews.size() || m_view == nullptr)
		return;
	context.textureViews[iSlot] = m_view;
	context.samplers[iSlot] = m_sampler;
}

void Texture2D::setData(void* iData, const uint32_t iSize) {
	if (m_texture == nullptr)
		return;
	writeLevel(m_texture, 0, m_specification.size, m_specification.format, iData, iSize);
}

auto Texture2D::setLevels(const TextureLevels& iLevels) -> bool {
	if (iLevels.levels.empty()) {
		OWL_CORE_WARN("WebGPU: No level given to set on the texture.")
		return false;
	}
	m_specification.size = iLevels.size;
	m_specification.format = iLevels.format;
	m_specification.levelCount = static_cast<uint32_t>(iLevels.levels.size());
	create();
	math::vec2ui size = iLevels.size;
	for (uint32_t level = 0; level < m_specification.levelCount; ++level) {
		writeLevel(m_texture, level, size, iLevels.format, iLevels.levels[level].data(), iLevels.levels[level].size());
		size = {std::max(size.x() / 2, 1U), std::max(size.y() / 2, 1U)};
	}
	return true;
}

void Texture2D::setFilterMode(const FilterMode iMode) {
	m_specification.filterMode = iMode;
	if (m_texture != nullptr)
		createSampler();
}

void Texture2D::reallocate(const Specification& iSpecs) {
	m_specification = iSpecs;
	create();
}

TextureCube::TextureCube(const Specification& iSpecs) : renderer::gpu::TextureCube{iSpecs}, m_id{nextId()} {
	const uint32_t face = m_specification.size.x();
	if (face == 0)
		return;
	// Copied from into the shadow and probe arrays of the 3D layout: `PbrTextures`.
	const wgpu::TextureDescriptor desc{.label = "Cube",
									   .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst |
												wgpu::TextureUsage::CopySrc,
									   .dimension = wgpu::TextureDimension::e2D,
									   .size = {.width = face, .height = face, .depthOrArrayLayers = g_faceCount},
									   .format = toFormat(m_specification.format),
									   .mipLevelCount = getLevelCount()};
	m_texture = Context::get().device.CreateTexture(&desc);
	const wgpu::TextureViewDescriptor viewDesc{.dimension = wgpu::TextureViewDimension::Cube};
	m_view = m_texture.CreateView(&viewDesc);
	// A new WebGPU texture reads as zeros: the faces start black, as on the other backends.
}

void TextureCube::bind(const uint32_t iSlot) const {
	OWL_CORE_WARN_ONCE("WebGPU: Cannot bind a cube to the 2D slot {}, bind it with the cubes.", iSlot)
}

void TextureCube::setData(void* iData, const uint32_t iSize) {
	if (iSize != m_specification.size.surface() * m_specification.getPixelSize() * g_faceCount) {
		OWL_CORE_WARN("WebGPU: TextureCube data of {} bytes does not hold the six faces.", iSize)
		return;
	}
	writeLayers(m_texture, 0, m_specification.size, g_faceCount, m_specification.format, iData, iSize);
}

auto TextureCube::setLevels(const TextureLevels& iLevels) -> bool {
	if (iLevels.size != m_specification.size || iLevels.format != m_specification.format ||
		iLevels.levels.size() != getLevelCount()) {
		OWL_CORE_WARN("WebGPU: TextureCube levels do not match the cube.")
		return false;
	}
	for (uint32_t level = 0; level < getLevelCount(); ++level) {
		const uint32_t face = getFaceSize(level);
		if (iLevels.levels[level].size() !=
			static_cast<size_t>(face) * face * m_specification.getPixelSize() * g_faceCount) {
			OWL_CORE_WARN("WebGPU: TextureCube level {} does not hold the six faces.", level)
			return false;
		}
		writeLayers(m_texture, level, {face, face}, g_faceCount, m_specification.format, iLevels.levels[level].data(),
					iLevels.levels[level].size());
	}
	return true;
}

}// namespace owl::renderer::gpu::webgpu
