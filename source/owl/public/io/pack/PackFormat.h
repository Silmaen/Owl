/**
 * @file PackFormat.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace owl::io::pack {

/// Magic bytes identifying an Owl pack file.
constexpr std::array<char, 4> g_packMagic = {'O', 'W', 'L', 'P'};

/// Current pack format version.
constexpr uint8_t g_packVersion = 1;

/// Pack file flags.
enum struct PackFlags : uint8_t {
	None = 0,
	Compressed = 1 << 0,
	Obfuscated = 1 << 1,
	Default = Compressed | Obfuscated,
};

/// Bitwise OR for PackFlags.
constexpr auto operator|(const PackFlags iLhs, const PackFlags iRhs) -> PackFlags {
	return static_cast<PackFlags>(static_cast<uint16_t>(iLhs) | static_cast<uint16_t>(iRhs));
}

/// Bitwise AND for PackFlags.
constexpr auto operator&(const PackFlags iLhs, const PackFlags iRhs) -> PackFlags {
	return static_cast<PackFlags>(static_cast<uint16_t>(iLhs) & static_cast<uint16_t>(iRhs));
}

/// Check if a flag is set.
constexpr auto hasFlag(const PackFlags iFlags, const PackFlags iFlag) -> bool {
	return (iFlags & iFlag) == iFlag;
}

/// Type of asset stored in the pack.
enum struct AssetType : uint8_t {
	Scene = 0,
	Texture = 1,
	Font = 2,
	Sound = 3,
	Mesh = 4,
	Other = 5,
};

/// Pack file header (40 bytes, fixed size).
struct PackHeader {
	std::array<char, 4> magic = g_packMagic;
	uint16_t version = g_packVersion;
	uint16_t flags = static_cast<uint16_t>(PackFlags::Default);
	uint32_t entryCount = 0;
	uint32_t reserved = 0;
	uint64_t tocOffset = 0;
	uint64_t tocSize = 0;
	uint64_t tocOriginalSize = 0;
};
static_assert(sizeof(PackHeader) == 40, "PackHeader must be 40 bytes");

/// Table of contents entry.
struct TocEntry {
	uint64_t pathHash = 0;
	std::string path;
	uint64_t dataOffset = 0;
	uint64_t dataSize = 0;
	uint64_t originalSize = 0;
	AssetType assetType = AssetType::Other;
};

/**
 * @brief Compute FNV-1a 64-bit hash of a path string.
 * @param[in] iPath The path to hash.
 * @return The hash value.
 */
OWL_API auto hashPath(const std::string& iPath) -> uint64_t;

/**
 * @brief Obfuscate or deobfuscate a buffer in-place (XOR, symmetric).
 * @param[in,out] ioBuffer The buffer to transform.
 * @param[in] iEntryIndex Entry index used as part of the rolling key.
 */
OWL_API void obfuscateBuffer(std::vector<uint8_t>& ioBuffer, uint32_t iEntryIndex);

/**
 * @brief Compress a buffer using zstd.
 * @param[in] iData The raw data to compress.
 * @return The compressed data, or empty on failure.
 */
OWL_API auto compressBuffer(const std::vector<uint8_t>& iData) -> std::vector<uint8_t>;

/**
 * @brief Decompress a zstd-compressed buffer.
 * @param[in] iCompressed The compressed data.
 * @param[in] iOriginalSize The expected original size.
 * @return The decompressed data, or empty on failure.
 */
OWL_API auto decompressBuffer(const std::vector<uint8_t>& iCompressed, uint64_t iOriginalSize) -> std::vector<uint8_t>;

/**
 * @brief Serialize TOC entries to binary.
 * @param[in] iEntries The entries to serialize.
 * @return The serialized binary data.
 */
OWL_API auto serializeToc(const std::vector<TocEntry>& iEntries) -> std::vector<uint8_t>;

/**
 * @brief Deserialize TOC entries from binary.
 * @param[in] iData The binary data.
 * @return The deserialized entries, or empty on failure.
 */
OWL_API auto deserializeToc(const std::vector<uint8_t>& iData) -> std::vector<TocEntry>;

}// namespace owl::io::pack
