/**
 * @file PackReader.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "data/assets/pack/PackReader.h"

#include <cstring>
#include <string_view>

namespace owl::data::assets::pack {

auto describe(const PackOpenError iError) -> std::string_view {
	switch (iError) {
		case PackOpenError::CannotOpenFile:
			return "the file is missing or not readable";
		case PackOpenError::ShortHeader:
			return "the file is truncated";
		case PackOpenError::InvalidMagic:
			return "the file is not an Owl pack";
		case PackOpenError::UnsupportedVersion:
			return "the pack was written by another Owl version";
		case PackOpenError::TocReadFailed:
		case PackOpenError::TocDecompressionFailed:
		case PackOpenError::TocSizeMismatch:
		case PackOpenError::TocOutOfBounds:
		case PackOpenError::TocTooLarge:
			return "the table of contents is corrupted";
		case PackOpenError::EntryOutOfBounds:
		case PackOpenError::EntrySizeInvalid:
		case PackOpenError::InvalidEntry:
			return "an entry is corrupted";
		case PackOpenError::UnsafeEntryPath:
			return "an entry path leaves the pack folder";
		case PackOpenError::UnexpectedException:
			return "the pack could not be parsed";
	}
	return "unknown error";
}

auto fixHint(const PackOpenError iError) -> std::string_view {
	switch (iError) {
		case PackOpenError::CannotOpenFile:
			return "check `PackFile` in `runner.yml` and that the pack sits next to the runner";
		case PackOpenError::UnsupportedVersion:
			return "pack the game again with this Owl version (Owl Nest, Project > Pack Game)";
		case PackOpenError::ShortHeader:
		case PackOpenError::InvalidMagic:
		case PackOpenError::TocReadFailed:
		case PackOpenError::TocDecompressionFailed:
		case PackOpenError::TocSizeMismatch:
		case PackOpenError::TocOutOfBounds:
		case PackOpenError::TocTooLarge:
		case PackOpenError::EntryOutOfBounds:
		case PackOpenError::EntrySizeInvalid:
		case PackOpenError::InvalidEntry:
		case PackOpenError::UnsafeEntryPath:
		case PackOpenError::UnexpectedException:
			return "copy the pack again or pack the game again (Owl Nest, Project > Pack Game)";
	}
	return "pack the game again";
}

PackReader::~PackReader() { close(); }

auto PackReader::tryOpen(const std::filesystem::path& iPackFile) -> owl::expected<void, PackOpenError> {
	try {
		auto result = openImpl(iPackFile);
		if (!result)
			close();
		return result;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("Pack: exception while opening '{}': {}.", iPackFile.string(), e.what())
		close();
		return owl::unexpected{PackOpenError::UnexpectedException};
	}
}

auto PackReader::openImpl(const std::filesystem::path& iPackFile) -> owl::expected<void, PackOpenError> {
	close();

	m_fileStream.open(iPackFile, std::ios::binary);
	if (!m_fileStream.is_open()) {
		OWL_CORE_WARN("Pack: cannot open '{}' for reading.", iPackFile.string())
		return owl::unexpected{PackOpenError::CannotOpenFile};
	}
	m_fileStream.seekg(0, std::ios::end);
	const auto endPos = static_cast<std::streamoff>(m_fileStream.tellg());
	m_fileStream.seekg(0, std::ios::beg);
	if (endPos < static_cast<std::streamoff>(sizeof(PackHeader))) {
		OWL_CORE_ERROR("Pack: '{}' is shorter than a pack header.", iPackFile.string())
		return owl::unexpected{PackOpenError::ShortHeader};
	}
	const auto fileSize = static_cast<uint64_t>(endPos);

	m_fileStream.read(reinterpret_cast<char*>(&m_header), sizeof(PackHeader));
	if (!m_fileStream.good()) {
		OWL_CORE_ERROR("Pack: short read on '{}' header.", iPackFile.string())
		return owl::unexpected{PackOpenError::ShortHeader};
	}

	if (m_header.magic != g_packMagic) {
		OWL_CORE_ERROR("Pack: '{}' has invalid magic header.", iPackFile.string())
		return owl::unexpected{PackOpenError::InvalidMagic};
	}

	if (m_header.version != g_packVersion) {
		OWL_CORE_ERROR("Pack: '{}' has unsupported version {} (current {}).", iPackFile.string(), m_header.version,
					   g_packVersion)
		return owl::unexpected{PackOpenError::UnsupportedVersion};
	}

	if (m_header.tocOffset < sizeof(PackHeader) || m_header.tocOffset > fileSize ||
		m_header.tocSize > fileSize - m_header.tocOffset) {
		OWL_CORE_ERROR("Pack: '{}' TOC ({} bytes at {}) lies outside the {}-byte file.", iPackFile.string(),
					   m_header.tocSize, m_header.tocOffset, fileSize)
		return owl::unexpected{PackOpenError::TocOutOfBounds};
	}

	const auto flags = static_cast<PackFlags>(m_header.flags);
	const bool compressed = hasFlag(flags, PackFlags::Compressed);
	const bool obfuscated = hasFlag(flags, PackFlags::Obfuscated);

	if (const auto declaredToc = compressed ? m_header.tocOriginalSize : m_header.tocSize; declaredToc > g_maxTocSize) {
		OWL_CORE_ERROR("Pack: '{}' declares a {}-byte TOC (limit {}).", iPackFile.string(), declaredToc, g_maxTocSize)
		return owl::unexpected{PackOpenError::TocTooLarge};
	}

	m_fileStream.seekg(static_cast<std::streamoff>(m_header.tocOffset));
	std::vector<uint8_t> tocData(m_header.tocSize);
	m_fileStream.read(reinterpret_cast<char*>(tocData.data()), static_cast<std::streamsize>(tocData.size()));
	if (static_cast<uint64_t>(m_fileStream.gcount()) != m_header.tocSize) {
		OWL_CORE_ERROR("Pack: TOC read failed on '{}'.", iPackFile.string())
		return owl::unexpected{PackOpenError::TocReadFailed};
	}

	if (obfuscated)
		obfuscateBuffer(tocData, m_header.entryCount);

	if (compressed && m_header.tocOriginalSize > 0) {
		auto decompressed = decompressBuffer(tocData, m_header.tocOriginalSize);
		if (decompressed.empty()) {
			OWL_CORE_ERROR("Pack: TOC decompression failed on '{}'.", iPackFile.string())
			return owl::unexpected{PackOpenError::TocDecompressionFailed};
		}
		tocData = std::move(decompressed);
	} else if (compressed && m_header.tocOriginalSize == 0) {
		tocData.clear();
	}

	m_toc = deserializeToc(tocData);
	if (m_toc.size() != m_header.entryCount) {
		OWL_CORE_ERROR("Pack: TOC entry count mismatch on '{}' ({} read, {} expected).", iPackFile.string(),
					   m_toc.size(), m_header.entryCount)
		return owl::unexpected{PackOpenError::TocSizeMismatch};
	}

	for (const auto& entry: m_toc) {
		if (auto valid = validateEntry(entry); !valid) {
			OWL_CORE_ERROR("Pack: '{}' rejected, invalid entry '{}'.", iPackFile.string(), entry.path)
			return valid;
		}
	}

	m_hashIndex.clear();
	m_hashIndex.reserve(m_toc.size());
	for (size_t i = 0; i < m_toc.size(); ++i) { m_hashIndex[m_toc[i].pathHash] = i; }

	return {};
}

auto PackReader::validateEntry(const TocEntry& iEntry) const -> owl::expected<void, PackOpenError> {
	if (!isSafeEntryPath(iEntry.path)) {
		OWL_CORE_ERROR("Pack: entry path '{}' is absolute or escapes the pack root.", iEntry.path)
		return owl::unexpected{PackOpenError::UnsafeEntryPath};
	}
	if (static_cast<uint8_t>(iEntry.assetType) > static_cast<uint8_t>(AssetType::Script)) {
		OWL_CORE_ERROR("Pack: entry '{}' has unknown asset type {}.", iEntry.path,
					   static_cast<uint8_t>(iEntry.assetType))
		return owl::unexpected{PackOpenError::InvalidEntry};
	}
	// The writer lays data blocks out between the header and the TOC.
	if (iEntry.dataOffset < sizeof(PackHeader) || iEntry.dataOffset > m_header.tocOffset ||
		iEntry.dataSize > m_header.tocOffset - iEntry.dataOffset) {
		OWL_CORE_ERROR("Pack: entry '{}' ({} bytes at {}) lies outside the data area.", iEntry.path, iEntry.dataSize,
					   iEntry.dataOffset)
		return owl::unexpected{PackOpenError::EntryOutOfBounds};
	}
	const bool compressed = hasFlag(static_cast<PackFlags>(m_header.flags), PackFlags::Compressed);
	const bool sizesConsistent =
			compressed ? (iEntry.dataSize == 0) == (iEntry.originalSize == 0) : iEntry.dataSize == iEntry.originalSize;
	if (iEntry.originalSize > g_maxEntrySize || !sizesConsistent) {
		OWL_CORE_ERROR("Pack: entry '{}' has inconsistent sizes ({} stored, {} original).", iEntry.path,
					   iEntry.dataSize, iEntry.originalSize)
		return owl::unexpected{PackOpenError::EntrySizeInvalid};
	}
	return {};
}

auto PackReader::open(const std::filesystem::path& iPackFile) -> bool { return tryOpen(iPackFile).has_value(); }

void PackReader::close() {
	if (m_fileStream.is_open())
		m_fileStream.close();
	m_toc.clear();
	m_hashIndex.clear();
	m_header = {};
}

auto PackReader::contains(const std::string& iPath) const -> bool { return findEntry(iPath) != nullptr; }

auto PackReader::readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>> {
	const auto* entry = findEntry(iPath);
	if (entry == nullptr)
		return std::nullopt;

	const auto flags = static_cast<PackFlags>(m_header.flags);
	const bool compressed = hasFlag(flags, PackFlags::Compressed);
	const bool obfuscated = hasFlag(flags, PackFlags::Obfuscated);

	// Find the entry index for obfuscation key.
	const auto it = m_hashIndex.find(entry->pathHash);
	if (it == m_hashIndex.end())
		return std::nullopt;
	const auto entryIndex = static_cast<uint32_t>(it->second);

	try {
		// Offsets and sizes were bounded by the file size in `tryOpen`.
		m_fileStream.clear();
		m_fileStream.seekg(static_cast<std::streamoff>(entry->dataOffset));
		std::vector<uint8_t> block(entry->dataSize);
		m_fileStream.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(block.size()));
		if (static_cast<uint64_t>(m_fileStream.gcount()) != entry->dataSize) {
			OWL_CORE_WARN("Pack: short read on entry '{}'.", iPath)
			return std::nullopt;
		}

		if (obfuscated)
			obfuscateBuffer(block, entryIndex);

		if (!compressed)
			return block;
		if (entry->originalSize == 0)
			return std::vector<uint8_t>{};
		auto data = decompressBuffer(block, entry->originalSize);
		if (data.empty()) {
			OWL_CORE_WARN("Pack: decompression failed on entry '{}'.", iPath)
			return std::nullopt;
		}
		return data;
	} catch (const std::exception& e) {
		OWL_CORE_ERROR("Pack: exception while reading entry '{}': {}.", iPath, e.what())
		return std::nullopt;
	}
}

auto PackReader::listEntries() const -> std::vector<std::string> {
	std::vector<std::string> paths;
	paths.reserve(m_toc.size());
	for (const auto& entry: m_toc) { paths.push_back(entry.path); }
	return paths;
}

auto PackReader::listEntries(const AssetType iType) const -> std::vector<std::string> {
	std::vector<std::string> paths;
	for (const auto& entry: m_toc) {
		if (entry.assetType == iType)
			paths.push_back(entry.path);
	}
	return paths;
}

auto PackReader::entrySize(const std::string& iPath) const -> std::optional<uint64_t> {
	const auto* entry = findEntry(iPath);
	if (entry == nullptr)
		return std::nullopt;
	return entry->originalSize;
}

auto PackReader::findEntry(const std::string& iPath) const -> const TocEntry* {
	const auto hash = hashPath(iPath);
	const auto it = m_hashIndex.find(hash);
	if (it == m_hashIndex.end())
		return nullptr;
	const auto& entry = m_toc[it->second];
	// Confirm path matches (hash collision guard).
	if (entry.path != iPath)
		return nullptr;
	return &entry;
}

}// namespace owl::data::assets::pack
