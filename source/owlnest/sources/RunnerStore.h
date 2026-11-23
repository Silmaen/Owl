/**
 * @file RunnerStore.h
 * @author Silmaen
 * @date 23/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <core/expected.h>
#include <data/assets/pack/GameExporter.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace owl::nest {

/**
 * @brief
 *  Why a runner could not be installed.
 */
enum struct RunnerError : uint8_t {
	/// The Nest package has no `runners.json`: it knows no runner archive.
	NoManifest,
	/// The archive is not one of the runners of this Owl Nest.
	UnknownRunner,
	/// The server could not be reached, or did not answer 200.
	Download,
	/// The size or the SHA-256 of the archive differs from `runners.json`.
	Mismatch,
	/// The archive could not be unpacked into the cache.
	Extract,
};

/**
 * @brief
 *  A runner archive of the release of this Owl Nest, as `runners.json` lists it.
 */
struct RunnerArchive {
	/// File name, `OwlGameRunner-<version>-<hash>-<target>.tar.gz`.
	std::string file;
	/// Size in bytes.
	uint64_t size = 0;
	/// SHA-256, lower-case hexadecimal.
	std::string sha256;
};

/**
 * @brief
 *  The game runners an export can use: the host and Web runners shipped with Owl Nest, the others downloaded or
 *  imported once into a cache shared by every project.
 *
 * Only the archives of the release of this Owl Nest are accepted: their size and SHA-256 come from the `runners.json`
 * of its package. The cache keeps one folder per release (`<version>-<hash>/<target>/`).
 */
class RunnerStore final {
public:
	RunnerStore(const RunnerStore&) = delete;

	RunnerStore(RunnerStore&&) = delete;

	auto operator=(const RunnerStore&) -> RunnerStore& = delete;

	auto operator=(RunnerStore&&) -> RunnerStore& = delete;

	/**
	 * @brief
	 *  Constructor, reading `runners.json` in the folder of Owl Nest.
	 * @param[in] iNestDirectory The folder of the Owl Nest executable (holds `runners.json` and `web/`).
	 * @param[in] iCacheRoot The folder of the downloaded runners, one sub-folder per release.
	 */
	RunnerStore(std::filesystem::path iNestDirectory, std::filesystem::path iCacheRoot);

	/**
	 * @brief
	 *  Destructor.
	 */
	~RunnerStore() = default;

	/// The runner server of the release builds; a mirror answering the same paths replaces it in the settings.
	static constexpr std::string_view defaultServer = "https://owl.argawaen.net";

	/**
	 * @brief
	 *  Name of a target in the runner archives and `runners.json`.
	 * @param[in] iTarget The export target (`Host` is the desktop target of this platform).
	 * @return `linux-x64`, `linux-arm64`, `windows-x64`, `android-arm64` or `web`.
	 */
	[[nodiscard]] static auto getTargetName(data::assets::pack::ExportTarget iTarget) -> std::string_view;

	/**
	 * @brief
	 *  Text of an error.
	 * @param[in] iError The error.
	 * @return A short sentence.
	 */
	[[nodiscard]] static auto getErrorMessage(RunnerError iError) -> std::string_view;

	/**
	 * @brief
	 *  Check whether the package of Owl Nest lists its runners.
	 * @return True when `runners.json` was read.
	 */
	[[nodiscard]] auto hasManifest() const -> bool { return !m_archives.empty(); }

	/**
	 * @brief
	 *  Check whether a target's runner ships with Owl Nest (its own desktop target and the Web).
	 * @param[in] iTarget The export target.
	 * @return True for a runner never downloaded.
	 */
	[[nodiscard]] static auto isBundled(data::assets::pack::ExportTarget iTarget) -> bool;

	/**
	 * @brief
	 *  Get the archive of a target, as `runners.json` lists it.
	 * @param[in] iTarget The export target.
	 * @return The archive, none when the manifest does not list it.
	 */
	[[nodiscard]] auto getArchive(data::assets::pack::ExportTarget iTarget) const -> std::optional<RunnerArchive>;

	/**
	 * @brief
	 *  Get the folder an export of a target takes its runner from.
	 * @param[in] iTarget The export target.
	 * @return The folder of Owl Nest, its `web/` folder, or the cache folder of the target.
	 */
	[[nodiscard]] auto getRunnerDirectory(data::assets::pack::ExportTarget iTarget) const -> std::filesystem::path;

	/**
	 * @brief
	 *  Check whether the runner of a target is ready: shipped, or downloaded and checked.
	 * @param[in] iTarget The export target.
	 * @return True when an export can use it.
	 */
	[[nodiscard]] auto isInstalled(data::assets::pack::ExportTarget iTarget) const -> bool;

	/**
	 * @brief
	 *  Get the cache folder of the release of this Owl Nest.
	 * @return `<cache root>/<version>-<hash>`.
	 */
	[[nodiscard]] auto getCacheDirectory() const -> std::filesystem::path;

	/**
	 * @brief
	 *  Get the address of a target's archive on a runner server.
	 * @param[in] iTarget The export target.
	 * @param[in] iServer The server, `https://host[:port]`.
	 * @return `<server>/runners/<version>/<hash>/<file>`, empty when the manifest does not list the target.
	 */
	[[nodiscard]] auto getDownloadUrl(data::assets::pack::ExportTarget iTarget, std::string_view iServer) const
			-> std::string;

	/**
	 * @brief
	 *  Download, check and install the runner of a target.
	 * @param[in] iTarget The export target.
	 * @param[in] iServer The server, `https://host[:port]`.
	 * @param[in] iProgress Called with the bytes received and the total; false cancels the download.
	 * @return Nothing on success, the error otherwise.
	 */
	auto download(data::assets::pack::ExportTarget iTarget, std::string_view iServer,
				  const std::function<bool(uint64_t, uint64_t)>& iProgress) -> expected<void, RunnerError>;

	/**
	 * @brief
	 *  Check and install a runner archive downloaded by hand.
	 * @param[in] iArchive The archive, named as the server names it.
	 * @return The target installed, the error otherwise.
	 */
	auto importArchive(const std::filesystem::path& iArchive)
			-> expected<data::assets::pack::ExportTarget, RunnerError>;

	/**
	 * @brief
	 *  Remove the cached runners of the other releases.
	 * @return The number of release folders removed.
	 */
	auto removeOtherVersions() -> size_t;

private:
	/**
	 * @brief
	 *  Check an archive against the manifest and unpack it into the target's cache folder.
	 * @param[in] iTarget The export target.
	 * @param[in] iArchive The archive on disk.
	 * @return Nothing on success, the error otherwise.
	 */
	[[nodiscard]] auto install(data::assets::pack::ExportTarget iTarget, const std::filesystem::path& iArchive) const
			-> expected<void, RunnerError>;

	/// The folder of Owl Nest.
	std::filesystem::path m_nestDirectory;
	/// The folder of the downloaded runners.
	std::filesystem::path m_cacheRoot;
	/// The version of the release (`runners.json`).
	std::string m_version;
	/// The abbreviated commit of the release (`runners.json`).
	std::string m_hash;
	/// The archives of the release, by target name.
	std::map<std::string, RunnerArchive, std::less<>> m_archives;
};

}// namespace owl::nest
