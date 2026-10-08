/**
 * @file HotReload.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "platform/FileWatcher.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace owl::app {

/**
 * @brief
 *  Reloads the assets whose file changed on disk, for iteration in the editor and the development runner.
 *
 * A `platform::FileWatcher` polls the asset directories on its own thread; on the frame, `onFrame` only reads an
 * atomic flag until a file changed. A changed file is then dispatched on the main thread, at the start of the frame:
 *  - images (`.png`, `.jpg`): every texture of the renderer's library loaded from that file gets the new pixels;
 *  - Slang shaders (`.slang`): every shader compiled from that file is recompiled and swapped, its pipelines rebuilt;
 *  - anything else (scenes, tilesets, tilemaps, Lua scripts): handed to the listeners, which own the scenes.
 *
 * A failed reload keeps the previous version and logs an error naming the file (with the Slang or Lua diagnostic).
 * The application never enables it when an asset pack is open: a packaged game does not watch anything.
 */
class OWL_API HotReload final {
public:
	HotReload(const HotReload&) = delete;

	HotReload(HotReload&&) = delete;

	auto operator=(const HotReload&) -> HotReload& = delete;

	auto operator=(HotReload&&) -> HotReload& = delete;

	/**
	 * @brief
	 *  Callback receiving a changed file.
	 */
	using Listener = std::function<void(const std::filesystem::path&)>;

	/**
	 * @brief
	 *  Result of the last dispatched file.
	 */
	struct Report {
		/// The changed file.
		std::filesystem::path file;
		/// Number of assets reloaded from it by the engine (textures, shaders).
		uint32_t reloaded = 0;
		/// Number of engine reloads that failed and kept the previous version.
		uint32_t failed = 0;
	};

	/**
	 * @brief
	 *  Constructor.
	 * @param[in] iPeriod Time between two scans of the watched directories.
	 */
	explicit HotReload(std::chrono::milliseconds iPeriod = std::chrono::milliseconds{500});

	/**
	 * @brief
	 *  Destructor, stops watching.
	 */
	~HotReload();

	/**
	 * @brief
	 *  Start or stop watching the directories.
	 * @param[in] iEnabled True to watch.
	 */
	void setEnabled(bool iEnabled);

	/**
	 * @brief
	 *  Check if the directories are watched.
	 * @return True when watching.
	 */
	[[nodiscard]] auto isEnabled() const -> bool { return m_watcher.isRunning(); }

	/**
	 * @brief
	 *  Watch a directory tree.
	 * @param[in] iDirectory The directory.
	 */
	void watchDirectory(const std::filesystem::path& iDirectory) { m_watcher.addDirectory(iDirectory); }

	/**
	 * @brief
	 *  Stop watching a directory tree.
	 * @param[in] iDirectory The directory.
	 */
	void unwatchDirectory(const std::filesystem::path& iDirectory) { m_watcher.removeDirectory(iDirectory); }

	/**
	 * @brief
	 *  Register a callback called on the main thread with every changed file, after the engine's own reloads.
	 * @param[in] iListener The callback.
	 * @return The id to give to `removeListener`.
	 */
	auto addListener(Listener iListener) -> uint32_t;

	/**
	 * @brief
	 *  Unregister a callback.
	 * @param[in] iId The id returned by `addListener`.
	 */
	void removeListener(uint32_t iId);

	/**
	 * @brief
	 *  Dispatch the files changed since the last frame. Costs one atomic read when nothing changed.
	 */
	void onFrame() {
		if (m_watcher.hasChanges())
			dispatchChanges();
	}

	/**
	 * @brief
	 *  Reload what was made from one file, now, then notify the listeners.
	 * @param[in] iFile The changed file.
	 * @return The report of the engine's reloads.
	 */
	auto reloadFile(const std::filesystem::path& iFile) -> Report;

	/**
	 * @brief
	 *  Get the report of the last dispatched file.
	 * @return The report.
	 */
	[[nodiscard]] auto getLastReport() const -> const Report& { return m_lastReport; }

	/**
	 * @brief
	 *  Access the file watcher (tests scan synchronously through it).
	 * @return The watcher.
	 */
	[[nodiscard]] auto getWatcher() -> platform::FileWatcher& { return m_watcher; }

private:
	/**
	 * @brief
	 *  Take the watcher's changes and reload each file.
	 */
	void dispatchChanges();

	/// Background poller of the watched directories.
	platform::FileWatcher m_watcher;
	/// Registered callbacks, by id.
	std::vector<std::pair<uint32_t, Listener>> m_listeners;
	/// Next listener id.
	uint32_t m_nextListenerId = 1;
	/// Report of the last dispatched file.
	Report m_lastReport;
};

}// namespace owl::app
