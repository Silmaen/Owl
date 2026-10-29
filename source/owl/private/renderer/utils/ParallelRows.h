/**
 * @file ParallelRows.h
 * @author Silmaen
 * @date 28/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace owl::renderer::utils {

/**
 * @brief
 *  Run a function once per row, the rows shared between the hardware threads.
 * @tparam Fn Callable taking the row index.
 * @param[in] iCount Number of rows.
 * @param[in] iFunction Function called with each row index, from any thread.
 */
template<typename Fn>
void parallelRows(const uint32_t iCount, Fn&& iFunction) {
	const auto workers = std::min<uint32_t>(std::max(1u, std::thread::hardware_concurrency()), iCount);
	if (workers <= 1) {
		for (uint32_t row = 0; row < iCount; ++row) { iFunction(row); }
		return;
	}
	std::atomic<uint32_t> next{0};
	auto work = [&]() -> void {
		for (;;) {
			const uint32_t row = next.fetch_add(1);
			if (row >= iCount) {
				return;
			}
			iFunction(row);
		}
	};
	std::vector<std::thread> threads;
	threads.reserve(workers - 1);
	for (uint32_t i = 1; i < workers; ++i) { threads.emplace_back(work); }
	work();
	for (auto& thread: threads) { thread.join(); }
}

}// namespace owl::renderer::utils
