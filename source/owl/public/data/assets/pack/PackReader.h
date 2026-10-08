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
#include <string_view>
#include <unordered_map>

namespace owl::data::assets::pack {

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
	TocOutOfBounds,///< TOC offset or size points outside the file.
	TocTooLarge,///< Declared TOC size exceeds `g_maxTocSize`.
	EntryOutOfBounds,///< An entry's data block lies outside the data area of the file.
	EntrySizeInvalid,///< An entry's stored and original sizes are inconsistent or exceed `g_maxEntrySize`.
	InvalidEntry,///< An entry has an unknown asset type.
	UnsafeEntryPath,///< An entry path is absolute or escapes the pack root (see `isSafeEntryPath`).
	UnexpectedException,///< An exception was raised while parsing; converted, never propagated.
};

/**
 * @brief
 *  Human-readable description of a pack open error.
 * @param[in] iError The error.
 * @return A short sentence fragment describing the error.
 */
[[nodiscard]] OWL_API auto describe(PackOpenError iError) -> std::string_view;

/**
 * @brief
 *  What the user can do to fix a pack open error.
 * @param[in] iError The error.
 * @return A short imperative sentence fragment, logged after `Fix:`.
 */
[[nodiscard]] OWL_API auto fixHint(PackOpenError iError) -> std::string_view;

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

	/**
	 * @brief
	 *  Move constructor.
	 */
	PackReader(PackReader&&) noexcept = default;

	auto operator=(const PackReader&) -> PackReader& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
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
	 *
	 * Every size and offset read from the file is checked against the real file size and the format limits
	 * before any allocation, and every entry path must pass `isSafeEntryPath`. Never throws.
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
	 * @return The decompressed data, or nullopt on failure (never throws).
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
	 *  Body of `tryOpen`, which may throw; `tryOpen` converts exceptions into errors.
	 * @param[in] iPackFile Path to the pack file.
	 * @return Empty success, or the reason of the failure.
	 */
	[[nodiscard]] auto openImpl(const std::filesystem::path& iPackFile) -> owl::expected<void, PackOpenError>;

	/**
	 * @brief
	 *  Check one decoded TOC entry against the file layout and the format limits.
	 * @param[in] iEntry The entry to check.
	 * @return Empty success, or the reason the entry is rejected.
	 */
	[[nodiscard]] auto validateEntry(const TocEntry& iEntry) const -> owl::expected<void, PackOpenError>;

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

}// namespace owl::data::assets::pack
