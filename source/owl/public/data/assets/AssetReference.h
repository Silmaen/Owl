/**
 * @file AssetReference.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/UUID.h"

#include <optional>
#include <string>
#include <string_view>

namespace owl::data::assets {

/// GUID of an asset, kept in the `.owlmeta` sidecar next to its file.
using AssetGuid = core::UUID;

/// Prefix of a reference by GUID: `gid:<16 hex digits>:<path hint>`.
inline constexpr std::string_view g_GuidReferencePrefix{"gid:"};

/**
 * @brief
 *  Write a GUID as 16 lowercase hexadecimal digits.
 * @param[in] iGuid The GUID.
 * @return The text form.
 */
[[nodiscard]] OWL_API auto formatGuid(AssetGuid iGuid) -> std::string;

/**
 * @brief
 *  Read a GUID written by `formatGuid`.
 * @param[in] iText Exactly 16 hexadecimal digits, either case.
 * @return The GUID, or nullopt when the text is not a GUID.
 */
[[nodiscard]] OWL_API auto parseGuid(std::string_view iText) -> std::optional<AssetGuid>;

/**
 * @brief
 *  An asset reference split into its parts.
 */
struct OWL_API ParsedReference {
	/// GUID of a `gid:` reference; nullopt for a plain path.
	std::optional<AssetGuid> guid;
	/// Path hint of a `gid:` reference, or the plain path itself.
	std::string path;
};

/**
 * @brief
 *  Split a serialized reference: `gid:<guid>:<hint>` or a plain path, kept as is.
 * @param[in] iReference The serialized reference.
 * @return Its GUID (if any) and its path.
 */
[[nodiscard]] OWL_API auto parseReference(std::string_view iReference) -> ParsedReference;

/**
 * @brief
 *  Build the serialized form of a reference by GUID.
 * @param[in] iGuid The asset GUID.
 * @param[in] iPathHint Path of the asset relative to its asset directory, used when the GUID is unknown.
 * @return `gid:<guid>:<hint>`.
 */
[[nodiscard]] OWL_API auto formatReference(AssetGuid iGuid, std::string_view iPathHint) -> std::string;

/**
 * @brief
 *  Turn a serialized reference into the path the loaders search, through the application asset registry.
 *
 * A known GUID gives the current path of its asset, an unknown one the hint; a plain path is returned unchanged.
 * @param[in] iReference The serialized reference.
 * @return The asset path, relative to an asset directory (or as written for a plain path).
 */
[[nodiscard]] OWL_API auto toAssetPath(std::string_view iReference) -> std::string;

/**
 * @brief
 *  Turn an asset path into the reference to serialize, through the application asset registry.
 *
 * A file the registry knows gives `gid:<guid>:<relative path>`; any other path is returned unchanged.
 * @param[in] iPath The asset path, relative to an asset directory or absolute.
 * @return The reference to write.
 */
[[nodiscard]] OWL_API auto toAssetReference(std::string_view iPath) -> std::string;

/// Prefix of a texture serialized by name, relative to an asset directory.
inline constexpr std::string_view g_TextureNamePrefix{"nam:"};

/**
 * @brief
 *  Turn a texture serialized by name (`nam:<path>`) into a reference by GUID when the registry knows the file.
 * @param[in] iSerialized A texture serialized string (`nam:`, `pat:`, `gid:`, `emp:`...).
 * @return `gid:<guid>:<path>` for a known `nam:` texture, the string unchanged otherwise.
 */
[[nodiscard]] OWL_API auto toTextureReference(std::string_view iSerialized) -> std::string;

/**
 * @brief
 *  Turn a texture reference by GUID into a texture serialized by name, at the current path of the file.
 * @param[in] iSerialized A texture serialized string.
 * @return `nam:<path>` for a `gid:` reference, the string unchanged otherwise.
 */
[[nodiscard]] OWL_API auto fromTextureReference(std::string_view iSerialized) -> std::string;

}// namespace owl::data::assets
