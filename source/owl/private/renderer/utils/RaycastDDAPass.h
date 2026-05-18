/**
 * @file RaycastDDAPass.h
 * @author Silmaen
 * @date 16/05/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "math/vectors.h"
#include "renderer/gpu/ComputeShader.h"
#include "renderer/gpu/StorageBuffer.h"

namespace owl::renderer::utils {

/**
 * @brief
 *  Per-column raycast DDA compute pass (#35). Replaces the CPU per-column
 *  walk in `RendererRaycast::drawTilemapWalls` with one thread per screen
 *  column on the GPU. Outputs the same per-column hit data the CPU walk
 *  used to build (tile index, perpendicular distance, texture-U fraction,
 *  wall height, side, transparency flag) into `getColumnHitBuffer()` plus
 *  a per-column hit-count SSBO and an opaque-depth zBuffer SSBO.
 *
 *  The biggest single CPU → GPU win on the v0.2.0 roadmap; 1280 screen
 *  columns × ~30 DDA steps per column moves off the main thread into a
 *  ~20-workgroup compute dispatch.
 *
 *  Caller responsibilities:
 *    - Upload the tile grid + tile metadata via `update(...)` whenever
 *      the tilemap changes (typically once on scene load / edit).
 *    - Call `dispatch(params)` once per frame before consuming the
 *      output buffers in the draw pass.
 *
 *  The Slang shader caps the per-column hit list at `kMaxHitsPerColumn`
 *  to match the CPU walk's transparent-stack budget.
 */
class RaycastDDAPass final {
public:
	/**
	 * @brief
	 *  Per-call parameters (camera + tilemap layout). Matches the Slang
	 *  `RaycastParams` struct byte-for-byte (std430).
	 */
	struct Params {
		/// Camera position in cell coordinates.
		math::vec2 camCellPos{0.0f, 0.0f};
		/// Camera forward direction in cell space.
		math::vec2 camCellDir{0.0f, 1.0f};
		/// Camera plane (perpendicular to forward) in cell space.
		math::vec2 camCellPlane{1.0f, 0.0f};
		/// Number of screen columns / threads dispatched.
		uint32_t numRays = 0;
		/// Maximum DDA steps per ray before bailout.
		uint32_t maxSteps = 0;
		/// Tilemap grid width in cells.
		uint32_t gridWidth = 0;
		/// Tilemap grid height in cells.
		uint32_t gridHeight = 0;
		/// Clamp for the perpendicular distance (avoids /0 near walls).
		float minPerpDist = 1.0e-3f;
		/// Padding for std430 layout.
		uint32_t _pad0 = 0;
		uint32_t _pad1 = 0;
		uint32_t _pad2 = 0;
	};

	/**
	 * @brief
	 *  Tile metadata. Matches the Slang `TileMeta` struct (std430).
	 */
	struct TileMeta {
		/// Vertical scale of the wall (rendered stripe height multiplier).
		float wallHeight = 1.0f;
		/// Non-zero when the tile is transparent (DDA keeps walking past).
		uint32_t transparent = 0;
		/// Padding.
		uint32_t _pad0 = 0;
		uint32_t _pad1 = 0;
	};

	/**
	 * @brief
	 *  Per-column hit record. Matches the Slang `ColumnHit` struct.
	 */
	struct ColumnHit {
		/// Tile index hit (>= 0).
		int32_t tileIndex = -1;
		/// Perpendicular distance in cell units.
		float perpDist = 0.0f;
		/// Texture U fraction along the wall face.
		float wallX = 0.0f;
		/// Vertical scale of the wall.
		float wallHeight = 0.0f;
		/// 0 = X-edge hit, 1 = Y-edge hit.
		uint32_t side = 0;
		/// 1 when the tile is transparent.
		uint32_t transparent = 0;
		/// Padding.
		uint32_t _pad0 = 0;
		uint32_t _pad1 = 0;
	};

	/**
	 * @brief
	 *  Maximum hits per column (transparent stack budget). Must match
	 *  the Slang shader's `kMaxHits`.
	 */
	static constexpr uint32_t kMaxHitsPerColumn = 8;
	/// Workgroup size in the compute shader.
	static constexpr uint32_t kWorkgroupSize = 64;

	RaycastDDAPass() = default;

	RaycastDDAPass(const RaycastDDAPass&) = delete;

	RaycastDDAPass(RaycastDDAPass&&) = delete;

