/**
 * @file PackExtractor.h
 * @author Silmaen
 * @date 05/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PackReader.h"

#include "core/expected.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace owl::data::assets::pack {

/**
 * @brief
 *  Categorised reasons a pack extraction may fail.
 */
enum struct PackExtractError : uint8_t {
	NotOpen,///< The reader has no open pack.
	UnsafeEntryPath,///< An entry would land outside the destination directory.
	ReadFailed,///< An entry could not be read or decompressed from the pack.
	CreateDirectoryFailed,///< A destination directory could not be created.
	WriteFailed,///< A destination file could not be opened or fully written.
	UnexpectedException,///< An exception was raised; converted, never propagated.
};

/**
 * @brief
 *  Outcome of a successful pack extraction.
 */
struct PackExtractStats {
	/// Number of entries written to disk.
	size_t written = 0;
	/// Number of entries skipped because an up-to-date file already existed.
	size_t skipped = 0;
};

/**
 * @brief
 *  Resolve where a pack entry lands under a root directory, refusing any path that escapes it.
 *
 * The entry must pass `isSafeEntryPath`; the result is then resolved through existing symbolic links and
 * must still lie strictly inside the resolved root.
 * @param[in] iRoot The destination root directory.
 * @param[in] iEntryPath The entry path as stored in the TOC.
 * @return The confined destination path, or nullopt when the entry would escape the root.
 */
OWL_API auto resolveEntryPath(const std::filesystem::path& iRoot, std::string_view iEntryPath)
		-> std::optional<std::filesystem::path>;

/**
 * @brief
 *  Write a buffer to a file, creating its parent directories, and check that every byte reached the disk.
 * @param[in] iFile The destination file.
 * @param[in] iData The bytes to write.
 * @return Empty success, or the reason the write failed.
 */
OWL_API auto writeEntryFile(const std::filesystem::path& iFile, std::span<const uint8_t> iData)
		-> owl::expected<void, PackExtractError>;

/**
 * @brief
 *  Extract every entry of an open pack under a destination directory.
 *
 * Entries whose destination already has the entry's size are skipped, as are `.slang` sources whose `.spv`
 * cache exists. Stops at the first failure.
 * @param[in] iReader The open pack reader.
 * @param[in] iDestDir The destination directory (created if missing).
 * @return The extraction counters, or the reason of the first failure.
 */
OWL_API auto extractPack(const PackReader& iReader, const std::filesystem::path& iDestDir)
		-> owl::expected<PackExtractStats, PackExtractError>;

}// namespace owl::data::assets::pack
