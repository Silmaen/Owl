/**
 * @file PackReader.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "io/pack/PackReader.h"

#include <cstring>

namespace owl::io::pack {

PackReader::~PackReader() { close(); }

auto PackReader::open(const std::filesystem::path& iPackFile) -> bool {
	close();

	m_fileStream.open(iPackFile, std::ios::binary);
	if (!m_fileStream.is_open())
		return false;

	// Read header.
	m_fileStream.read(reinterpret_cast<char*>(&m_header), sizeof(PackHeader));
	if (!m_fileStream.good()) {
		close();
		return false;
	}

	// Validate magic.
	if (m_header.magic != g_packMagic) {
		close();
		return false;
	}

	// Validate version.
	if (m_header.version != g_packVersion) {
		close();
		return false;
	}

	const auto flags = static_cast<PackFlags>(m_header.flags);
	const bool compressed = hasFlag(flags, PackFlags::Compressed);
	const bool obfuscated = hasFlag(flags, PackFlags::Obfuscated);

	// Read TOC.
	m_fileStream.seekg(static_cast<std::streamoff>(m_header.tocOffset));
	std::vector<uint8_t> tocData(m_header.tocSize);
	m_fileStream.read(reinterpret_cast<char*>(tocData.data()), static_cast<std::streamsize>(m_header.tocSize));
	if (!m_fileStream.good() && !m_fileStream.eof()) {
		close();
		return false;
	}

	if (obfuscated)
		obfuscateBuffer(tocData, m_header.entryCount);

	if (compressed && m_header.tocOriginalSize > 0) {
		auto decompressed = decompressBuffer(tocData, m_header.tocOriginalSize);
		if (decompressed.empty()) {
			close();
			return false;
		}
		tocData = std::move(decompressed);
	} else if (compressed && m_header.tocOriginalSize == 0) {
		tocData.clear();
	}

	m_toc = deserializeToc(tocData);
	if (m_toc.size() != m_header.entryCount) {
		close();
		return false;
	}

	// Build hash index.
	m_hashIndex.clear();
	m_hashIndex.reserve(m_toc.size());
	for (size_t i = 0; i < m_toc.size(); ++i) {
		m_hashIndex[m_toc[i].pathHash] = i;
	}

	return true;
}

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

	// Read the compressed/obfuscated block.
	m_fileStream.seekg(static_cast<std::streamoff>(entry->dataOffset));
	std::vector<uint8_t> block(entry->dataSize);
	m_fileStream.read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(entry->dataSize));
	if (!m_fileStream.good() && !m_fileStream.eof())
		return std::nullopt;

	if (obfuscated)
		obfuscateBuffer(block, entryIndex);

	if (compressed)
		return decompressBuffer(block, entry->originalSize);

	return block;
}

auto PackReader::listEntries() const -> std::vector<std::string> {
	std::vector<std::string> paths;
	paths.reserve(m_toc.size());
	for (const auto& entry : m_toc) {
		paths.push_back(entry.path);
	}
	return paths;
}

auto PackReader::listEntries(const AssetType iType) const -> std::vector<std::string> {
	std::vector<std::string> paths;
	for (const auto& entry : m_toc) {
		if (entry.assetType == iType)
			paths.push_back(entry.path);
	}
	return paths;
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

}// namespace owl::io::pack
