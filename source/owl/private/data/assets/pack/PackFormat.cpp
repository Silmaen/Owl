/**
 * @file PackFormat.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "data/assets/pack/PackFormat.h"
#include "core/Macros.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

OWL_DIAG_PUSH
OWL_DIAG_DISABLE_CLANG("-Wunsafe-buffer-usage")
#include <zstd.h>
OWL_DIAG_POP

namespace owl::data::assets::pack {

namespace {
// FNV-1a constants for 64-bit.
constexpr uint64_t g_fnvBasis = 14695981039346656037ULL;
constexpr uint64_t g_fnvPrime = 1099511628211ULL;

// Obfuscation seed.
constexpr uint8_t g_obfuscationSeed = 0xA7;
}// namespace

auto isSafeEntryPath(const std::string_view iPath) -> bool {
	if (iPath.empty() || iPath.front() == '/' || iPath.front() == '\\')
		return false;
	if (iPath.find_first_of(std::string_view{"\0:", 2}) != std::string_view::npos)
		return false;
	size_t start = 0;
	while (start <= iPath.size()) {
		const auto end = iPath.find_first_of("/\\", start);
		const auto component =
				iPath.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
		if (component == "..")
			return false;
		if (end == std::string_view::npos)
			break;
		start = end + 1;
	}
	return true;
}

auto hashPath(const std::string& iPath) -> uint64_t {
	uint64_t hash = g_fnvBasis;
	for (const auto ch: iPath) {
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
	if (iOriginalSize > g_maxEntrySize) {
		OWL_CORE_WARN("Pack: refusing to decompress {} bytes (limit {}).", iOriginalSize, g_maxEntrySize)
		return {};
	}
	const auto frameSize = ZSTD_getFrameContentSize(iCompressed.data(), iCompressed.size());
	if (frameSize == ZSTD_CONTENTSIZE_ERROR) {
		OWL_CORE_WARN("Pack: block is not a zstd frame.")
		return {};
	}
	try {
		if (frameSize != ZSTD_CONTENTSIZE_UNKNOWN) {
			if (frameSize != iOriginalSize) {
				OWL_CORE_WARN("Pack: zstd frame holds {} bytes, {} declared.", frameSize, iOriginalSize)
				return {};
			}
			std::vector<uint8_t> decompressed(iOriginalSize);
			const auto result =
					ZSTD_decompress(decompressed.data(), decompressed.size(), iCompressed.data(), iCompressed.size());
			if ((ZSTD_isError(result) != 0u) || result != iOriginalSize) {
				OWL_CORE_WARN("Pack: zstd decompression failed.")
				return {};
			}
			return decompressed;
		}
		// Frame without a recorded size: stream it so the output only grows with the bytes really produced.
		const uniq<ZSTD_DStream, decltype(&ZSTD_freeDStream)> stream(ZSTD_createDStream(), &ZSTD_freeDStream);
		if (stream == nullptr) {
			OWL_CORE_WARN("Pack: cannot create a zstd stream.")
			return {};
		}
		std::vector<uint8_t> decompressed(std::min<uint64_t>(iOriginalSize, ZSTD_DStreamOutSize()));
		ZSTD_inBuffer input{.src = iCompressed.data(), .size = iCompressed.size(), .pos = 0};
		ZSTD_outBuffer output{.dst = decompressed.data(), .size = decompressed.size(), .pos = 0};
		size_t pending = 1;
		while (pending != 0) {
			if (output.pos == output.size) {
				if (output.size == iOriginalSize) {
					OWL_CORE_WARN("Pack: zstd frame exceeds its declared {} bytes.", iOriginalSize)
					return {};
				}
				decompressed.resize(std::min<uint64_t>(iOriginalSize, decompressed.size() * 2));
				output.dst = decompressed.data();
				output.size = decompressed.size();
			}
			pending = ZSTD_decompressStream(stream.get(), &output, &input);
			if (ZSTD_isError(pending) != 0u) {
				OWL_CORE_WARN("Pack: zstd stream decompression failed.")
				return {};
			}
			if (pending != 0 && input.pos == input.size && output.pos < output.size) {
				OWL_CORE_WARN("Pack: truncated zstd frame.")
				return {};
			}
		}
		if (output.pos != iOriginalSize || input.pos != input.size) {
			OWL_CORE_WARN("Pack: zstd frame holds {} bytes, {} declared.", output.pos, iOriginalSize)
			return {};
		}
		return decompressed;
	} catch (const std::bad_alloc&) {
		OWL_CORE_WARN("Pack: out of memory decompressing {} bytes.", iOriginalSize)
		return {};
	}
}

auto serializeToc(const std::vector<TocEntry>& iEntries) -> std::vector<uint8_t> {
	std::vector<uint8_t> data;
	for (const auto& [pathHash, path, dataOffset, dataSize, originalSize, assetType]: iEntries) {
		// pathHash (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&pathHash),
					reinterpret_cast<const uint8_t*>(&pathHash) + sizeof(pathHash));
		// pathLength (2 bytes)
		const auto pathLen = static_cast<uint16_t>(path.size());
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&pathLen),
					reinterpret_cast<const uint8_t*>(&pathLen) + sizeof(pathLen));
		// path (variable)
		data.insert(data.end(), path.begin(), path.end());
		// dataOffset (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&dataOffset),
					reinterpret_cast<const uint8_t*>(&dataOffset) + sizeof(dataOffset));
		// dataSize (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&dataSize),
					reinterpret_cast<const uint8_t*>(&dataSize) + sizeof(dataSize));
		// originalSize (8 bytes)
		data.insert(data.end(), reinterpret_cast<const uint8_t*>(&originalSize),
					reinterpret_cast<const uint8_t*>(&originalSize) + sizeof(originalSize));
		// assetType (1 byte)
		data.push_back(static_cast<uint8_t>(assetType));
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

}// namespace owl::data::assets::pack
