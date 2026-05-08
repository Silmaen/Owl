/**
 * @file PackReader.h
 * @author Silmaen
 * @date 09/03/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "PackFormat.h"

#include "core/expected.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>

namespace owl::io::pack {

/**
 * @brief
 *  Categorised reasons a `PackReader::tryOpen` may fail.
 */
enum struct PackOpenError : uint8_t {
	CannotOpenFile,///< The OS refused to open the file (missing / not readable).
	ShortHeader,///< File is shorter than a `PackHeader` or read interrupted.
	InvalidMagic,///< Magic-bytes mismatch — file is not an `.owlpack`.
	UnsupportedVersion,///< Pack version not supported by this engine build.
	TocReadFailed,///< I/O error while reading the table of contents.
	TocDecompressionFailed,///< zstd decompression of the TOC failed.
	TocSizeMismatch,///< Decoded TOC entry count differs from the header's claim.
};

/**
 * @brief
 *  Reads Owl pack files at runtime.
 */
class OWL_API PackReader final {
public:
	PackReader() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~PackReader();

	PackReader(const PackReader&) = delete;

	PackReader(PackReader&&) noexcept = default;

	auto operator=(const PackReader&) -> PackReader& = delete;

	auto operator=(PackReader&&) noexcept -> PackReader& = default;

	/**
	 * @brief
	 *  Open a pack file and read its table of contents.
	 *
	 * Convenience wrapper over `tryOpen` that discards the failure reason. Prefer `tryOpen`
	 * when the caller needs to react to the specific error.
	 * @param[in] iPackFile Path to the pack file.
	 * @return True if the pack was opened successfully.
	 */
	[[nodiscard]] auto open(const std::filesystem::path& iPackFile) -> bool;

	/**
	 * @brief
	 *  Open a pack file and read its table of contents, returning a categorised error on failure.
	 * @param[in] iPackFile Path to the pack file.
	 * @return Empty success on success, or `owl::unexpected{PackOpenError::*}` on failure.
	 */
	[[nodiscard]] auto tryOpen(const std::filesystem::path& iPackFile) -> owl::expected<void, PackOpenError>;

	/**
	 * @brief
	 *  Close the pack file.
	 */
	void close();

	/**
	 * @brief
	 *  Check if the pack contains an entry.
	 * @param[in] iPath The asset path to look up.
	 * @return True if the entry exists.
	 */
	[[nodiscard]] auto contains(const std::string& iPath) const -> bool;

	/**
	 * @brief
	 *  Read and decompress an entry by path.
	 * @param[in] iPath The asset path.
	 * @return The decompressed data, or nullopt on failure.
	 */
	[[nodiscard]] auto readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>>;

	/**
	 * @brief
	 *  List all entry paths in the pack.
	 * @return Vector of all paths.
	 */
	[[nodiscard]] auto listEntries() const -> std::vector<std::string>;

	/**
	 * @brief
	 *  List entries of a specific asset type.
	 * @param[in] iType The asset type to filter.
	 * @return Vector of matching paths.
	 */
	[[nodiscard]] auto listEntries(AssetType iType) const -> std::vector<std::string>;

	/**
	 * @brief
	 *  Get the original (uncompressed) size of an entry.
	 * @param[in] iPath The asset path.
	 * @return The original size in bytes, or nullopt if the entry is not found.
	 */
	[[nodiscard]] auto entrySize(const std::string& iPath) const -> std::optional<uint64_t>;

	/**
	 * @brief
	 *  Check if a pack file is currently open.
	 * @return True if open.
	 */
	[[nodiscard]] auto isOpen() const -> bool { return m_fileStream.is_open(); }

	/**
	 * @brief
	 *  Get the pack header.
	 * @return The header.
	 */
	[[nodiscard]] auto getHeader() const -> const PackHeader& { return m_header; }

private:
	/**
	 * @brief
	 *  Find a TOC entry by path.
	 * @param[in] iPath The path to search.
	 * @return Pointer to the entry, or nullptr.
	 */
	[[nodiscard]] auto findEntry(const std::string& iPath) const -> const TocEntry*;

	/// Open pack file stream — `mutable` so const lookups can re-seek for blob reads.
	mutable std::ifstream m_fileStream;
	/// Cached pack header (magic, version, TOC offset/size).
	PackHeader m_header{};
	/// Table of contents — one entry per packed asset.
	std::vector<TocEntry> m_toc;
	/// Path-hash → TOC index, for O(1) `loadEntry()` lookups.
	std::unordered_map<uint64_t, size_t> m_hashIndex;
};

}// namespace owl::io::pack
