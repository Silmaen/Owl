/**
 * @file FileWatcher.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace owl::platform {

/**
 * @brief
 *  Tell whether two paths name the same file on disk.
 * @param[in] iLhs First path.
 * @param[in] iRhs Second path.
 * @return True when both exist and are the same file, or when their normalised absolute forms are equal.
 */
OWL_API auto isSameFile(const std::filesystem::path& iLhs, const std::filesystem::path& iRhs) -> bool;

/**
 * @brief
 *  Polls directory trees on a background thread and reports the files whose content changed.
 *
 * The thread compares each regular file's modification time and size with the previous scan; a change is reported
 * once the file has kept the same stamp for one more period, so a file still being written is not reported half
 * done. Created files count as changed, deleted ones are forgotten. The main thread only reads an atomic flag
 * (`hasChanges`) until something changed: watching costs nothing on the frame.
 */
class OWL_API FileWatcher final {
public:
	FileWatcher(const FileWatcher&) = delete;

	FileWatcher(FileWatcher&&) = delete;

	auto operator=(const FileWatcher&) -> FileWatcher& = delete;

	auto operator=(FileWatcher&&) -> FileWatcher& = delete;

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iPeriod Time between two scans of the background thread.
	 */
	explicit FileWatcher(std::chrono::milliseconds iPeriod = std::chrono::milliseconds{500});

	/**
	 * @brief
	 *  Destructor, stops the thread.
	 */
	~FileWatcher();

	/**
	 * @brief
	 *  Watch a directory tree; its current files are recorded at once and are not reported.
	 * @param[in] iDirectory The directory, watched recursively.
	 */
	void addDirectory(const std::filesystem::path& iDirectory);

	/**
	 * @brief
	 *  Stop watching a directory tree added by `addDirectory`.
	 * @param[in] iDirectory The directory.
	 */
	void removeDirectory(const std::filesystem::path& iDirectory);

	/**
	 * @brief
	 *  Get the watched directories.
	 * @return The directories.
	 */
	[[nodiscard]] auto getDirectories() const -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Start the background thread (no effect when it runs already).
	 */
	void start();

	/**
	 * @brief
	 *  Stop the background thread; the pending changes are kept.
	 */
	void stop();

	/**
	 * @brief
	 *  Check if the background thread runs.
	 * @return True when it runs.
	 */
	[[nodiscard]] auto isRunning() const -> bool { return m_thread.joinable(); }

	/**
	 * @brief
	 *  Check if changes wait to be taken. Lock-free: this is the only call meant for every frame.
	 * @return True when `takeChanges` would return something.
	 */
	[[nodiscard]] auto hasChanges() const noexcept -> bool { return m_hasChanges.load(std::memory_order_acquire); }

	/**
	 * @brief
	 *  Take the changed files reported since the last call.
	 * @return The changed files, each once, in the order they were reported.
	 */
	auto takeChanges() -> std::vector<std::filesystem::path>;

	/**
	 * @brief
	 *  Run one scan now, on the calling thread (the background thread calls it at each period).
	 */
	void scan();

private:
	/**
	 * @brief
	 *  Modification stamp of a file.
	 */
	struct Stamp {
		/// Last write time.
		std::filesystem::file_time_type time;
		/// Size in bytes.
		uintmax_t size = 0;
		/**
		 * @brief
		 *  Comparison operator.
		 * @param[in] iOther Other stamp.
		 * @return True when both are equal.
		 */
		auto operator==(const Stamp& iOther) const -> bool = default;
	};

	/**
	 * @brief
	 *  Collect the stamps of every regular file of a tree.
	 * @param[in] iDirectory The tree root.
	 * @param[in,out] ioStamps Stamps, by generic path.
	 */
	static void collect(const std::filesystem::path& iDirectory, std::unordered_map<std::string, Stamp>& ioStamps);

	/// Time between two scans.
	std::chrono::milliseconds m_period;
	/// Guards the watched directories and the pending changes.
	mutable std::mutex m_mutex;
	/// Serialises the scans (the thread and a direct `scan` call).
	std::mutex m_scanMutex;
	/// Watched directory trees.
	std::vector<std::filesystem::path> m_directories;
	/// Stamps seen at the last report of each file.
	std::unordered_map<std::string, Stamp> m_known;
	/// Files whose stamp changed at the last scan, reported when it holds for one more scan.
	std::unordered_map<std::string, Stamp> m_settling;
	/// Changes reported and not taken yet.
	std::vector<std::filesystem::path> m_pending;
	/// True when `m_pending` is not empty.
	std::atomic<bool> m_hasChanges{false};
	/// Guards the wait of the background thread between two scans.
	std::mutex m_wakeMutex;
	/// Wakes the background thread when it is asked to stop.
	std::condition_variable_any m_wake;
	/// Background scanning thread.
	std::jthread m_thread;
};

}// namespace owl::platform
