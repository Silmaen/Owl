/**
 * @file PackFormat.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "io/pack/PackFormat.h"
#include "core/Macros.h"

#include <cstring>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wunsafe-buffer-usage")
#include <zstd.h>
OWL_DIAG_POP

namespace owl::io::pack {

namespace {
/// FNV-1a constants for 64-bit.
constexpr uint64_t g_fnvBasis = 14695981039346656037ULL;
constexpr uint64_t g_fnvPrime = 1099511628211ULL;

/// Obfuscation seed.
constexpr uint8_t g_obfuscationSeed = 0xA7;
}// namespace

auto hashPath(const std::string& iPath) -> uint64_t {
	uint64_t hash = g_fnvBasis;
	for (const auto ch : iPath) {
		hash ^= static_cast<uint64_t>(static_cast<uint8_t>(ch));
		hash *= g_fnvPrime;
	}
	return hash;
}

void obfuscateBuffer(std::vector<uint8_t>& ioBuffer, const uint32_t iEntryIndex) {
	for (size_t i = 0; i < ioBuffer.size(); ++i) {
		const auto key = static_cast<uint8_t>((g_obfuscationSeed ^ iEntryIndex) + i * 37);
		ioBuffer[i] ^= key;
	}
}

auto compressBuffer(const std::vector<uint8_t>& iData) -> std::vector<uint8_t> {
	if (iData.empty())
		return {};
	const auto bound = ZSTD_compressBound(iData.size());
	std::vector<uint8_t> compressed(bound);
	const auto result = ZSTD_compress(compressed.data(), bound, iData.data(), iData.size(), 3);
	if (ZSTD_isError(result) != 0u)
		return {};
	compressed.resize(result);
	return compressed;
}

auto decompressBuffer(const std::vector<uint8_t>& iCompressed, const uint64_t iOriginalSize) -> std::vector<uint8_t> {
	if (iCompressed.empty() || iOriginalSize == 0)
		return {};
	std::vector<uint8_t> decompressed(iOriginalSize);
	const auto result = ZSTD_decompress(decompressed.data(), iOriginalSize, iCompressed.data(), iCompressed.size());
	if ((ZSTD_isError(result) != 0u) || result != iOriginalSize)
		return {};
	return decompressed;
}

auto serializeToc(const std::vector<TocEntry>& iEntries) -> std::vector<uint8_t> {
	std::vector<uint8_t> data;
	for (const auto& entry : iEntries) {
		// pathHash (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entry.pathHash),
					reinterpret_cast<const uint8_t*>(&entry.pathHash) + sizeof(entry.pathHash));
		// pathLength (2 bytes)
		const auto pathLen = static_cast<uint16_t>(entry.path.size());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&pathLen),
					reinterpret_cast<const uint8_t*>(&pathLen) + sizeof(pathLen));
		// path (variable)
		data.insert(data.end(), entry.path.begin(), entry.path.end());
		// dataOffset (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entry.dataOffset),
					reinterpret_cast<const uint8_t*>(&entry.dataOffset) + sizeof(entry.dataOffset));
		// dataSize (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entry.dataSize),
					reinterpret_cast<const uint8_t*>(&entry.dataSize) + sizeof(entry.dataSize));
		// originalSize (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&entry.originalSize),
					reinterpret_cast<const uint8_t*>(&entry.originalSize) + sizeof(entry.originalSize));
		// assetType (1 byte)
		data.push_back(static_cast<uint8_t>(entry.assetType));
	}
	return data;
}

auto deserializeToc(const std::vector<uint8_t>& iData) -> std::vector<TocEntry> {
	std::vector<TocEntry> entries;
	size_t offset = 0;
	while (offset < iData.size()) {
		TocEntry entry;
		// pathHash (8 bytes)
		if (offset + sizeof(uint64_t) > iData.size())
			break;
		std::memcpy(&entry.pathHash, iData.data() + offset, sizeof(uint64_t));
		offset += sizeof(uint64_t);
		// pathLength (2 bytes)
		if (offset + sizeof(uint16_t) > iData.size())
			break;
		uint16_t pathLen = 0;
		std::memcpy(&pathLen, iData.data() + offset, sizeof(uint16_t));
		offset += sizeof(uint16_t);
		// path (variable)
		if (offset + pathLen > iData.size())
			break;
		entry.path.assign(reinterpret_cast<const char*>(iData.data() + offset), pathLen);
		offset += pathLen;
		// dataOffset (8 bytes)
		if (offset + sizeof(uint64_t) > iData.size())
			break;
		std::memcpy(&entry.dataOffset, iData.data() + offset, sizeof(uint64_t));
		offset += sizeof(uint64_t);
		// dataSize (8 bytes)
		if (offset + sizeof(uint64_t) > iData.size())
			break;
		std::memcpy(&entry.dataSize, iData.data() + offset, sizeof(uint64_t));
		offset += sizeof(uint64_t);
		// originalSize (8 bytes)
		if (offset + sizeof(uint64_t) > iData.size())
			break;
		std::memcpy(&entry.originalSize, iData.data() + offset, sizeof(uint64_t));
		offset += sizeof(uint64_t);
		// assetType (1 byte)
		if (offset + 1 > iData.size())
			break;
		entry.assetType = static_cast<AssetType>(iData[offset]);
		offset += 1;
		entries.push_back(std::move(entry));
	}
	return entries;
}

}// namespace owl::io::pack
