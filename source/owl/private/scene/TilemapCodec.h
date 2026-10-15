/**
 * @file TilemapCodec.h
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "scene/component/Tilemap.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace owl::scene::codec {

/// Line separating the YAML header of a `.owltilemap` from its binary payload (the YAML end-of-document marker).
inline constexpr std::string_view g_PayloadSeparator{"\n...\n"};

/// First bytes of the binary payload.
inline constexpr std::string_view g_PayloadMagic{"OWLTCHNK"};

/**
 * @brief
 *  Header and payload of a `.owltilemap` text.
 */
struct TilemapFileParts {
	/// The YAML header (the whole text when there is no payload).
	std::string_view header;
	/// The bytes after the separator, empty when there is none.
	std::string_view payload;
	/// Whether the separator was found.
	bool hasPayload = false;
};

/**
 * @brief
 *  Split a `.owltilemap` text at its first payload separator.
 * @param[in] iText The file content.
 * @return The header and the payload.
 */
[[nodiscard]] OWL_API auto splitFile(std::string_view iText) -> TilemapFileParts;

/**
 * @brief
 *  Comma-separated terrain indices of a layer, row-major (the text form of the scene inline tilemaps).
 * @param[in] iLayer The layer.
 * @return The encoded text.
 */
[[nodiscard]] OWL_API auto encodeTerrainText(const component::TilemapLayer& iLayer) -> std::string;

/**
 * @brief
 *  Decode comma-separated tile indices.
 * @param[in] iEncoded The text.
 * @param[in] iExpected Number of cells; missing ones are empty, extra ones ignored.
 * @return The indices.
 */
[[nodiscard]] OWL_API auto decodeTileText(std::string_view iEncoded, size_t iExpected) -> std::vector<int32_t>;

/**
 * @brief
 *  Binary payload holding the non-empty chunks of every layer.
 *
 * Little-endian whatever the host: the magic, `u32` chunk side, `u32` layer count, then per layer a `u32` chunk
 * count and per chunk `u32` x, `u32` y, the `u16` terrain ids, the `u16` overlay ids and the `u8` flags of its
 * cells; then a `u8` height flag followed, when 1, by the `u8` corner heights; a final `u32` FNV-1a checksum of every
 * byte before it.
 * @param[in] iLayers The layers.
 * @param[in] iHeights The corner heights, empty for a flat map.
 * @return The payload bytes.
 */
[[nodiscard]] OWL_API auto encodePayload(std::span<const component::TilemapLayer> iLayers,
										 std::span<const uint8_t> iHeights) -> std::string;

/**
 * @brief
 *  Read a binary payload into layers already sized; every chunk it holds replaces the layer's one.
 *
 * Refuses (and logs why) a truncated payload, a wrong magic, chunk side or layer count, a chunk outside its layer
 * or given twice, trailing bytes and a checksum mismatch; the layers are only written when the whole payload is
 * valid.
 * @param[in] iPayload The payload bytes.
 * @param[in,out] ioLayers The layers, in file order.
 * @param[out] oHeights The corner heights, left empty when the payload holds none.
 * @param[in] iHeightCount Number of corners of the grid, `(width + 1) * (height + 1)`.
 * @param[in] iWithHeights Whether the payload has the height section (format 3 and later).
 * @return True when the payload was valid and applied.
 */
[[nodiscard]] OWL_API auto decodePayload(std::string_view iPayload, std::span<component::TilemapLayer> ioLayers,
										 std::vector<uint8_t>& oHeights, size_t iHeightCount, bool iWithHeights)
		-> bool;

/**
 * @brief
 *  32-bit FNV-1a hash.
 * @param[in] iBytes The bytes.
 * @return The hash.
 */
[[nodiscard]] OWL_API auto checksum(std::string_view iBytes) -> uint32_t;

}// namespace owl::scene::codec
