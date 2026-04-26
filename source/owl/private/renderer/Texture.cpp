/**
 * @file Texture.cpp
 * @author Silmaen
 * @date 12/12/2022
 * Copyright © 2022 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "renderer/Texture.h"

#include "core/Application.h"
#include "core/task/Scheduler.h"
#include "null/Texture.h"
#include "opengl/Texture.h"
#include "renderer/Renderer.h"
#include "renderer/TextureDecoder.h"
#include "vulkan/Texture.h"

#include <fstream>

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
	return std::format("spec:{}", m_specification.toString());
}

auto Texture::Specification::toString() const -> std::string {
	return std::format("{}:{}:{}:{}", size.x(), size.y(), magic_enum::enum_name(format), generateMips);
}
void Texture::Specification::fromString(const std::string& iString) {
	std::stringstream ss(iString);
	std::string token;
	std::getline(ss, token, ':');
	size.x() = static_cast<uint32_t>(std::stoul(token));
	std::getline(ss, token, ':');
	size.y() = static_cast<uint32_t>(std::stoul(token));
	std::getline(ss, token, ':');
	format = magic_enum::enum_cast<ImageFormat>(token).value_or(ImageFormat::Rgb8);
	std::getline(ss, token, ':');
	generateMips = token != "false";
}

auto Texture::Specification::getPixelSize() const -> uint8_t {
	switch (format) {
		case ImageFormat::Rgb8:
			return 3;
		case ImageFormat::Rgba8:
			return 4;
		case ImageFormat::R8:
			return 1;
		case ImageFormat::Rgba32F:
			return 16;
		case ImageFormat::None:
			return 0;
	}
	return 0;
}

Texture2D::Texture2D(std::filesystem::path iPath) : Texture{std::move(iPath)} {}

Texture2D::Texture2D(const Specification& iSpecs) : Texture{iSpecs} {}

auto Texture2D::create(const std::filesystem::path& iFile) -> shared<Texture2D> {
	const auto api = RenderCommand::getApi();
	switch (api) {
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

	OWL_CORE_ERROR("Unknown RendererAPI ({})", static_cast<int>(api))
	return nullptr;
}

auto Texture2D::create(const Specification& iSpecs) -> shared<Texture2D> {
	const shared<Texture2D> tex;
	const auto api = RenderCommand::getApi();
	switch (api) {
		case RenderAPI::Type::Null:
			return mkShared<null::Texture2D>(iSpecs);
		case RenderAPI::Type::OpenGL:
			return mkShared<opengl::Texture2D>(iSpecs);
		case RenderAPI::Type::Vulkan:
			return mkShared<vulkan::Texture2D>(iSpecs);
	}

	OWL_CORE_ERROR("Unknown RendererAPI ({})", static_cast<int>(api))
	return nullptr;
}

namespace {

/// @brief Read a file on disk into a byte buffer; returns empty on failure.
auto readFileBytes(const std::filesystem::path& iPath) -> std::vector<uint8_t> {
	std::ifstream in(iPath, std::ios::binary | std::ios::ate);
	if (!in) {
		return {};
	}
	const auto size = static_cast<std::streamsize>(in.tellg());
	if (size <= 0) {
		return {};
	}
	in.seekg(0);
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	in.read(reinterpret_cast<char*>(bytes.data()), size);
	return bytes;
}

/// @brief Source bytes resolved from a `nam:`/`pat:` serialized reference.
struct ResolvedSource {
	std::vector<uint8_t> bytes;///< File contents (already in memory).
	std::filesystem::path path;///< Resolved disk path, empty when the bytes came from a pack.
	std::string name;///< Value of the `nam:` prefix, empty for `pat:`.
};

/// @brief Resolve a `nam:`/`pat:` reference to its bytes (pack → asset dirs → filesystem).
auto resolveTextureSource(std::string_view iKey, const std::string& iValue) -> ResolvedSource {
	ResolvedSource out;
	if (iKey == "nam:") {
		out.name = iValue;
		if (core::Application::instanced() && core::Application::get().hasOpenPack()) {
			if (auto data = core::Application::get().loadFromPack(iValue); data) {
				out.bytes = std::move(*data);
				return out;
			}
		}
		if (core::Application::instanced()) {
			const std::filesystem::path name(iValue);
			for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
				if (const std::filesystem::path filePath = assetsPath / name;
					std::filesystem::exists(filePath)) {
					out.path = filePath;
					out.bytes = readFileBytes(filePath);
					return out;
				}
			}
		}
		out.path = std::filesystem::path(iValue);
		out.bytes = readFileBytes(out.path);
		return out;
	}
	// "pat:"
	out.path = std::filesystem::path(iValue);
	out.bytes = readFileBytes(out.path);
	return out;
}

}// namespace

auto Texture2D::createFromSerialized(const std::string& iTextureSerializedName) -> shared<Texture2D> {
	if (iTextureSerializedName.size() < 4)
		return nullptr;
	const auto key = iTextureSerializedName.substr(0, 4);
	const auto val = iTextureSerializedName.substr(4);
	if (key == "emp:")
		return create(Specification{.size = {0, 0}, .format = ImageFormat::Rgb8});
	if (key == "nam:") {
		// Check pack first.
		if (core::Application::instanced() && core::Application::get().hasOpenPack()) {
			if (auto data = core::Application::get().loadFromPack(val); data) {
				const auto tempDir = std::filesystem::temp_directory_path() / "owl_pack_cache";
				std::filesystem::create_directories(tempDir);
				const auto tempFile = tempDir / std::filesystem::path(val).filename();
				{
					std::ofstream out(tempFile, std::ios::binary);
					out.write(reinterpret_cast<const char*>(data->data()),
							  static_cast<std::streamsize>(data->size()));
				}
				auto texture = create(tempFile);
				if (texture != nullptr) {
					texture->m_name = val;
					return texture;
				}
			}
		}
		// Resolve the asset name through registered asset directories.
		if (core::Application::instanced()) {
			const std::filesystem::path name(val);
			for (const auto& [title, assetsPath]: core::Application::get().getAssetDirectories()) {
				if (const std::filesystem::path filePath = assetsPath / name; std::filesystem::exists(filePath)) {
					auto texture = create(filePath);
					if (texture != nullptr)
						texture->m_name = val;
					return texture;
				}
			}
		}
		return create(std::filesystem::path(val));
	}
	if (key == "pat:")
		return create(std::filesystem::path(val));
	if (key == "siz:") {
		Specification specs;
		specs.fromString(val);
		return create(specs);
	}
	return nullptr;
}

auto Texture2D::createFromSerializedAsync(const std::string& iTextureSerializedName,
										  core::task::Scheduler& ioScheduler) -> shared<Texture2D> {
	if (iTextureSerializedName.size() < 4) {
		return nullptr;
	}
	const auto key = iTextureSerializedName.substr(0, 4);
	// "emp:" and "siz:" do not carry a decodable image; use the sync factory.
	if (key == "emp:" || key == "siz:") {
		return createFromSerialized(iTextureSerializedName);
	}
	if (key != "nam:" && key != "pat:") {
		return nullptr;
	}
	const auto val = iTextureSerializedName.substr(4);

	auto source = resolveTextureSource(key, val);
	if (source.bytes.empty()) {
		// Could not get bytes; fall back to sync path which handles the warning + failure cleanly.
		return createFromSerialized(iTextureSerializedName);
	}

	// Peek dimensions cheaply from the header, so the GPU texture can be created at the right size
	// before the full decode completes.
	const auto peeked = peekImageSize(source.bytes);
	if (!peeked) {
		return createFromSerialized(iTextureSerializedName);
	}

	// Always create the placeholder in Rgba8 so the later async upload is a simple setData of
	// matching size (the worker decodes with desired_channels = 4).
	auto texture = create(Specification{.size = *peeked, .format = ImageFormat::Rgba8, .generateMips = false});
	if (!texture) {
		return nullptr;
	}
	if (!source.name.empty()) {
		texture->m_name = source.name;
	}
	if (!source.path.empty()) {
		texture->m_path = source.path;
	}
	texture->m_loadState = LoadState::Pending;

	// Fill the GPU placeholder with opaque white (W * H * 4 bytes = 0xFF).
	const auto byteCount = static_cast<size_t>(peeked->x()) * static_cast<size_t>(peeked->y()) * 4u;
	std::vector<uint8_t> whiteBuffer(byteCount, 0xFFu);
	texture->setData(whiteBuffer.data(), static_cast<uint32_t>(whiteBuffer.size()));

	// Hand off decode work to a worker; termination callback uploads the real pixels on the main thread.
	auto sharedBytes = mkShared<std::vector<uint8_t>>(std::move(source.bytes));
	auto sharedDecoded = mkShared<DecodedImage>();
	const weak<Texture2D> weakTex = texture;

	ioScheduler.pushTask(core::task::Task{
			[sharedBytes, sharedDecoded]() -> void {
				*sharedDecoded = decodeImageBytes(std::span<const uint8_t>(*sharedBytes), 4);
			},
			[weakTex, sharedDecoded]() -> void {
				const auto tex = weakTex.lock();
				if (!tex) {
					return;// texture handle was released while the worker ran
				}
				if (!sharedDecoded->valid) {
					tex->m_loadState = LoadState::Failed;
					return;
				}
				tex->setData(sharedDecoded->pixels.data(),
							 static_cast<uint32_t>(sharedDecoded->pixels.size()));
				tex->m_loadState = LoadState::Ready;
			},
	});

	return texture;
}

auto Texture2D::createFromSerializedForDeserialize(const std::string& iTextureSerializedName) -> shared<Texture2D> {
	if (core::Application::instanced()) {
		return createFromSerializedAsync(iTextureSerializedName, core::Application::get().getTaskScheduler());
	}
	return createFromSerialized(iTextureSerializedName);
}

Texture::~Texture() = default;

}// namespace owl::renderer
