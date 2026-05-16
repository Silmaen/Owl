/**
 * @file WorldTransformPass.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/Transform.h"
#include "math/matrices.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::utils {

/**
 * @brief
 *  Hierarchical world-transform compute pre-pass. Owns three SSBOs
 *  (`locals[]`, `parents[]`, `worlds[]`) and a Slang compute shader that
 *  walks each entity's parent chain to its root and writes the resulting
 *  world matrix into `worlds[]`. One dispatch per frame, no per-level
 *  barriers — the shader only reads `locals[]` (frozen during the
 *  dispatch) so a single workgroup launch is race-free regardless of
 *  hierarchy depth.
 *
 *  Caller responsibilities:
 *    - Topologically sort the entries so every `parentIdx < own index`.
 *      Roots get `parentIdx == -1`.
 *    - Call `compute(entries)` once per frame, before issuing any
 *      graphics draw that reads from `getWorldBuffer()`.
 *
 *  Buffer sizing: entries are padded up to a multiple of 64 (the
 *  shader's workgroup size); tail slots are filled with identity
 *  matrices and `parentIdx = -1`, so padded threads compute a harmless
 *  identity world matrix that no graphics pass indexes.
 *
 *  Synchronisation: `compute()` issues `RenderCommand::
 *  storageBufferMemoryBarrier()` after the dispatch, so downstream
 *  draws that read `worlds[]` see the freshly-written matrices.
 */
class WorldTransformPass final {
public:
	/**
	 * @brief
	 *  Single hierarchy entry — the input pair the compute shader needs
	 *  to walk the parent chain.
	 */
	struct Entry {
		/// Local-space transform.
		math::mat4 local = math::identity<float, 4>();
		/// Parent index (-1 for root).
		int32_t parentIdx = -1;
	};

	WorldTransformPass() = default;

	WorldTransformPass(const WorldTransformPass&) = delete;

	WorldTransformPass(WorldTransformPass&&) = delete;

	auto operator=(const WorldTransformPass&) -> WorldTransformPass& = delete;

	auto operator=(WorldTransformPass&&) -> WorldTransformPass& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~WorldTransformPass() = default;

	/**
	 * @brief
	 *  Compile the compute shader and allocate the initial SSBOs.
	 *  Idempotent — calling twice is a no-op.
	 */
	OWL_API void init();

	/**
	 * @brief
	 *  Release the compute shader and SSBOs.
	 */
	OWL_API void shutdown();

	/**
	 * @brief
	 *  Dispatch the world-transform compute. Pads `iEntries` up to a
	 *  multiple of 64, uploads to the SSBOs (growing them if needed),
	 *  binds them to the compute shader and runs one dispatch. Issues
	 *  `storageBufferMemoryBarrier()` after the dispatch so the next
	 *  draw can safely read `getWorldBuffer()`.
	 * @param[in] iEntries Topologically-sorted entries (parentIdx < own
	 *  index for every non-root entry).
	 */
	OWL_API void compute(std::span<const Entry> iEntries);

	/**
	 * @brief
	 *  Output SSBO of world matrices, indexed by entry index. Read-only
	 *  from the caller's perspective. The size in matrices is the
	 *  padded entry count (multiple of 64).
	 * @return The world-matrix SSBO, or nullptr if `init()` was not
	 *  called.
	 */
	[[nodiscard]] auto getWorldBuffer() const -> shared<gpu::StorageBuffer> { return m_worldBuffer; }

	/**
	 * @brief
	 *  Number of valid entries in `getWorldBuffer()` (entries past this
	 *  index are padded identity matrices). Equal to the last
	 *  `iEntries.size()` passed to `compute()`.
	 * @return Valid entry count.
	 */
	[[nodiscard]] auto getEntryCount() const -> uint32_t { return m_entryCount; }

private:
	/// Workgroup size declared by the compute shader. Padding granularity.
	static constexpr uint32_t kWorkgroupSize = 64;

	/// Compute shader instance.
	shared<gpu::ComputeShader> m_shader;
	/// SSBO of local-space matrices (input).
	shared<gpu::StorageBuffer> m_localsBuffer;
	/// SSBO of parent indices (input).
	shared<gpu::StorageBuffer> m_parentsBuffer;
	/// SSBO of computed world matrices (output).
	shared<gpu::StorageBuffer> m_worldBuffer;
	/// Padded capacity (in entries) of the SSBOs.
	uint32_t m_paddedCapacity = 0;
	/// Number of valid entries in the last `compute()` call.
	uint32_t m_entryCount = 0;
	/// True once `init()` has run successfully.
	bool m_ready = false;
};

}// namespace owl::renderer::utils
