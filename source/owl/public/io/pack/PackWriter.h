/**
 * @file PackWriter.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PackFormat.h"

#include <filesystem>

namespace owl::io::pack {

/**
 * @brief Creates Owl pack files from assets.
 */
class OWL_API PackWriter final {
public:
	PackWriter() = default;
	~PackWriter() = default;
	PackWriter(const PackWriter&) = delete;
	PackWriter(PackWriter&&) = default;
	auto operator=(const PackWriter&) -> PackWriter& = delete;
	auto operator=(PackWriter&&) -> PackWriter& = default;

	/**
	 * @brief Add a file from disk to the pack.
	 * @param[in] iSourceFile Path to the source file.
	 * @param[in] iPackPath Relative path inside the pack.
	 * @param[in] iType The asset type.
	 */
	void addFile(const std::filesystem::path& iSourceFile, const std::string& iPackPath, AssetType iType);

	/**
	 * @brief Add raw data to the pack.
	 * @param[in] iData The raw data bytes.
	 * @param[in] iPackPath Relative path inside the pack.
	 * @param[in] iType The asset type.
	 */
	void addData(const std::vector<uint8_t>& iData, const std::string& iPackPath, AssetType iType);

	/**
	 * @brief Write the pack file to disk.
	 * @param[in] iOutputFile The output file path.
	 * @param[in] iFlags Pack flags (compression, obfuscation).
	 * @return True on success.
	 */
	[[nodiscard]] auto write(const std::filesystem::path& iOutputFile,
							 PackFlags iFlags = PackFlags::Default) const -> bool;

	/**
	 * @brief Get the number of pending entries.
	 * @return Entry count.
	 */
	[[nodiscard]] auto entryCount() const -> size_t { return m_entries.size(); }

	/**
	 * @brief Clear all pending entries.
	 */
	void clear() { m_entries.clear(); }

private:
	struct PendingEntry {
		std::string packPath;
		std::vector<uint8_t> rawData;
		AssetType assetType;
	};
	std::vector<PendingEntry> m_entries;
};

}// namespace owl::io::pack