	auto operator=(const RaycastDDAPass&) -> RaycastDDAPass& = delete;

	auto operator=(RaycastDDAPass&&) -> RaycastDDAPass& = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RaycastDDAPass() = default;

	/**
	 * @brief
	 *  Compile the compute shader and allocate the params SSBO. Grid /
	 *  meta / output buffers are allocated lazily on the first
	 *  `update()` / `dispatch()` call. Idempotent.
	 */
	OWL_API void init();

	/**
	 * @brief
	 *  Release the compute shader and SSBOs.
	 */
	OWL_API void shutdown();

	/**
	 * @brief
	 *  Upload the tile grid (`iWidth * iHeight` int32_t entries, row-major,
	 *  `-1` = empty) and tile metadata. Call once on scene activation;
	 *  cheap subsequent calls reuse the buffers if the size matches.
	 * @param[in] iTileGrid The cell array (row-major).
	 * @param[in] iWidth Grid width.
	 * @param[in] iHeight Grid height.
	 * @param[in] iTileMeta Tile-metadata array (indexed by tile id).
	 */
	OWL_API void update(std::span<const int32_t> iTileGrid, uint32_t iWidth, uint32_t iHeight,
						std::span<const TileMeta> iTileMeta);

	/**
	 * @brief
	 *  Run the per-column DDA on the GPU. Allocates / resizes the output
	 *  buffers if `iParams.numRays` changed since the last call. Issues
	 *  `storageBufferMemoryBarrier()` after the dispatch.
	 * @param[in] iParams Per-call parameters (camera + grid dims).
	 */
	OWL_API void dispatch(const Params& iParams);

	/**
	 * @brief
	 *  Per-column hit-count SSBO (`uint32_t[numRays]`).
	 * @return SSBO or nullptr if not initialised.
	 */
	[[nodiscard]] auto getHitCountBuffer() const -> shared<gpu::StorageBuffer> { return m_hitCountBuffer; }

	/**
	 * @brief
	 *  Per-column opaque-depth zBuffer SSBO (`float[numRays]`). `+∞` for
	 *  columns that never hit an opaque wall (consumed by the sprite
	 *  occlusion pass).
	 * @return SSBO or nullptr if not initialised.
	 */
	[[nodiscard]] auto getZBufferBuffer() const -> shared<gpu::StorageBuffer> { return m_zBufferBuffer; }

	/**
	 * @brief
	 *  Column-hit SSBO indexed by `[col * kMaxHitsPerColumn + k]` for
	 *  `k in [0, hitCount[col])`. Trailing slots up to `kMaxHitsPerColumn`
	 *  are stale data — readers must consult `hitCount[col]`.
	 * @return SSBO or nullptr if not initialised.
	 */
	[[nodiscard]] auto getColumnHitBuffer() const -> shared<gpu::StorageBuffer> { return m_columnHitsBuffer; }

	/**
	 * @brief
	 *  Number of columns the output buffers can hold (= `iParams.numRays`
	 *  from the last `dispatch()` call).
	 * @return Column count.
	 */
	[[nodiscard]] auto getColumnCapacity() const -> uint32_t { return m_columnCapacity; }

private:
	/// Compute shader.
	shared<gpu::ComputeShader> m_shader;
	/// Params SSBO (read-only, single struct).
	shared<gpu::StorageBuffer> m_paramsBuffer;
	/// Grid SSBO (int32_t per cell).
	shared<gpu::StorageBuffer> m_gridBuffer;
	/// Tile-meta SSBO.
	shared<gpu::StorageBuffer> m_metaBuffer;
	/// Per-column hit-count SSBO (output).
	shared<gpu::StorageBuffer> m_hitCountBuffer;
	/// Per-column zBuffer SSBO (output).
	shared<gpu::StorageBuffer> m_zBufferBuffer;
	/// Column-hits SSBO (output, sized to columnCapacity * kMaxHitsPerColumn).
	shared<gpu::StorageBuffer> m_columnHitsBuffer;
	/// Grid capacity in cells.
	uint32_t m_gridCapacity = 0;
	/// Tile meta capacity in entries.
	uint32_t m_metaCapacity = 0;
	/// Column capacity in numRays.
	uint32_t m_columnCapacity = 0;
	/// True once `init()` succeeded.
	bool m_ready = false;
};

}// namespace owl::renderer::utils
