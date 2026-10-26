/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include "BindingTable.h"
#include "core/external/opengl46.h"
#include "renderer/TextureDecoder.h"

#include <algorithm>

namespace owl::renderer::gpu::opengl {

namespace {
auto glDataFormat(const ImageFormat& iFormat) -> GLenum {
	switch (iFormat) {
		case ImageFormat::Rgba8:
			return GL_RGBA;
		case ImageFormat::Rgb8:
			return GL_RGB;
		case ImageFormat::R8:
			return GL_RED;
		case ImageFormat::Rgba32F:
			return GL_RGBA;
		case ImageFormat::Bc7:
		case ImageFormat::Astc4x4:
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}

auto glDataType(const ImageFormat& iFormat) -> GLenum {
	return iFormat == ImageFormat::Rgba32F ? GL_FLOAT : GL_UNSIGNED_BYTE;
}

auto glInternalDataFormat(const ImageFormat& iFormat) -> GLenum {
	switch (iFormat) {
		case ImageFormat::Rgba8:
			return GL_RGBA8;
		case ImageFormat::Rgb8:
			return GL_RGB8;
		case ImageFormat::R8:
			return GL_R8;
		case ImageFormat::Rgba32F:
			return GL_RGBA32F;
		case ImageFormat::Bc7:
			return GL_COMPRESSED_RGBA_BPTC_UNORM;
		case ImageFormat::Astc4x4:
			return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}

void applySamplerFilter(const GLuint iTexture, const FilterMode iMode, const uint32_t iMipLevels) {
	if (iMode == FilterMode::Nearest) {
		glTextureParameteri(iTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(iTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTextureParameteri(iTexture, GL_TEXTURE_MIN_FILTER, iMipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(iTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glTextureParameteri(iTexture, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(iTexture, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

}// namespace

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::gpu::Texture2D{std::move(iPath)} {

	OWL_PROFILE_FUNCTION()

	const auto decoded = decodeImageFile(m_path);
	if (!decoded.valid) {
		return;
	}
	m_specification.format = decoded.format;
	m_specification.size = decoded.size;

	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);

	m_mipLevels = m_specification.getMipLevelCount();
	glTextureStorage2D(m_textureId, static_cast<GLsizei>(m_mipLevels), glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	applySamplerFilter(m_textureId, m_specification.filterMode, m_mipLevels);

	setData(const_cast<uint8_t*>(decoded.pixels.data()), static_cast<uint32_t>(decoded.pixels.size()));
}

Texture2D::Texture2D(const Specification& iSpecs)
	: renderer::gpu::Texture2D{iSpecs}, m_mipLevels{iSpecs.getMipLevelCount()} {

	OWL_PROFILE_FUNCTION()

	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);

	glTextureStorage2D(m_textureId, static_cast<GLsizei>(m_mipLevels), glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	applySamplerFilter(m_textureId, m_specification.filterMode, m_mipLevels);
}

Texture2D::Texture2D(const uint32_t iTextureId, const Specification& iSpecs)
	: renderer::gpu::Texture2D{iSpecs}, m_textureId{iTextureId}, m_borrowed{true} {}

void Texture2D::setBorrowed(const uint32_t iTextureId, const math::vec2ui& iSize) {
	BindingTable::forgetTexture(m_textureId);
	m_textureId = iTextureId;
	m_specification.size = iSize;
}

Texture2D::~Texture2D() {
	OWL_PROFILE_FUNCTION()

	BindingTable::forgetTexture(m_textureId);
	if (!m_borrowed)
		glDeleteTextures(1, &m_textureId);
}

void Texture2D::bind(const uint32_t iSlot) const {
	OWL_PROFILE_FUNCTION()

	glBindTextureUnit(iSlot, m_textureId);
	BindingTable::recordTexture(iSlot, m_textureId);
}

void Texture2D::setFilterMode(const FilterMode iMode) {
	OWL_PROFILE_FUNCTION()

	m_specification.filterMode = iMode;
	if (m_textureId != 0)
		applySamplerFilter(m_textureId, iMode, m_mipLevels);
}

void Texture2D::reallocate(const Specification& iSpecs) {
	OWL_PROFILE_FUNCTION()

	BindingTable::forgetTexture(m_textureId);
	glDeleteTextures(1, &m_textureId);
	m_specification = iSpecs;
	m_mipLevels = m_specification.getMipLevelCount();
	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);
	glTextureStorage2D(m_textureId, static_cast<GLsizei>(m_mipLevels), glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));
	applySamplerFilter(m_textureId, m_specification.filterMode, m_mipLevels);
}

void Texture2D::setData(void* iData, [[maybe_unused]] const uint32_t iSize) {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(iSize == m_specification.size.surface() * m_specification.getPixelSize(),
					"Data size mismatch texture size!")
	glTextureSubImage2D(m_textureId, 0, 0, 0, static_cast<GLsizei>(m_specification.size.x()),
						static_cast<GLsizei>(m_specification.size.y()), glDataFormat(m_specification.format),
						glDataType(m_specification.format), iData);
	if (m_mipLevels > 1)
		glGenerateTextureMipmap(m_textureId);
}

auto Texture2D::setLevels(const TextureLevels& iLevels) -> bool {
	OWL_PROFILE_FUNCTION()

	if (iLevels.levels.empty() || iLevels.size.surface() == 0) {
		OWL_CORE_WARN("OpenGL Texture: No level to set on '{}'.", getName())
		return false;
	}
	auto specs = m_specification;
	specs.size = iLevels.size;
	specs.format = iLevels.format;
	specs.levelCount = static_cast<uint32_t>(iLevels.levels.size());
	reallocate(specs);
	const bool compressed = isBlockCompressed(iLevels.format);
	for (uint32_t level = 0; level < specs.levelCount; ++level) {
		const auto width = static_cast<GLsizei>(std::max(1u, iLevels.size.x() >> level));
		const auto height = static_cast<GLsizei>(std::max(1u, iLevels.size.y() >> level));
		const auto& data = iLevels.levels[level];
		if (compressed)
			glCompressedTextureSubImage2D(m_textureId, static_cast<GLint>(level), 0, 0, width, height,
										  glInternalDataFormat(iLevels.format), static_cast<GLsizei>(data.size()),
										  data.data());
		else
			glTextureSubImage2D(m_textureId, static_cast<GLint>(level), 0, 0, width, height,
								glDataFormat(iLevels.format), glDataType(iLevels.format), data.data());
	}
	return true;
}

}// namespace owl::renderer::gpu::opengl
