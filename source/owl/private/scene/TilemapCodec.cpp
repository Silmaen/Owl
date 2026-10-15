/**
 * @file TilemapCodec.cpp
 * @author Silmaen
 * @date 14/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "TilemapCodec.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace owl::scene::codec {

namespace {

using component::TilemapChunk;
using component::TilemapLayer;

constexpr uint32_t g_fnvBasis = 2166136261U;
constexpr uint32_t g_fnvPrime = 16777619U;
constexpr size_t g_chunkBytes = 2 * sizeof(uint32_t) + TilemapChunk::cellCount * (2 * sizeof(uint16_t) + 1);

void putU16(std::string& ioOut, const uint16_t iValue) {
	ioOut.push_back(static_cast<char>(iValue & 0xFFU));
	ioOut.push_back(static_cast<char>((iValue >> 8U) & 0xFFU));
}

void putU32(std::string& ioOut, const uint32_t iValue) {
	for (uint32_t shift = 0; shift < 32; shift += 8) ioOut.push_back(static_cast<char>((iValue >> shift) & 0xFFU));
}

// Little-endian reader over a byte buffer; every read past the end fails.
class Reader {
public:
	explicit Reader(const std::string_view iBytes) : m_bytes{iBytes} {}

	[[nodiscard]] auto remaining() const -> size_t { return m_bytes.size() - m_cursor; }

	auto take(const size_t iCount) -> std::optional<std::string_view> {
		if (remaining() < iCount)
			return std::nullopt;
		const auto view = m_bytes.substr(m_cursor, iCount);
		m_cursor += iCount;
		return view;
	}

	auto u32() -> std::optional<uint32_t> {
		const auto bytes = take(4);
		if (!bytes)
			return std::nullopt;
		uint32_t value = 0;
		for (uint32_t i = 0; i < 4; ++i) value |= static_cast<uint32_t>(static_cast<uint8_t>((*bytes)[i])) << (8U * i);
		return value;
	}

private:
	std::string_view m_bytes;
	size_t m_cursor = 0;
};

auto byteAt(const std::string_view iBytes, const size_t iIndex) -> uint32_t {
	return static_cast<uint8_t>(iBytes[iIndex]);
}

void readIds(const std::string_view iBytes, std::span<uint16_t> oIds) {
	for (size_t i = 0; i < oIds.size(); ++i)
		oIds[i] = static_cast<uint16_t>(byteAt(iBytes, 2 * i) | (byteAt(iBytes, 2 * i + 1) << 8U));
}

// One chunk read from the payload, applied once the whole payload is known to be valid.
struct DecodedChunk {
	size_t layer = 0;
	math::vec2ui coords;
	TilemapChunk content;
};

auto readChunk(Reader& ioReader, const size_t iLayer, const math::vec2ui& iChunkGrid, DecodedChunk& oChunk) -> bool {
	const auto cx = ioReader.u32();
	const auto cy = ioReader.u32();
	if (!cx || !cy) {
		OWL_CORE_ERROR("TilemapAsset: Payload truncated in a chunk header of layer {}.", iLayer)
		return false;
	}
	if (*cx >= iChunkGrid.x() || *cy >= iChunkGrid.y()) {
		OWL_CORE_ERROR("TilemapAsset: Chunk ({}, {}) of layer {} lies outside its {}×{} chunk grid.", *cx, *cy, iLayer,
					   iChunkGrid.x(), iChunkGrid.y())
		return false;
	}
	const auto terrain = ioReader.take(TilemapChunk::cellCount * sizeof(uint16_t));
	const auto overlay = ioReader.take(TilemapChunk::cellCount * sizeof(uint16_t));
	const auto flags = ioReader.take(TilemapChunk::cellCount);
	if (!terrain || !overlay || !flags) {
		OWL_CORE_ERROR("TilemapAsset: Payload truncated in chunk ({}, {}) of layer {}.", *cx, *cy, iLayer)
		return false;
	}
	oChunk.layer = iLayer;
	oChunk.coords = {*cx, *cy};
	readIds(*terrain, oChunk.content.terrain);
	readIds(*overlay, oChunk.content.overlay);
	for (size_t i = 0; i < TilemapChunk::cellCount; ++i)
		oChunk.content.flags[i] = static_cast<uint8_t>(byteAt(*flags, i));
	return true;
}

auto readHeader(Reader& ioReader, const size_t iLayerCount) -> bool {
	const auto magic = ioReader.take(g_PayloadMagic.size());
	if (!magic || *magic != g_PayloadMagic) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload has no valid magic.")
		return false;
	}
	const auto side = ioReader.u32();
	const auto layerCount = ioReader.u32();
	if (!side || !layerCount) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload truncated in its header.")
		return false;
	}
	if (*side != TilemapChunk::side) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload has chunks of {} cells, expected {}.", *side, TilemapChunk::side)
		return false;
	}
	if (*layerCount != iLayerCount) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload holds {} layers, the header {}.", *layerCount, iLayerCount)
		return false;
	}
	return true;
}

auto readLayers(Reader& ioReader, const std::span<TilemapLayer> iLayers, std::vector<DecodedChunk>& oChunks) -> bool {
	for (size_t layerIndex = 0; layerIndex < iLayers.size(); ++layerIndex) {
		const auto chunkGrid = iLayers[layerIndex].getChunkGridSize();
		const auto count = ioReader.u32();
		if (!count) {
			OWL_CORE_ERROR("TilemapAsset: Payload truncated before the chunks of layer {}.", layerIndex)
			return false;
		}
		if (*count > static_cast<size_t>(chunkGrid.x()) * chunkGrid.y() ||
			*count > ioReader.remaining() / g_chunkBytes) {
			OWL_CORE_ERROR("TilemapAsset: Layer {} announces {} chunks, more than its grid or the payload holds.",
						   layerIndex, *count)
			return false;
		}
		std::set<std::pair<uint32_t, uint32_t>> seen;
		for (uint32_t i = 0; i < *count; ++i) {
			auto& chunk = oChunks.emplace_back();
			if (!readChunk(ioReader, layerIndex, chunkGrid, chunk))
				return false;
			if (!seen.emplace(chunk.coords.x(), chunk.coords.y()).second) {
				OWL_CORE_ERROR("TilemapAsset: Chunk ({}, {}) of layer {} is stored twice.", chunk.coords.x(),
							   chunk.coords.y(), layerIndex)
				return false;
			}
		}
	}
	return true;
}

auto readHeights(Reader& ioReader, const size_t iHeightCount, std::vector<uint8_t>& oHeights) -> bool {
	const auto flag = ioReader.take(1);
	if (!flag || (*flag)[0] > '\1') {
		OWL_CORE_ERROR("TilemapAsset: Payload has no valid height section.")
		return false;
	}
	if ((*flag)[0] == '\0')
		return true;
	const auto bytes = ioReader.take(iHeightCount);
	if (!bytes) {
		OWL_CORE_ERROR("TilemapAsset: Payload truncated in the {} corner heights.", iHeightCount)
		return false;
	}
	oHeights.assign(bytes->begin(), bytes->end());
	return true;
}

}// namespace

auto splitFile(const std::string_view iText) -> TilemapFileParts {
	const auto separator = iText.find(g_PayloadSeparator);
	if (separator == std::string_view::npos)
		return {.header = iText, .payload = {}, .hasPayload = false};
	return {.header = iText.substr(0, separator + 1),
			.payload = iText.substr(separator + g_PayloadSeparator.size()),
			.hasPayload = true};
}

auto encodeTerrainText(const TilemapLayer& iLayer) -> std::string {
	const auto& size = iLayer.getSize();
	std::string out;
	out.reserve(static_cast<size_t>(size.x()) * size.y() * 3);
	for (uint32_t y = 0; y < size.y(); ++y) {
		for (uint32_t x = 0; x < size.x(); ++x) {
			if (x > 0 || y > 0)
				out.push_back(',');
			out += std::to_string(iLayer.getTile(x, y));
		}
	}
	return out;
}

auto decodeTileText(const std::string_view iEncoded, const size_t iExpected) -> std::vector<int32_t> {
	std::vector<int32_t> out(iExpected, component::g_EmptyTileIndex);
	if (iEncoded.empty())
		return out;
	size_t cursor = 0;
	size_t idx = 0;
	while (cursor <= iEncoded.size() && idx < iExpected) {
		const size_t comma = iEncoded.find(',', cursor);
		const size_t end = (comma == std::string_view::npos) ? iEncoded.size() : comma;
		int32_t value = component::g_EmptyTileIndex;
		(void) std::from_chars(iEncoded.data() + cursor, iEncoded.data() + end, value);
		out[idx++] = value;
		if (comma == std::string_view::npos)
			break;
		cursor = comma + 1;
	}
	return out;
}

auto encodePayload(const std::span<const TilemapLayer> iLayers, const std::span<const uint8_t> iHeights)
		-> std::string {
	size_t chunkTotal = 0;
	for (const auto& layer: iLayers) chunkTotal += layer.getAllocatedChunkCount();
	std::string out;
	out.reserve(g_PayloadMagic.size() + 17 + iLayers.size() * 4 + chunkTotal * g_chunkBytes + iHeights.size());
	out.append(g_PayloadMagic);
	putU32(out, TilemapChunk::side);
	putU32(out, static_cast<uint32_t>(iLayers.size()));
	std::vector<std::pair<math::vec2ui, const TilemapChunk*>> stored;
	for (const auto& layer: iLayers) {
		stored.clear();
		const auto grid = layer.getChunkGridSize();
		for (uint32_t cy = 0; cy < grid.y(); ++cy) {
			for (uint32_t cx = 0; cx < grid.x(); ++cx) {
				if (const auto* chunk = layer.getChunk({cx, cy}); chunk != nullptr && !chunk->isEmpty())
					stored.emplace_back(math::vec2ui{cx, cy}, chunk);
			}
		}
		putU32(out, static_cast<uint32_t>(stored.size()));
		for (const auto& [coords, chunk]: stored) {
			putU32(out, coords.x());
			putU32(out, coords.y());
			for (const uint16_t id: chunk->terrain) putU16(out, id);
			for (const uint16_t id: chunk->overlay) putU16(out, id);
			for (const uint8_t flag: chunk->flags) out.push_back(static_cast<char>(flag));
		}
	}
	out.push_back(iHeights.empty() ? '\0' : '\1');
	for (const uint8_t height: iHeights) out.push_back(static_cast<char>(height));
	putU32(out, checksum(out));
	return out;
}

auto decodePayload(const std::string_view iPayload, const std::span<TilemapLayer> ioLayers,
				   std::vector<uint8_t>& oHeights, const size_t iHeightCount, const bool iWithHeights) -> bool {
	if (iPayload.size() < sizeof(uint32_t)) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload truncated ({} bytes).", iPayload.size())
		return false;
	}
	const auto body = iPayload.substr(0, iPayload.size() - sizeof(uint32_t));
	Reader trailer{iPayload.substr(body.size())};
	if (const auto stored = trailer.u32(); !stored || *stored != checksum(body)) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload checksum mismatch (corrupted or truncated file).")
		return false;
	}
	Reader reader{body};
	if (!readHeader(reader, ioLayers.size()))
		return false;
	std::vector<DecodedChunk> chunks;
	if (!readLayers(reader, ioLayers, chunks))
		return false;
	std::vector<uint8_t> heights;
	if (iWithHeights && !readHeights(reader, iHeightCount, heights))
		return false;
	if (reader.remaining() != 0) {
		OWL_CORE_ERROR("TilemapAsset: Binary payload has {} unexpected trailing bytes.", reader.remaining())
		return false;
	}
	for (const auto& [layer, coords, content]: chunks) std::ignore = ioLayers[layer].setChunk(coords, content);
	oHeights = std::move(heights);
	return true;
}

auto checksum(const std::string_view iBytes) -> uint32_t {
	uint32_t hash = g_fnvBasis;
	for (const char byte: iBytes) {
		hash ^= static_cast<uint8_t>(byte);
		hash *= g_fnvPrime;
	}
	return hash;
}

}// namespace owl::scene::codec
