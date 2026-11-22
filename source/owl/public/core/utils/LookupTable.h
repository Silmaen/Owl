/**
 * @file LookupTable.h
 * @author Silmaen
 * @date 18/11/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <functional>
#include <ranges>
#include <type_traits>

namespace owl::core::utils {

/**
 * @brief
 *  Finds the entry of a lookup table whose projected member equals a key.
 * @details
 *  A pointer rather than an iterator: a `std::array` iterator is a pointer in libstdc++, a class in the libc++ of
 *  Emscripten, and code holding it in an `auto*` builds on one of them only.
 * @tparam Table Range of the entries, contiguous.
 * @tparam Key Type of the key.
 * @tparam Proj Projection of an entry to the member compared with the key.
 * @param[in] iTable Table to search.
 * @param[in] iKey Key to find.
 * @param[in] iProj Projection of an entry.
 * @return The first matching entry, or nullptr.
 */
template<std::ranges::contiguous_range Table, typename Key, typename Proj>
constexpr auto findEntry(Table& iTable, const Key& iKey, Proj iProj)
		-> std::remove_reference_t<std::ranges::range_reference_t<Table>>* {
	for (auto& entry: iTable) {
		if (std::invoke(iProj, entry) == iKey)
			return &entry;
	}
	return nullptr;
}

}// namespace owl::core::utils
