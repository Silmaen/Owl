/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 30/06/2023
 * Copyright © 2023 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include <stb_image.h>

#include <utility>

namespace owl::renderer::opengl_legacy {


Texture2D::Texture2D(uint32_t width_, uint32_t height_) : width{width_}, height{height_} {
	OWL_PROFILE_FUNCTION()

	internalFormat = GL_RGBA8;
	dataFormat = GL_RGBA;

	gl_21::glGenTextures(1, &rendererID);
	gl_21::glBindTexture(GL_TEXTURE_2D, rendererID);
	gl_21::glTexStorage2D(rendererID, 1, internalFormat, static_cast<gl_21::GLsizei>(width), static_cast<gl_21::GLsizei>(height));

	gl_21::glTexParameteri(rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl_21::glTexParameteri(rendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl_21::glTexParameteri(rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl_21::glTexParameteri(rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2D::Texture2D(std::filesystem::path path_) : path{std::move(path_)} {
	OWL_PROFILE_FUNCTION()

	int width_, height_, channels;
	stbi_set_flip_vertically_on_load(1);
	stbi_uc *data = nullptr;
	{
		OWL_PROFILE_SCOPE("stbi_load - Texture2D::Texture2D(const std::filesystem::path &)")
		data = stbi_load(path.string().c_str(), &width_, &height_, &channels, 0);
	}
	if (!data) {
		OWL_CORE_WARN("Failed to load image {}", path.string())
		return;
	}
	width = static_cast<uint32_t>(width_);
	height = static_cast<uint32_t>(height_);

	if (channels == 4) {
		internalFormat = GL_RGBA8;
		dataFormat = GL_RGBA;
	} else if (channels == 3) {
		internalFormat = GL_RGB8;
		dataFormat = GL_RGB;
	}

	OWL_CORE_ASSERT(internalFormat & dataFormat, "Format not supported!")
	gl_21::glGenTextures(1, &rendererID);
	gl_21::glBindTexture(GL_TEXTURE_2D, rendererID);
	gl_21::glTexStorage2D(rendererID, 1, internalFormat, static_cast<gl_21::GLsizei>(width), static_cast<gl_21::GLsizei>(height));

	gl_21::glTexParameteri(rendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl_21::glTexParameteri(rendererID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl_21::glTexParameteri(rendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl_21::glTexParameteri(rendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	gl_21::glTexSubImage2D(rendererID, 0, 0, 0, static_cast<gl_21::GLsizei>(width), static_cast<gl_21::GLsizei>(height), dataFormat, GL_UNSIGNED_BYTE, data);

	stbi_image_free(data);
}

Texture2D::~Texture2D() {
	OWL_PROFILE_FUNCTION()

	gl_21::glDeleteTextures(1, &rendererID);
}

void Texture2D::bind([[maybe_unused]] uint32_t slot) const {
	OWL_PROFILE_FUNCTION()

	gl_21::glBindTexture(rendererID, rendererID);
}

void Texture2D::setData([[maybe_unused]] void *data, [[maybe_unused]] uint32_t size) {
	OWL_PROFILE_FUNCTION()

	[[maybe_unused]] uint32_t bpp = dataFormat == GL_RGBA ? 4 : 3;
	OWL_CORE_ASSERT(size == width * height * bpp, "Data must be entire texture!")
	gl_21::glTexSubImage2D(rendererID, 0, 0, 0, static_cast<gl_21::GLsizei>(width), static_cast<gl_21::GLsizei>(height), dataFormat, GL_UNSIGNED_BYTE, data);
}

}// namespace owl::renderer::opengl_legacy
