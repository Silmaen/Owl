/**
 * @file AssetRegistry.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/assets/AssetMeta.h"
#include "data/assets/AssetReference.h"

#include <cstddef>
#include <filesystem>
#include <list>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace owl::data::assets {

/**
 * @brief
 *  GUID of every asset of the asset directories, read from the `.owlmeta` sidecars.
 *
 * An editable directory (a project opened in Owl Nest) gets a sidecar for each asset that has none; a read-only one
 * (engine assets, a game run from its folder) only reads the sidecars it holds. A game run from a pack fills the
 * registry from the pack index instead. Lookups are safe from any thread.
 */
class OWL_API AssetRegistry final {
public:
	/**
	 * @brief
	 *  One registered asset.
	 */
	struct Entry {
		/// Content of its sidecar.
		AssetMeta meta;
		/// Asset directory holding it; empty for an entry of a pack index.
		std::filesystem::path root;
		/// Path relative to the asset directory (or to the pack), with `/` separators.
		std::string path;
	};

	/**
	 * @brief
	 *  What a directory scan found.
	 */
	struct ScanReport {
		/// Assets registered.
		size_t assets{0};
		/// Sidecars created for assets that had none.
		size_t created{0};
		/// Copies of an asset given a new GUID (a file copied with its sidecar).
		size_t reassigned{0};
		/// Sidecars whose asset file is missing.
		size_t orphans{0};
	};

	/**
	 * @brief
	 *  Default constructor.
	 */
	AssetRegistry();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~AssetRegistry();

	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	AssetRegistry(const AssetRegistry&) = delete;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	AssetRegistry(AssetRegistry&&) = delete;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const AssetRegistry&) -> AssetRegistry& = delete;

	/**
	 * @brief
	 *  Deleted move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(AssetRegistry&&) -> AssetRegistry& = delete;

	/**
	 * @brief
	 *  Scan an asset directory and register its assets; it takes priority over the directories added before.
	 *
	 * Hidden folders (`.owl/` cache, `.git/`) are skipped.
	 * @param[in] iRoot The asset directory.
	 * @param[in] iEditable True to create the missing sidecars and fix duplicated GUIDs on disk.
	 * @return What the scan found.
	 */
	auto addDirectory(const std::filesystem::path& iRoot, bool iEditable) -> ScanReport;

	/**
	 * @brief
	 *  Forget the assets of an asset directory.
	 * @param[in] iRoot The asset directory.
	 */
	void removeDirectory(const std::filesystem::path& iRoot);

	/**
	 * @brief
	 *  Register a file that appeared in an asset directory, creating its sidecar in an editable one.
	 * @param[in] iFile Absolute path of the file.
	 * @return Its GUID, or nullopt when the file is no asset or lies in no directory of the registry.
	 */
	auto registerFile(const std::filesystem::path& iFile) -> std::optional<AssetGuid>;

	/**
	 * @brief
	 *  Forget a file that left its asset directory (its sidecar stays on disk).
	 * @param[in] iFile Absolute path of the file.
	 */
	void forgetFile(const std::filesystem::path& iFile);

	/**
	 * @brief
	 *  Rename or move an asset file with its sidecar; its GUID, hence every reference to it, is kept.
	 *
	 * A folder is moved with every sidecar inside it.
	 * @param[in] iFrom Current absolute path.
	 * @param[in] iTo New absolute path.
	 * @return True when the file was moved.
	 */
	auto moveFile(const std::filesystem::path& iFrom, const std::filesystem::path& iTo) -> bool;

	/**
	 * @brief
	 *  Delete an asset file (or a folder) with its sidecars.
	 * @param[in] iFile Absolute path.
	 * @return True when the file was deleted.
	 */
	auto deleteFile(const std::filesystem::path& iFile) -> bool;

	/**
	 * @brief
	 *  Copy an asset file; the copy gets a new GUID and the import settings of the original.
	 * @param[in] iFrom Absolute path of the original.
	 * @param[in] iTo Absolute path of the copy.
	 * @return The GUID of the copy, or nullopt when the copy failed.
	 */
	auto copyFile(const std::filesystem::path& iFrom, const std::filesystem::path& iTo) -> std::optional<AssetGuid>;

	/**
	 * @brief
	 *  Replace the import settings of an asset and write its sidecar.
	 * @param[in] iMeta The new meta; its GUID names the asset.
	 * @return True when the sidecar was written (the asset lies in an editable directory).
	 */
	auto setMeta(const AssetMeta& iMeta) -> bool;

	/**
	 * @brief
	 *  Register the entries of a pack index (`guid: path` pairs written by `serializeIndex`).
	 * @param[in] iText YAML text of the index.
	 * @return The number of entries registered.
	 */
	auto loadIndex(std::string_view iText) -> size_t;

	/**
	 * @brief
	 *  Write the pack index of some assets.
	 * @param[in] iPaths Paths of the packed assets, relative to their asset directory.
	 * @return YAML text mapping the GUID of each known asset to its path.
	 */
	[[nodiscard]] auto serializeIndex(const std::vector<std::string>& iPaths) const -> std::string;

	/**
	 * @brief
	 *  Find an asset by GUID.
	 * @param[in] iGuid The GUID.
	 * @return The asset, or nullopt when unknown.
	 */
	[[nodiscard]] auto find(AssetGuid iGuid) const -> std::optional<Entry>;

	/**
	 * @brief
	 *  Find the GUID of an asset path.
	 * @param[in] iPath Path relative to an asset directory (the first directory holding it wins) or absolute.
	 * @return The GUID, or nullopt when the path is no registered asset.
	 */
	[[nodiscard]] auto findGuid(std::string_view iPath) const -> std::optional<AssetGuid>;

	/**
	 * @brief
	 *  Turn a serialized reference into the path the loaders search.
	 * @param[in] iReference `gid:<guid>:<hint>` or a plain path.
	 * @return The current path of a known GUID, the hint of an unknown one, a plain path unchanged.
	 */
	[[nodiscard]] auto toAssetPath(std::string_view iReference) const -> std::string;

	/**
	 * @brief
	 *  Turn an asset path into the reference to serialize.
	 * @param[in] iPath Path relative to an asset directory or absolute.
	 * @return `gid:<guid>:<relative path>` for a registered asset, the path unchanged otherwise.
	 */
	[[nodiscard]] auto toAssetReference(std::string_view iPath) const -> std::string;

	/**
	 * @brief
	 *  Snapshot of every registered asset.
	 * @return The entries, in no particular order.
	 */
	[[nodiscard]] auto getEntries() const -> std::vector<Entry>;

	/**
	 * @brief
	 *  Get the number of registered assets.
	 * @return The asset count.
	 */
	[[nodiscard]] auto size() const -> size_t;

	/**
	 * @brief
	 *  Forget every asset and directory.
	 */
	void clear();

