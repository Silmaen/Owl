/**
 * @file AssetCooker.cpp
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "owlpch.h"

#include "data/assets/cook/AssetCooker.h"

#include "core/YamlNode.h"
#include "core/external/ktx.h"
#include "renderer/TextureDecoder.h"
#include "scene/EntityLinkMigration.h"
#include "scene/PrefabSerializer.h"
#include "scene/SceneSerializer.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>

namespace owl::data::assets::cook {

namespace {

// `VK_FORMAT_R8G8B8A8_UNORM`: colour textures are uploaded unorm, the shaders convert from sRGB.
constexpr uint32_t g_vkFormatRgba8Unorm = 37;
// Zstandard level of the UASTC supercompression: the cook runs offline, the pack is read many times.
constexpr ktx_uint32_t g_zstdLevel = 18;
// Quality of the ETC1S encoder (1 to 255).
constexpr ktx_uint32_t g_etc1sQuality = 128;

constexpr uint32_t g_textureCookerVersion = 1;
constexpr uint32_t g_copyCookerVersion = 1;
constexpr uint32_t g_documentCookerVersion = 1;

auto isSceneDocument(const std::filesystem::path& iFile) -> bool {
	const auto extension = iFile.extension();
	return extension == ".owl" || extension == ".owlhud" || extension == ".owlprefab";
}

// A scene, HUD or prefab upgraded to the current format and stored as a binary tree: the runner skips the parse.
auto cookSceneDocument(const std::span<const uint8_t> iSource, const std::string_view iName)
		-> expected<std::vector<uint8_t>, CookError> {
	OWL_PROFILE_FUNCTION()

	std::string text{iSource.begin(), iSource.end()};
	if (core::isBinaryYaml(text))
		return std::vector<uint8_t>(iSource.begin(), iSource.end());
	const auto kind = scene::documentKindOf(std::filesystem::path{iName});
	const auto& format = kind == scene::DocumentKind::Prefab ? scene::PrefabSerializer::format()
															 : scene::SceneSerializer::format(kind);
	try {
		auto document = mkUniq<core::YamlDocument>(text, iName);
		if (!scene::canReadWithoutMigration(document->getRoot(), format)) {
			if (const auto version = core::upgradeDocumentText(format, text, iName); !version) {
				OWL_CORE_WARN("Cook: Cannot upgrade '{}': {}.", iName, core::describe(version.error()))
				return unexpected{CookError::InvalidSource};
			}
			document = mkUniq<core::YamlDocument>(text, iName);
		}
		const auto binary = document->toBinary();
		return std::vector<uint8_t>(binary.begin(), binary.end());
	} catch (const core::YamlError& error) {
		OWL_CORE_WARN("Cook: '{}' does not parse: {}.", iName, error.what())
		return unexpected{CookError::InvalidSource};
	}
}

// Halve an RGBA8 level with a box filter; an odd edge averages the texels it has.
auto downsample(const std::vector<uint8_t>& iPixels, const math::vec2ui iSize) -> std::vector<uint8_t> {
	const math::vec2ui half{std::max(1u, iSize.x() / 2), std::max(1u, iSize.y() / 2)};
	std::vector<uint8_t> result(static_cast<size_t>(half.surface()) * 4);
	for (uint32_t y = 0; y < half.y(); ++y) {
		for (uint32_t x = 0; x < half.x(); ++x) {
			for (uint32_t channel = 0; channel < 4; ++channel) {
				uint32_t sum = 0;
				uint32_t count = 0;
				for (uint32_t dy = 0; dy < 2; ++dy) {
					for (uint32_t dx = 0; dx < 2; ++dx) {
						const uint32_t sx = std::min(x * 2 + dx, iSize.x() - 1);
						const uint32_t sy = std::min(y * 2 + dy, iSize.y() - 1);
						sum += iPixels[(static_cast<size_t>(sy) * iSize.x() + sx) * 4 + channel];
						++count;
					}
				}
				result[(static_cast<size_t>(y) * half.x() + x) * 4 + channel] = static_cast<uint8_t>(sum / count);
			}
		}
	}
	return result;
}

auto cookTexture(const AssetMeta& iMeta, const std::span<const uint8_t> iSource, const std::string_view iName)
		-> expected<std::vector<uint8_t>, CookError> {
	OWL_PROFILE_FUNCTION()

	if (iMeta.texture.compression == TextureCompression::None) {
		if (!renderer::peekImageSize(iSource))
			OWL_CORE_WARN("Cook: '{}' is no readable image, packed as is: the game will not show it.", iName)
		return std::vector<uint8_t>(iSource.begin(), iSource.end());
	}
	const auto decoded = renderer::decodeImageBytes(iSource, 4);
	if (!decoded.valid) {
		OWL_CORE_WARN("Cook: '{}' is no readable image.", iName)
		return unexpected{CookError::InvalidSource};
	}
	std::vector<std::vector<uint8_t>> levels{decoded.pixels};
	if (iMeta.texture.mips && !iMeta.texture.nearest) {
		math::vec2ui size = decoded.size;
		while (size.x() > 1 || size.y() > 1) {
			levels.push_back(downsample(levels.back(), size));
			size = {std::max(1u, size.x() / 2), std::max(1u, size.y() / 2)};
		}
	}

	ktxTextureCreateInfo info{};
	info.vkFormat = g_vkFormatRgba8Unorm;
	info.baseWidth = decoded.size.x();
	info.baseHeight = decoded.size.y();
	info.baseDepth = 1;
	info.numDimensions = 2;
	info.numLevels = static_cast<ktx_uint32_t>(levels.size());
	info.numLayers = 1;
	info.numFaces = 1;
	info.isArray = KTX_FALSE;
	info.generateMipmaps = KTX_FALSE;
	ktxTexture2* texture = nullptr;
	if (const auto result = ktxTexture2_Create(&info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
		result != KTX_SUCCESS) {
		OWL_CORE_WARN("Cook: Cannot create the KTX2 texture of '{}' ({}).", iName, ktxErrorString(result))
		return unexpected{CookError::EncodeFailed};
	}
	auto* base = ktxTexture(texture);
	for (size_t level = 0; level < levels.size(); ++level)
		ktxTexture_SetImageFromMemory(base, static_cast<ktx_uint32_t>(level), 0, 0, levels[level].data(),
									  levels[level].size());

	ktxBasisParams params{};
	params.structSize = sizeof(params);
	params.threadCount = std::max(1u, std::thread::hardware_concurrency());
	const bool uastc = iMeta.texture.compression == TextureCompression::Uastc;
	params.uastc = uastc ? KTX_TRUE : KTX_FALSE;
	if (uastc)
		params.uastcFlags = KTX_PACK_UASTC_LEVEL_DEFAULT;
	else
		params.qualityLevel = g_etc1sQuality;
	auto result = ktxTexture2_CompressBasisEx(texture, &params);
	if (result == KTX_SUCCESS && uastc)
		result = ktxTexture2_DeflateZstd(texture, g_zstdLevel);
	if (result != KTX_SUCCESS) {
		OWL_CORE_WARN("Cook: Cannot compress '{}' ({}).", iName, ktxErrorString(result))
		ktxTexture2_Destroy(texture);
		return unexpected{CookError::EncodeFailed};
	}
	ktx_uint8_t* bytes = nullptr;
	ktx_size_t size = 0;
	result = ktxTexture_WriteToMemory(base, &bytes, &size);
	ktxTexture2_Destroy(texture);
	if (result != KTX_SUCCESS || bytes == nullptr) {
		OWL_CORE_WARN("Cook: Cannot write the KTX2 texture of '{}' ({}).", iName, ktxErrorString(result))
		return unexpected{CookError::EncodeFailed};
	}
	std::vector<uint8_t> cooked(bytes, bytes + size);
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory) libktx allocates the file with malloc.
	std::free(bytes);
	return cooked;
}

}// namespace

auto describe(const CookError iError) -> std::string_view {
	switch (iError) {
		case CookError::Unreadable:
			return "the source cannot be read";
		case CookError::InvalidSource:
			return "the source is not a valid asset";
		case CookError::EncodeFailed:
			return "the encoder rejected the source";
		case CookError::Cancelled:
			return "the cook was cancelled";
	}
	return "unknown cook error";
}

auto getCookerVersion(const Importer iImporter) -> uint32_t {
	switch (iImporter) {
		case Importer::Texture:
			return g_textureCookerVersion;
		case Importer::Document:
			return g_documentCookerVersion;
		case Importer::Copy:
		case Importer::Mesh:
		case Importer::Sound:
		case Importer::Script:
		case Importer::Font:
			return g_copyCookerVersion;
	}
	return g_copyCookerVersion;
}

auto cookAsset(const AssetMeta& iMeta, const std::span<const uint8_t> iSource, const std::string_view iName)
		-> expected<std::vector<uint8_t>, CookError> {
	switch (iMeta.importer) {
		case Importer::Texture:
			return cookTexture(iMeta, iSource, iName);
		case Importer::Document:
			if (isSceneDocument(std::filesystem::path{iName}))
				return cookSceneDocument(iSource, iName);
			break;
		case Importer::Copy:
		case Importer::Mesh:
		case Importer::Sound:
		case Importer::Script:
		case Importer::Font:
			break;
	}
	return std::vector<uint8_t>(iSource.begin(), iSource.end());
}

}// namespace owl::data::assets::cook
