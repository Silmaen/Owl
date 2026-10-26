/**
 * @file AssetMeta.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "data/assets/AssetReference.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace owl::data::assets {

/// Extension of the sidecar file holding the GUID and the import settings of an asset.
inline constexpr std::string_view g_MetaExtension{".owlmeta"};

/**
 * @brief
 *  How the cook processes an asset; chosen from the file extension.
 */
enum struct Importer : uint8_t {
	Copy,///< Packed as is.
	Texture,///< Image decoded at cook time.
	Mesh,///< 3D model.
	Sound,///< Audio clip.
	Script,///< Lua script.
	Font,///< Font file.
	Document,///< Owl document (scene, material, tileset, tilemap, HUD, prefab, animation).
};

/**
 * @brief
 *  How the cook stores a texture in a pack.
 */
enum struct TextureCompression : uint8_t {
	None,///< The source file as is (PNG, JPEG...): lossless, decoded at load.
	Uastc,///< Basis Universal UASTC in KTX2: near-lossless, transcoded to BC7 / ASTC at load.
	Etc1s,///< Basis Universal ETC1S in KTX2: smallest, lossy, transcoded to BC7 / ASTC at load.
};

/**
 * @brief
 *  Import settings of a texture: the filtering applies wherever the texture is loaded, the rest to the cook.
 */
struct OWL_API TextureImportSettings {
	/// Nearest-texel filtering (pixel art) instead of bilinear.
	bool nearest{false};
	/// Storage of the texture in a pack.
	TextureCompression compression{TextureCompression::None};
	/// Mipmap chain built by the cook (compressed textures only, ignored with nearest filtering).
	bool mips{true};

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other settings.
	 * @return True when every field matches.
	 */
	auto operator==(const TextureImportSettings& iOther) const -> bool = default;
};

/**
 * @brief
 *  Content of an `.owlmeta` sidecar.
 */
struct OWL_API AssetMeta {
	/// GUID of the asset.
	AssetGuid guid;
	/// Importer chosen for the asset.
	Importer importer{Importer::Copy};
	/// Settings of a texture asset (unused by the other importers).
	TextureImportSettings texture;

	/**
	 * @brief
	 *  Equality operator.
	 * @param[in] iOther The other meta.
	 * @return True when every field matches.
	 */
	auto operator==(const AssetMeta& iOther) const -> bool = default;
};

/**
 * @brief
 *  Failure of reading or writing a sidecar.
 */
enum struct MetaError : uint8_t {
	Unreadable,///< The file could not be opened or parsed.
	InvalidGuid,///< The `Guid` field is missing or is not 16 hexadecimal digits.
	NewerVersion,///< Written by a newer Owl, or its version could not be upgraded.
	WriteFailed,///< The file could not be written.
};

/**
 * @brief
 *  Describe a sidecar failure.
 * @param[in] iError The failure.
 * @return A short message.
 */
[[nodiscard]] OWL_API auto describe(MetaError iError) -> std::string_view;

/**
 * @brief
 *  Importer of a file, from its extension.
 * @param[in] iFile The asset file.
 * @return The importer, or nullopt for a file that is not an asset (sidecar, buffer, settings, text).
 */
[[nodiscard]] OWL_API auto importerOf(const std::filesystem::path& iFile) -> std::optional<Importer>;

/**
 * @brief
 *  Name of an importer as written in a sidecar.
 * @param[in] iImporter The importer.
 * @return Its name.
 */
[[nodiscard]] OWL_API auto importerName(Importer iImporter) -> std::string_view;

/**
 * @brief
 *  Path of the sidecar of an asset file.
 * @param[in] iFile The asset file.
 * @return `<file>.owlmeta`.
 */
[[nodiscard]] OWL_API auto metaPathOf(const std::filesystem::path& iFile) -> std::filesystem::path;

/**
 * @brief
 *  Read a sidecar from its text.
 * @param[in] iText YAML text of the sidecar.
 * @param[in] iSourceName Name used in log messages.
 * @return The meta, or the reason it could not be read.
 */
[[nodiscard]] OWL_API auto parseMeta(std::string_view iText, std::string_view iSourceName)
		-> expected<AssetMeta, MetaError>;

/**
 * @brief
 *  Write a sidecar as text.
 * @param[in] iMeta The meta.
 * @return Its YAML text, settings of the importer only.
 */
[[nodiscard]] OWL_API auto serializeMeta(const AssetMeta& iMeta) -> std::string;

/**
 * @brief
 *  Read a sidecar file.
 * @param[in] iMetaFile Path of the `.owlmeta` file.
 * @return The meta, or the reason it could not be read.
 */
[[nodiscard]] OWL_API auto readMeta(const std::filesystem::path& iMetaFile) -> expected<AssetMeta, MetaError>;

/**
 * @brief
 *  Write a sidecar file atomically.
 * @param[in] iMetaFile Path of the `.owlmeta` file.
 * @param[in] iMeta The meta.
 * @return Nothing, or `MetaError::WriteFailed`.
 */
[[nodiscard]] OWL_API auto writeMeta(const std::filesystem::path& iMetaFile, const AssetMeta& iMeta)
		-> expected<void, MetaError>;

}// namespace owl::data::assets
