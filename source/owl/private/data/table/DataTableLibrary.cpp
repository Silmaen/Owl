/**
 * @file DataTableLibrary.cpp
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#include "owlpch.h"

#include "data/table/DataTableLibrary.h"

#include "data/assets/AssetReference.h"

#include <mutex>
#include <string>

namespace owl::data::table {

auto DataTableLibrary::get(const std::string_view iAsset) -> shared<const DataTable> {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	if (const auto found = m_tables.find(path); found != m_tables.end())
		return found->second;
	++m_loadCount;
	shared<const DataTable> table;
	if (auto loaded = DataTable::load(path); loaded)
		table = mkShared<const DataTable>(std::move(*loaded));
	m_tables.emplace(path, table);
	return table;
}

void DataTableLibrary::invalidate(const std::string_view iAsset) {
	const std::string path = data::assets::toAssetPath(iAsset);
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_tables.erase(path);
}

void DataTableLibrary::clear() {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	m_tables.clear();
}

auto DataTableLibrary::size() const -> size_t {
	const std::scoped_lock<std::mutex> lock{m_mutex};
	return m_tables.size();
}

}// namespace owl::data::table
