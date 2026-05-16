/**
 * @file BitonicSortPass.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::utils {

/**
 * @brief
 *  GPU sprite Z-sort (#32) — wraps a single-workgroup bitonic-sort compute
 *  shader. The shader sorts an SSBO of `(float key, uint value)` pairs
 *  ascending by `key`. Painter's-order callers (back-to-front) negate
 *  their distance before submitting.
 *
 *  Capacity is fixed at `kSortSize = 1024`; callers pad up to that with
 *  sentinel entries (`key = +∞`, `value` unused) and remember the real
 *  count so they can ignore tail entries in the sorted output.
 *
 *  Synchronisation: `sort()` emits `RenderCommand::
 *  storageBufferMemoryBarrier()` after the dispatch, so the next draw
 *  can read `getBuffer()` safely.
 */
class BitonicSortPass final {
public:
	/**
	 * @brief
	 *  Sort key + payload value pair. Layout must match the Slang
	 *  `SortItem` struct (one float + one uint, std430).
	 */
	struct Item {
		/// Sort key (ascending).
		float key = 0.0f;
		/// User payload (e.g., sprite index).
		uint32_t value = 0;
	};

	/// Maximum sorted item count. Matches the Slang shader's `kSortSize`.
	static constexpr uint32_t kSortSize = 1024;

	BitonicSortPass() = default;

	BitonicSortPass(const BitonicSortPass&) = delete;

	BitonicSortPass(BitonicSortPass&&) = delete;

	auto operator=(const BitonicSortPass&) -> BitonicSortPass& = delete;

	auto operator=(BitonicSortPass&&) -> BitonicSortPass& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~BitonicSortPass() = default;

	/**
	 * @brief
	 *  Compile the compute shader and allocate the fixed-size SSBO.
	 *  Idempotent.
	 */
	OWL_API void init();

	/**
	 * @brief
	 *  Release the compute shader and SSBO.
	 */
	OWL_API void shutdown();

	/**
	 * @brief
	 *  Upload `iItems` into the SSBO (padded with `+∞` keys up to
	 *  `kSortSize`), dispatch the sort, and emit a storage barrier.
	 *  Items larger than `kSortSize` are truncated with a warning.
	 * @param[in] iItems Items to sort. Up to `kSortSize` entries.
	 */
	OWL_API void sort(std::span<const Item> iItems);

	/**
	 * @brief
	 *  Backing SSBO holding the sorted items. Downstream graphics
	 *  passes bind this with `bindStorageBuffer`. The first
	 *  `getItemCount()` entries are the real sorted data; the tail is
	 *  the padding (`+∞` keys, ignore).
	 * @return Sorted SSBO, or nullptr if `init()` was not called.
	 */
	[[nodiscard]] auto getBuffer() const -> shared<gpu::StorageBuffer> { return m_buffer; }

	/**
	 * @brief
	 *  Number of valid sorted items (i.e., `iItems.size()` at the last
	 *  `sort()` call, clamped to `kSortSize`).
	 * @return Real item count.
	 */
	[[nodiscard]] auto getItemCount() const -> uint32_t { return m_itemCount; }

private:
	/// Compute shader instance.
	shared<gpu::ComputeShader> m_shader;
	/// Fixed-capacity SSBO holding the items (sorted in place by `sort()`).
	shared<gpu::StorageBuffer> m_buffer;
	/// Valid item count from the last `sort()` call.
	uint32_t m_itemCount = 0;
	/// True once `init()` succeeded.
	bool m_ready = false;
};

}// namespace owl::renderer::utils
