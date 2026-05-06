/**
 * @file PackWriter.cpp
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#include "io/pack/PackWriter.h"

#include <fstream>

namespace owl::io::pack {

void PackWriter::addFile(const std::filesystem::path& iSourceFile, const std::string& iPackPath,
						 const AssetType iType) {
	std::ifstream file(iSourceFile, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		OWL_CORE_WARN("Pack: addFile cannot open '{}' (skipping).", iSourceFile.string())
		return;
	}
	const auto size = static_cast<size_t>(file.tellg());
	file.seekg(0);
	std::vector<uint8_t> data(size);
	file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
	if (!file.good() && !file.eof()) {
		OWL_CORE_WARN("Pack: addFile read failure on '{}' (skipping).", iSourceFile.string())
		return;
	}
	m_entries.push_back({iPackPath, std::move(data), iType});
}

void PackWriter::addData(const std::vector<uint8_t>& iData, const std::string& iPackPath, const AssetType iType) {
	m_entries.push_back({iPackPath, iData, iType});
}

auto PackWriter::write(const std::filesystem::path& iOutputFile, const PackFlags iFlags,
					   const ProgressCallback& iProgress, const CancelCheck& iCancelCheck) const -> bool {
	std::ofstream file(iOutputFile, std::ios::binary);
	if (!file.is_open()) {
		OWL_CORE_ERROR("Pack: cannot open '{}' for writing.", iOutputFile.string())
		return false;
	}

	const bool compress = hasFlag(iFlags, PackFlags::Compressed);
	const bool obfuscate = hasFlag(iFlags, PackFlags::Obfuscated);

	// Write placeholder header (will be updated at the end).
	PackHeader header;
	header.flags = static_cast<uint16_t>(iFlags);
	header.entryCount = static_cast<uint32_t>(m_entries.size());
	file.write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));

	// Write data blocks and build TOC entries.
	std::vector<TocEntry> tocEntries;
	tocEntries.reserve(m_entries.size());

	const auto totalEntries = static_cast<uint32_t>(m_entries.size());
	for (uint32_t idx = 0; idx < totalEntries; ++idx) {
		if (iCancelCheck && iCancelCheck())
			return false;
		if (iProgress)
			iProgress(idx, totalEntries);

		const auto& entry = m_entries[idx];

		TocEntry tocEntry;
		tocEntry.pathHash = hashPath(entry.packPath);
		tocEntry.path = entry.packPath;
		tocEntry.originalSize = entry.rawData.size();
		tocEntry.assetType = entry.assetType;
		tocEntry.dataOffset = static_cast<uint64_t>(file.tellp());

		auto block = compress ? compressBuffer(entry.rawData) : entry.rawData;
		if (compress && block.empty() && !entry.rawData.empty()) {
			OWL_CORE_ERROR("Pack: compression failed on entry '{}' ({} bytes).", entry.packPath, entry.rawData.size())
			return false;
		}

		if (obfuscate)
			obfuscateBuffer(block, idx);

		tocEntry.dataSize = block.size();
		file.write(reinterpret_cast<const char*>(block.data()), static_cast<std::streamsize>(block.size()));

		tocEntries.push_back(std::move(tocEntry));
	}

	// Write TOC.
	header.tocOffset = static_cast<uint64_t>(file.tellp());
	auto tocData = serializeToc(tocEntries);
	header.tocOriginalSize = tocData.size();
	if (compress) {
		auto compressedToc = compressBuffer(tocData);
		if (compressedToc.empty() && !tocData.empty()) {
			OWL_CORE_ERROR("Pack: TOC compression failed ({} bytes).", tocData.size())
			return false;
		}
		tocData = std::move(compressedToc);
	}
	if (obfuscate)
		obfuscateBuffer(tocData, header.entryCount);

	header.tocSize = tocData.size();
	file.write(reinterpret_cast<const char*>(tocData.data()), static_cast<std::streamsize>(tocData.size()));

	// Rewrite header with final values.
	file.seekp(0);
	file.write(reinterpret_cast<const char*>(&header), sizeof(PackHeader));

	return file.good();
}

}// namespace owl::io::pack
