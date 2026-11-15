/**
 * @file ExtensionPack.h
 * @author Silmaen
 * @date 15/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "ExtensionManifest.h"
#include "PackFormat.h"
#include "PackReader.h"

#include "core/Core.h"
#include "core/expected.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace owl::data::assets::pack {

/**
 * @brief
 *  Why an extension pack is not loaded or not written.
 */
enum struct ExtensionLoadError : uint8_t {
	CannotOpen,///< Neither a readable `.owlmod` nor a folder.
	NoManifest,///< No `extension.yml` at the root of the pack.
	InvalidManifest,///< The manifest cannot be read (`ManifestError`).
	DuplicateId,///< Another pack of the same id is already loaded.
	MissingDependency,///< A dependency is not among the packs.
	DependencyTooOld,///< A dependency is older than the version asked for.
	DependencyRejected,///< A dependency was not loaded itself.
	DependencyCycle,///< The pack depends on itself through its dependencies.
	WriteFailed///< The `.owlmod` cannot be written.
};

/**
 * @brief
 *  Human-readable description of an extension load error.
 * @param[in] iError The error.
 * @return A short sentence fragment.
 */
[[nodiscard]] OWL_API auto describe(ExtensionLoadError iError) -> std::string_view;

/**
 * @brief
 *  Pack type of a file, from its extension: what the pack table of contents records for it.
 * @param[in] iPath The file.
 * @return The asset type (`AssetType::Other` for an unknown extension).
 */
[[nodiscard]] OWL_API auto getPackAssetType(const std::filesystem::path& iPath) -> AssetType;

/**
 * @brief
 *  One extension pack opened for reading: a packed `.owlmod` (the `.owlpack` format) or its source folder.
 *
 * Its entries are paths relative to the asset root, as a game pack holds them: a pack entry with the path of a game
 * asset overrides it, another one extends the game. The manifest, the `.owlmeta` sidecars and the hidden files are
 * no entries; a `Data` pack has no script entry. A folder is listed when opened, only its listed files are read.
 */
class OWL_API ExtensionPack final {
public:
	/**
	 * @brief
	 *  Default constructor: an empty pack, holding nothing.
	 */
	ExtensionPack() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~ExtensionPack();

	ExtensionPack(const ExtensionPack&) = delete;

	/**
	 * @brief
	 *  Move constructor.
	 */
	ExtensionPack(ExtensionPack&&) noexcept = default;

	auto operator=(const ExtensionPack&) -> ExtensionPack& = delete;

	/**
	 * @brief
	 *  Move assignment operator.
	 * @return A reference to this object.
	 */
	auto operator=(ExtensionPack&&) noexcept -> ExtensionPack& = default;

	/**
	 * @brief
	 *  Open an extension pack and read its manifest.
	 * @param[in] iSource A `.owlmod` file, or a folder holding `extension.yml`.
	 * @return The pack, or why it cannot be opened.
	 */
	[[nodiscard]] static auto open(const std::filesystem::path& iSource) -> expected<ExtensionPack, ExtensionLoadError>;

	/**
	 * @brief
	 *  Read the manifest of an extension pack without listing its entries.
	 * @param[in] iSource A `.owlmod` file, or a folder holding `extension.yml`.
	 * @return The manifest, or why it cannot be read.
	 */
	[[nodiscard]] static auto readManifest(const std::filesystem::path& iSource)
			-> expected<ExtensionManifest, ExtensionLoadError>;

	/**
	 * @brief
	 *  Get the manifest.
	 * @return The manifest.
	 */
	[[nodiscard]] auto getManifest() const -> const ExtensionManifest& { return m_manifest; }

	/**
	 * @brief
	 *  Get the file or folder the pack was opened from.
	 * @return The source path.
	 */
	[[nodiscard]] auto getSource() const -> const std::filesystem::path& { return m_source; }

	/**
	 * @brief
	 *  Check whether the pack is a packed `.owlmod` file (false for a source folder).
	 * @return True for a packed file.
	 */
	[[nodiscard]] auto isPacked() const -> bool { return m_reader.isOpen(); }

	/**
	 * @brief
	 *  Check whether the pack holds an entry.
	 * @param[in] iPath Asset path, relative to the asset root (lexically normalized).
	 * @return True when the entry exists.
	 */
	[[nodiscard]] auto contains(const std::string& iPath) const -> bool;

	/**
	 * @brief
	 *  Read an entry (thread-safe).
	 * @param[in] iPath Asset path, relative to the asset root (lexically normalized).
	 * @return The bytes, or nothing when the pack does not hold the entry or it cannot be read.
	 */
	[[nodiscard]] auto readEntry(const std::string& iPath) const -> std::optional<std::vector<uint8_t>>;

	/**
	 * @brief
	 *  List the entries, sorted.
	 * @return The asset paths.
	 */
	[[nodiscard]] auto listEntries() const -> const std::vector<std::string>& { return m_entries; }

private:
	/// Manifest of the pack.
	ExtensionManifest m_manifest;
	/// File or folder of the pack.
	std::filesystem::path m_source;
	/// Reader of a packed file (closed for a folder).
	PackReader m_reader;
	/// Entries, sorted.
	std::vector<std::string> m_entries;
	/// Entries, for the lookups.
	std::unordered_set<std::string> m_entrySet;
};

/**
 * @brief
 *  Write the source folder of an extension pack as a `.owlmod` file: its manifest and every entry it holds.
 * @param[in] iSourceFolder Folder holding `extension.yml`.
 * @param[in] iOutputFile The `.owlmod` written (its folder is created).
 * @param[in] iFlags Pack flags (compression, obfuscation).
 * @return The number of entries written besides the manifest, or why the pack was not written.
 */
[[nodiscard]] OWL_API auto writeExtensionPack(const std::filesystem::path& iSourceFolder,
											  const std::filesystem::path& iOutputFile,
											  PackFlags iFlags = PackFlags::Default)
		-> expected<size_t, ExtensionLoadError>;

}// namespace owl::data::assets::pack
