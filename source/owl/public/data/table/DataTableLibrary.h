/**
 * @file DataTableLibrary.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "data/table/DataTable.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace owl::data::table {

/**
 * @brief
 *  The data tables a game has read, by asset path: each file is read once, from the pack or the asset folders.
 *
 * One lives in each `app::EngineContext`; Owl Nest forgets a table when it saves it, so the next read sees the edit.
 */
class OWL_API DataTableLibrary final {
public:
	/**
	 * @brief
	 *  Build an empty library.
	 */
	DataTableLibrary() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~DataTableLibrary() = default;

	DataTableLibrary(const DataTableLibrary&) = delete;

	DataTableLibrary(DataTableLibrary&&) = delete;

	auto operator=(const DataTableLibrary&) -> DataTableLibrary& = delete;

	auto operator=(DataTableLibrary&&) -> DataTableLibrary& = delete;

	/**
	 * @brief
	 *  A table, read on first use.
	 * @param[in] iAsset Asset path or `gid:` reference of the table.
	 * @return The table, or null when it cannot be read (the failure is kept until `invalidate`).
	 */
	[[nodiscard]] auto get(std::string_view iAsset) -> shared<const DataTable>;

	/**
	 * @brief
	 *  Forget a table: the next `get` reads its file again.
	 * @param[in] iAsset Asset path or `gid:` reference of the table.
	 */
	void invalidate(std::string_view iAsset);

	/**
	 * @brief
	 *  Forget every table.
	 */
	void clear();

	/**
	 * @brief
	 *  Number of files read.
	 * @return The count.
	 */
	[[nodiscard]] auto getLoadCount() const -> uint64_t { return m_loadCount; }

	/**
	 * @brief
	 *  Number of tables held (failed reads included).
	 * @return The count.
	 */
	[[nodiscard]] auto size() const -> size_t;

private:
	/// Guards the tables (scripts of several scenes may read at once).
	mutable std::mutex m_mutex;
	/// Tables by asset path (null for a file that failed to load).
	std::unordered_map<std::string, shared<const DataTable>> m_tables;
	/// Number of files read.
	uint64_t m_loadCount = 0;
};

}// namespace owl::data::table
