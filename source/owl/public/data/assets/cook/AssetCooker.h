/**
 * @file AssetCooker.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "data/assets/AssetMeta.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

/**
 * @namespace owl::data::assets::cook
 * @brief
 *  Offline processing of the assets into their packed form, and its cache.
 */
namespace owl::data::assets::cook {

/**
 * @brief
 *  Failure of the cook of one asset.
 */
enum struct CookError : uint8_t {
	Unreadable,///< The source file cannot be read.
	InvalidSource,///< The source is no valid asset of its importer (a corrupt image, a scene that does not parse).
	EncodeFailed,///< The encoder rejected the source.
	Cancelled,///< The caller cancelled the cook.
};

/**
 * @brief
 *  Describe a cook failure.
 * @param[in] iError The failure.
 * @return A short message.
 */
[[nodiscard]] OWL_API auto describe(CookError iError) -> std::string_view;

/**
 * @brief
 *  Version of the output of an importer; a new version invalidates every cached result of the importer.
 * @param[in] iImporter The importer.
 * @return The version.
 */
[[nodiscard]] OWL_API auto getCookerVersion(Importer iImporter) -> uint32_t;

/**
 * @brief
 *  Cook one asset: a compressed texture becomes KTX2 (mipmaps included), a scene, prefab or HUD becomes the binary
 *  scene format, any other asset is kept as is (an unreadable image only warns).
 * @param[in] iMeta The asset sidecar (importer and settings).
 * @param[in] iSource Bytes of the source file.
 * @param[in] iName Asset path, for log messages.
 * @return The cooked bytes, or the reason of the failure.
 */
[[nodiscard]] OWL_API auto cookAsset(const AssetMeta& iMeta, std::span<const uint8_t> iSource, std::string_view iName)
		-> expected<std::vector<uint8_t>, CookError>;

}// namespace owl::data::assets::cook
