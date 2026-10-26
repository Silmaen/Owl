/**
 * @file CookCache.h
 * @author Silmaen
 * @date 26/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "core/expected.h"
#include "data/assets/AssetMeta.h"
#include "data/assets/cook/AssetCooker.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::data::assets::cook {

/**
 * @brief
 *  Content-addressed store of cooked assets: an asset is cooked again only when its bytes, its import settings or
 *  its cooker version change.
 *
 * A cooked result lives in `<directory>/<2 hex>/<key>.owlc`, the key being the XXH3-128 hash of the source bytes,
 * the importer, its version and its settings. A manifest (`manifest.yml`) remembers the size, write time and key of
 * every source, so an unchanged file is not even hashed again. Safe from several threads.
 */
class OWL_API CookCache final {
public:
	/**
	 * @brief
	 *  Counters of the cache since its creation.
	 */
	struct Stats {
		/// Assets served from the cache.
		size_t hits{0};
		/// Assets cooked and stored.
		size_t cooked{0};
		/// Assets whose cook failed.
		size_t failed{0};
	};

	/**
	 * @brief
	 *  Open the cache of a directory (`<project>/.owl/cache/cooked`), reading its manifest.
	 * @param[in] iDirectory The cache directory, created when missing.
	 */
	explicit CookCache(std::filesystem::path iDirectory);

	/**
	 * @brief
	 *  Destructor, writes the manifest.
	 */
	~CookCache();

	/**
	 * @brief
	 *  Deleted copy constructor.
	 */
	CookCache(const CookCache&) = delete;

	/**
	 * @brief
	 *  Deleted move constructor.
	 */
	CookCache(CookCache&&) = delete;

	/**
	 * @brief
	 *  Deleted copy affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(const CookCache&) -> CookCache& = delete;

	/**
	 * @brief
	 *  Deleted move affectation operator.
	 * @return A reference to this object.
	 */
	auto operator=(CookCache&&) -> CookCache& = delete;

	/**
	 * @brief
	 *  Key of a cooked result.
	 * @param[in] iSource Bytes of the source.
	 * @param[in] iMeta Importer and settings of the asset.
	 * @return 32 hexadecimal digits.
	 */
	[[nodiscard]] static auto computeKey(std::span<const uint8_t> iSource, const AssetMeta& iMeta) -> std::string;

	/**
	 * @brief
	 *  Cooked bytes of a source file, from the cache or cooked now and stored.
	 * @param[in] iSource Absolute path of the source file.
	 * @param[in] iMeta Importer and settings of the asset.
	 * @param[in] iName Asset path, for log messages.
	 * @return The cooked bytes, or the reason of the failure.
	 */
	auto cook(const std::filesystem::path& iSource, const AssetMeta& iMeta, std::string_view iName)
			-> expected<std::vector<uint8_t>, CookError>;

	/**
	 * @brief
	 *  Tell whether the cooked result of a source is in the cache for its current bytes and settings.
	 * @param[in] iSource Absolute path of the source file.
	 * @param[in] iMeta Importer and settings of the asset.
	 * @return True when `cook` would not cook it again.
	 */
	[[nodiscard]] auto isCached(const std::filesystem::path& iSource, const AssetMeta& iMeta) -> bool;

	/**
	 * @brief
	 *  Write the manifest now.
	 * @return True when the manifest was written.
	 */
	auto saveManifest() -> bool;

	/**
	 * @brief
	 *  Delete every cooked result and the manifest.
	 */
	void clear();

	/**
	 * @brief
	 *  Get the counters of the cache.
	 * @return The counters.
	 */
	[[nodiscard]] auto getStats() const -> Stats;

	/**
	 * @brief
	 *  Get the cache directory.
	 * @return The directory.
	 */
	[[nodiscard]] auto getDirectory() const -> const std::filesystem::path& { return m_directory; }

private:
	/**
	 * @brief
	 *  What the manifest knows of one source file.
	 */
	struct ManifestEntry {
		/// Size of the source in bytes.
		uintmax_t size{0};
		/// Write time of the source, in ticks of the file clock.
		int64_t writeTime{0};
		/// Settings part of the key (importer, version, settings).
		std::string settings;
		/// Key of the cooked result.
		std::string key;
	};

	/**
	 * @brief
	 *  Path of the cooked result of a key.
	 * @param[in] iKey The key.
	 * @return `<directory>/<2 hex>/<key>.owlc`.
	 */
	[[nodiscard]] auto resultPath(const std::string& iKey) const -> std::filesystem::path;

	/**
	 * @brief
	 *  Key of a source whose size and write time match the manifest.
	 * @param[in] iSource Absolute path of the source.
	 * @param[in] iSettings Settings part of the key.
	 * @return The key, or empty when the manifest does not match.
	 */
	[[nodiscard]] auto manifestKey(const std::filesystem::path& iSource, const std::string& iSettings) -> std::string;

	/// The cache directory.
	std::filesystem::path m_directory;
	/// Guards the manifest and the counters.
	mutable std::mutex m_mutex;
	/// Manifest entries by absolute source path.
	std::unordered_map<std::string, ManifestEntry> m_manifest;
	/// True when the manifest changed since it was read.
	bool m_dirty{false};
	/// Counters.
	Stats m_stats;
};

}// namespace owl::data::assets::cook
