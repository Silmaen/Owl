/**
 * @file RecoveryManager.h
 * @author Silmaen
 * @date 08/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "document/Document.h"

#include <core/FormatVersion.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace owl::nest {

class DocumentManager;

/**
 * @brief
 *  One autosaved document, as listed in the recovery manifest.
 */
struct RecoveryEntry {
	/// Kind of document, to reopen it with the right editor.
	DocumentType type = DocumentType::Scene;
	/// File the document edits; empty for an untitled document.
	std::filesystem::path originalPath;
	/// Tab title at the time of the autosave.
	std::string title;
	/// Name of the snapshot file inside the recovery folder.
	std::string dataFile;
	/// UTC time of the autosave, for display.
	std::string savedAt;
};

/**
 * @brief
 *  Periodic autosave of the modified documents of a project and recovery of them at the next launch.
 *
 * Snapshots live outside the project, in `<working dir>/OwlNest_recovery/<project>-<hash>/`: one file per dirty
 * document plus a `recovery.yml` manifest. The folder is emptied once the user recovered or discarded them; a
 * snapshot older than its saved file is never offered.
 */
class RecoveryManager final {
public:
	RecoveryManager(const RecoveryManager&) = delete;

	RecoveryManager(RecoveryManager&&) = delete;

	auto operator=(const RecoveryManager&) -> RecoveryManager& = delete;

	auto operator=(RecoveryManager&&) -> RecoveryManager& = delete;

	RecoveryManager() = default;

	~RecoveryManager() = default;

	/// Name of the manifest file inside the recovery folder.
	static constexpr const char* g_manifestName = "recovery.yml";

	/**
	 * @brief
	 *  Format descriptor of the recovery manifest.
	 * @return The descriptor.
	 */
	[[nodiscard]] static auto format() -> const core::DocumentFormat&;

	/**
	 * @brief
	 *  Recovery folder of a project.
	 * @param[in] iRoot Folder holding every project's recovery folder (the editor's working directory).
	 * @param[in] iProjectDirectory The project folder.
	 * @return `iRoot/OwlNest_recovery/<project name>-<hash of its path>`.
	 */
	[[nodiscard]] static auto directoryFor(const std::filesystem::path& iRoot,
										   const std::filesystem::path& iProjectDirectory) -> std::filesystem::path;

	/**
	 * @brief
	 *  Set the folder the snapshots are written to; empty disables the autosave.
	 * @param[in] iDirectory The recovery folder.
	 */
	void setDirectory(const std::filesystem::path& iDirectory);

	/**
	 * @brief
	 *  Get the recovery folder.
	 * @return The folder, empty when disabled.
	 */
	[[nodiscard]] auto getDirectory() const -> const std::filesystem::path& { return m_directory; }

	/**
	 * @brief
	 *  Set the autosave period.
	 * @param[in] iSeconds Seconds between two autosaves; 0 or less disables the periodic autosave.
	 */
	void setInterval(float iSeconds) { m_interval = iSeconds; }

	/**
	 * @brief
	 *  Get the autosave period.
	 * @return Seconds between two autosaves.
	 */
	[[nodiscard]] auto getInterval() const -> float { return m_interval; }

	/**
	 * @brief
	 *  Suspend the autosave, e.g. while the user has not decided what to do with the pending snapshots.
	 * @param[in] iSuspended True to suspend.
	 */
	void setSuspended(const bool iSuspended) { m_suspended = iSuspended; }

	/**
	 * @brief
	 *  Check whether the autosave is suspended.
	 * @return True when suspended.
	 */
	[[nodiscard]] auto isSuspended() const -> bool { return m_suspended; }

	/**
	 * @brief
	 *  Advance the autosave clock.
	 * @param[in] iDeltaSeconds Time elapsed since the previous call.
	 * @return True when an autosave is due (the clock restarts).
	 */
	auto onUpdate(float iDeltaSeconds) -> bool;

	/**
	 * @brief
	 *  Write the snapshot of every modified document that supports it, and drop the others.
	 * @param[in] iDocuments The open documents.
	 * @return Number of snapshot files written (unchanged snapshots are not rewritten).
	 */
	auto autosave(const DocumentManager& iDocuments) -> size_t;

	/**
	 * @brief
	 *  Snapshots left by a previous session, minus those older than their saved file.
	 * @return The entries, empty when there is nothing to recover.
	 */
	[[nodiscard]] auto getPendingEntries() const -> std::vector<RecoveryEntry>;

	/**
	 * @brief
	 *  Read the content of a snapshot.
	 * @param[in] iEntry The entry.
	 * @return The content, or nothing when the file cannot be read.
	 */
	[[nodiscard]] auto readSnapshot(const RecoveryEntry& iEntry) const -> std::optional<std::string>;

	/**
	 * @brief
	 *  Delete every snapshot and the manifest.
	 */
	void clear();

	/**
	 * @brief
	 *  Move the recovery folder aside (kept for a manual recovery) and start an empty one.
	 * @return The folder the snapshots now live in, empty when there was nothing to move.
	 */
	auto keepAside() -> std::filesystem::path;

private:
	/// Last snapshot written for one document.
	struct Written {
		/// Hash of the content, to skip unchanged snapshots.
		size_t hash = 0;
		/// UTC time of the write.
		std::string savedAt;
	};
	/// Folder of the snapshots; empty when disabled.
	std::filesystem::path m_directory;
	/// Seconds between two autosaves.
	float m_interval = 60.f;
	/// Seconds since the last autosave.
	float m_elapsed = 0.f;
	/// True while the autosave is suspended.
	bool m_suspended = false;
	/// Snapshots written in this session, by document id.
	std::unordered_map<uint64_t, Written> m_written;
};

}// namespace owl::nest
