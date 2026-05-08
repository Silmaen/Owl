/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 12/12/2022
 * Copyright (c) 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"
#include "core/external/opengl46.h"
#include "renderer/TextureDecoder.h"

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
			return GL_RGBA32F;
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}

auto glInternalDataFormat(const ImageFormat& iFormat) -> GLenum {
	switch (iFormat) {
		case ImageFormat::Rgba8:
			return GL_RGBA8;
		case ImageFormat::Rgb8:
			return GL_RGB8;
		case ImageFormat::R8:
			return GL_RED_INTEGER;
		case ImageFormat::Rgba32F:
			return GL_RGBA32F;
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}

/**
 * @brief
 *  Apply min/mag/wrap filter parameters honouring the spec's `filterMode`.
 *
 * `Nearest` snaps both min and mag to `GL_NEAREST` so the texture stays
 * pixel-crisp at any distance — required for the raycaster wall stripes
 * (otherwise distant walls turn into blurry filtered mush). `Linear` keeps
 * the existing behaviour (linear minification, nearest magnification — a
 * compromise that matches Owl's typical 2D content).
 */
void applySamplerFilter(const GLuint iTexture, const FilterMode iMode) {
	if (iMode == FilterMode::Nearest) {
		glTextureParameteri(iTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(iTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTextureParameteri(iTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(iTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

	glTextureStorage2D(m_textureId, 1, glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	applySamplerFilter(m_textureId, m_specification.filterMode);

	setData(const_cast<uint8_t*>(decoded.pixels.data()), static_cast<uint32_t>(decoded.pixels.size()));
}

Texture2D::Texture2D(const Specification& iSpecs) : renderer::gpu::Texture2D{iSpecs} {

	OWL_PROFILE_FUNCTION()

	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);

	glTextureStorage2D(m_textureId, 1, glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	applySamplerFilter(m_textureId, m_specification.filterMode);
}

Texture2D::~Texture2D() {
	OWL_PROFILE_FUNCTION()

	glDeleteTextures(1, &m_textureId);
}

void Texture2D::bind(const uint32_t iSlot) const {
	OWL_PROFILE_FUNCTION()

	glBindTextureUnit(iSlot, m_textureId);
}

void Texture2D::setFilterMode(const FilterMode iMode) {
	OWL_PROFILE_FUNCTION()

	m_specification.filterMode = iMode;
	if (m_textureId != 0)
		applySamplerFilter(m_textureId, iMode);
}

void Texture2D::setData(void* iData, [[maybe_unused]] const uint32_t iSize) {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(iSize == m_specification.size.surface() * m_specification.getPixelSize(),
					"Data size mismatch texture size!")
	glTextureSubImage2D(m_textureId, 0, 0, 0, static_cast<GLsizei>(m_specification.size.x()),
						static_cast<GLsizei>(m_specification.size.y()), glDataFormat(m_specification.format),
						GL_UNSIGNED_BYTE, iData);
}

}// namespace owl::renderer::gpu::opengl
