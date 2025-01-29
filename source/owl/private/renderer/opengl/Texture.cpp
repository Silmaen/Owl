/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"
#include "core/external/opengl46.h"
#include <stb_image.h>

namespace owl::renderer::opengl {

namespace {
auto glDataFormat(const ImageFormat& iFormat) -> GLenum {
	switch (iFormat) {
		case ImageFormat::RGBA8:
			return GL_RGBA;
		case ImageFormat::RGB8:
			return GL_RGB;
		case ImageFormat::R8:
			return GL_RED;
		case ImageFormat::RGBA32F:
			return GL_RGBA32F;
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}
auto glInternalDataFormat(const ImageFormat& iFormat) -> GLenum {
	switch (iFormat) {
		case ImageFormat::RGBA8:
			return GL_RGBA8;
		case ImageFormat::RGB8:
			return GL_RGB8;
		case ImageFormat::R8:
			return GL_RED_INTEGER;
		case ImageFormat::RGBA32F:
			return GL_RGBA32F;
		case ImageFormat::None:
			return GL_NONE;
	}
	return GL_NONE;
}
}// namespace

Texture2D::Texture2D(std::filesystem::path iPath) : renderer::Texture2D{std::move(iPath)} {
	OWL_PROFILE_FUNCTION()

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc* data = nullptr;
	{
		OWL_PROFILE_SCOPE("stbi_load - OpenGL::Texture2D::Texture2D(const std::filesystem::path &)")
		data = stbi_load(m_path.string().c_str(), &width, &height, &channels, 0);
	}
	if (data == nullptr) {
		OWL_CORE_WARN("OpenGL Texture: Failed to load image {}", m_path.string())
		return;
	}


	if ((channels != 4) && (channels != 3)) {
		OWL_CORE_ERROR("OpenGL Texture: Impossible to load {}, invalid number of channels {}: must be 3 or 4.")
		return;
	}
	m_specification.format = channels == 4 ? ImageFormat::RGBA8 : ImageFormat::RGB8;
	m_specification.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);
	glTextureStorage2D(m_textureId, 1, glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	glTextureParameteri(m_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_textureId, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTextureSubImage2D(m_textureId, 0, 0, 0, static_cast<GLsizei>(m_specification.size.x()),
						static_cast<GLsizei>(m_specification.size.y()), glDataFormat(m_specification.format),
						GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);
}

Texture2D::Texture2D(const Specification& iSpecs) : renderer::Texture2D{iSpecs} {
	OWL_PROFILE_FUNCTION()

	glCreateTextures(GL_TEXTURE_2D, 1, &m_textureId);
	glTextureStorage2D(m_textureId, 1, glInternalDataFormat(m_specification.format),
					   static_cast<GLsizei>(m_specification.size.x()), static_cast<GLsizei>(m_specification.size.y()));

	glTextureParameteri(m_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(m_textureId, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTextureParameteri(m_textureId, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2D::~Texture2D() {
	OWL_PROFILE_FUNCTION()

	glDeleteTextures(1, &m_textureId);
}

void Texture2D::bind(const uint32_t iSlot) const {
	OWL_PROFILE_FUNCTION()

	glBindTextureUnit(iSlot, m_textureId);
}

void Texture2D::setData(void* iData, [[maybe_unused]] const uint32_t iSize) {
	OWL_PROFILE_FUNCTION()

	OWL_CORE_ASSERT(iSize == m_specification.size.surface() * m_specification.getPixelSize(),
					"Data size missmatch texture size!")
	glTextureSubImage2D(m_textureId, 0, 0, 0, static_cast<GLsizei>(m_specification.size.x()),
						static_cast<GLsizei>(m_specification.size.y()), glDataFormat(m_specification.format),
						GL_UNSIGNED_BYTE, iData);
}

}// namespace owl::renderer::opengl
