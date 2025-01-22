/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "Texture.h"

#include "Renderer.h"
#include "core/Application.h"
#include "null/Texture.h"
#include "opengl/Texture.h"
#include "vulkan/Texture.h"

namespace owl::renderer {


Texture::Texture(std::filesystem::path iPath) : m_path{std::move(iPath)} {}

Texture::Texture(const Specification& iSpecs) : m_specification{iSpecs} {}

[[nodiscard]] auto Texture::getSerializeString() const -> std::string {
	if (!isLoaded()) {
		return "emp:";
	}
	if (!m_name.empty()) {
		return "nam:" + m_name;
	}
	if (!m_path.empty()) {
		return "pat:" + m_path.string();
	}
	return fmt::format("spec:{}", m_specification.toString());
}

auto Texture::Specification::toString() const -> std::string {
	return fmt::format("{}:{}:{}:{}", size.x(), size.y(), magic_enum::enum_name(format), generateMips);
}
void Texture::Specification::fromString(const std::string& iString) {
	std::stringstream ss(iString);
	std::string token;
	std::getline(ss, token, ':');
	size.x() = static_cast<uint32_t>(std::stoul(token));
	std::getline(ss, token, ':');
	size.y() = static_cast<uint32_t>(std::stoul(token));
	std::getline(ss, token, ':');
	format = magic_enum::enum_cast<ImageFormat>(token).value_or(ImageFormat::RGB8);
	std::getline(ss, token, ':');
	generateMips = token != "false";
}

auto Texture::Specification::getPixelSize() const -> uint8_t {
	switch (format) {
		case ImageFormat::RGB8:
			return 3;
		case ImageFormat::RGBA8:
			return 4;
		case ImageFormat::R8:
			return 1;
		case ImageFormat::RGBA32F:
			return 16;
		case ImageFormat::None:
			return 0;
	}
	return 0;
}

Texture2D::Texture2D(std::filesystem::path iPath) : Texture{std::move(iPath)} {}

Texture2D::Texture2D(const Specification& iSpecs) : Texture{iSpecs} {}

auto Texture2D::create(const std::filesystem::path& iFile) -> shared<Texture2D> {
	switch (RenderCommand::getApi()) {
		case RenderAPI::Type::Null:
			{
				if (auto texture = mkShared<null::Texture2D>(iFile); texture->isLoaded())// No data
					return texture;
				return nullptr;
			}
		case RenderAPI::Type::OpenGL:
			{
				if (auto texture = mkShared<opengl::Texture2D>(iFile); texture->isLoaded())// No data
					return texture;
				return nullptr;
			}
		case RenderAPI::Type::Vulkan:
			{
				if (auto texture = mkShared<vulkan::Texture2D>(iFile); texture->isLoaded())// No data
					return texture;
				return nullptr;
			}
	}

	OWL_CORE_ERROR("Unknown RendererAPI!")
	return nullptr;
}

auto Texture2D::create(const Specification& iSpecs) -> shared<Texture2D> {
	shared<Texture2D> tex;
	switch (RenderCommand::getApi()) {
		case RenderAPI::Type::Null:
			tex = mkShared<null::Texture2D>(iSpecs);
			break;
		case RenderAPI::Type::OpenGL:
			tex = mkShared<opengl::Texture2D>(iSpecs);
			break;
		case RenderAPI::Type::Vulkan:
			tex = mkShared<vulkan::Texture2D>(iSpecs);
			break;
	}
	if (tex) {
		return tex;
	}

	OWL_CORE_ERROR("Unknown RendererAPI!")
	return nullptr;
}

auto Texture2D::createFromSerialized(const std::string& iTextureSerializedName) -> shared<Texture2D> {
	if (iTextureSerializedName.size() < 4)
		return nullptr;
	const auto key = iTextureSerializedName.substr(0, 4);
	const auto val = iTextureSerializedName.substr(4);
	if (key == "emp:")
		return create(Specification{.size = {0, 0}, .format = ImageFormat::RGB8});
	if (key == "nam:")
		return create(val);
	if (key == "pat:")
		return create(std::filesystem::path(val));
	if (key == "siz:") {
		Specification specs;
		specs.fromString(val);
		return create(specs);
	}
	return nullptr;
}

Texture::~Texture() = default;

}// namespace owl::renderer