private:
	/**
	 * @brief
	 *  A scanned asset directory.
	 */
	struct Root {
		/// Absolute path of the directory.
		std::filesystem::path path;
		/// True when sidecars may be written in it.
		bool editable{false};
	};

	/**
	 * @brief
	 *  Find the directory holding a file (caller holds the lock).
	 * @param[in] iFile Absolute path.
	 * @return The directory, or nullptr.
	 */
	[[nodiscard]] auto rootOf(const std::filesystem::path& iFile) const -> const Root*;

	/**
	 * @brief
	 *  Register one file of a directory (caller holds the unique lock).
	 * @param[in] iRoot The directory holding it.
	 * @param[in] iFile Absolute path of the file.
	 * @param[in,out] ioReport Counters of the scan.
	 * @return Its GUID, or nullopt when it is no asset or has no usable sidecar.
	 */
	auto registerLocked(const Root& iRoot, const std::filesystem::path& iFile, ScanReport& ioReport)
			-> std::optional<AssetGuid>;

	/**
	 * @brief
	 *  Rebuild the path index from the entries, in directory priority order (caller holds the unique lock).
	 */
	void rebuildPathIndex();

	/// Guards every member.
	mutable std::shared_mutex m_mutex;
	/// Scanned directories, highest priority first.
	std::list<Root> m_roots;
	/// Assets by GUID.
	std::unordered_map<AssetGuid, Entry> m_entries;
	/// GUID by relative path (highest priority directory wins).
	std::unordered_map<std::string, AssetGuid> m_byRelativePath;
	/// GUID by absolute path, `/` separators.
	std::unordered_map<std::string, AssetGuid> m_byAbsolutePath;
};

}// namespace owl::data::assets
