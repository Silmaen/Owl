/**
 * @file PlanetTerrain.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Renderer3D.h"
#include "core/Core.h"
#include "math/PerlinNoise.h"
#include "math/vectors.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace owl::core::task {
class Scheduler;
}// namespace owl::core::task

/**
 * @brief
 *  Planetary terrain: a cube-sphere quadtree whose leaves are meshed chunks, heights from a height map.
 */
namespace owl::renderer {

/// Number of faces of the cube the sphere is made from.
constexpr uint8_t g_planetFaces = 6;
/// Deepest level a planet quadtree accepts.
constexpr uint32_t g_planetMaxLevel = 20;
/// Most quads along a chunk side.
constexpr uint32_t g_planetMaxChunkResolution = 64;

/**
 * @brief
 *  How a planet looks: its size, its relief, its colours, its atmosphere and when it goes to scaled space.
 *
 * Authored on `scene::component::Planet`; colours are sRGB as authored.
 */
struct OWL_API PlanetSettings {
	/// Radius of the sea surface, in metres.
	double radius = 1000.0;
	/// Height, in metres, of a height value of 1 above a value of 0.
	float heightScale = 60.f;
	/// Height value of the sea surface: lower terrain is flattened to it and drawn as sea.
	float seaLevel = 0.35f;
	/// Amplitude of the detail noise added to the height map, in height values.
	float detailAmplitude = 0.08f;
	/// Frequency of the detail noise, in cycles around the planet.
	float detailFrequency = 12.f;
	/// Deepest level of the quadtree (each level halves the chunk size).
	uint32_t maxLevel = 8;
	/// A chunk splits while the eye is closer than this many times its size.
	float lodFactor = 2.5f;
	/// Quads along a chunk side.
	uint32_t chunkResolution = 16;
	/// Colour of the deep sea.
	math::vec3 seaColor{0.05f, 0.16f, 0.35f};
	/// Colour of the shores and the low land.
	math::vec3 lowColor{0.33f, 0.47f, 0.2f};
	/// Colour of the high land.
	math::vec3 highColor{0.45f, 0.38f, 0.3f};
	/// Colour of the peaks.
	math::vec3 peakColor{0.93f, 0.93f, 0.95f};
	/// Draw the atmosphere shell.
	bool atmosphere = true;
	/// Thickness of the atmosphere above the sea surface, in metres.
	float atmosphereHeight = 120.f;
	/// Rayleigh optical depth of the atmosphere at the zenith, per channel (Earth: 0.046, 0.108, 0.265).
	math::vec3 rayleighDepth{0.046f, 0.108f, 0.265f};
	/// Mie (haze) optical depth of the atmosphere at the zenith.
	float mieDepth = 0.02f;
	/// Strength of the sunlight scattered by the atmosphere; above 1 it stands in for the multiple scattering.
	float scatteringIntensity = 3.f;
	/// Beyond this distance from its centre, in metres, the planet is drawn in scaled space; 0 never.
	double scaledSpaceDistance = 0.0;

	/**
	 * @brief
	 *  Copy of these settings with every field brought into its accepted range.
	 * @return The clamped settings.
	 */
	[[nodiscard]] auto clamped() const -> PlanetSettings;

	/**
	 * @brief
	 *  Field-wise comparison.
	 * @param[in] iOther The settings to compare with.
	 * @return True when all fields are equal.
	 */
	auto operator==(const PlanetSettings& iOther) const -> bool;
};

/**
 * @brief
 *  Square grid of height values (0 to 1) wrapped on the sphere by a triplanar projection, seamless everywhere.
 */
struct OWL_API PlanetHeightMap {
	/// Size of the grid in cells (zero when the planet has no height map).
	math::vec2ui size{0, 0};
	/// Values, row-major from the top-left, `size.x() * size.y()` of them.
	std::vector<float> values;

	/**
	 * @brief
	 *  Height value in a direction from the centre of the planet.
	 * @param[in] iDirection Unit direction.
	 * @return The value, 0.5 without a map.
	 */
	[[nodiscard]] auto sample(const math::vec3d& iDirection) const -> float;

	/**
	 * @brief
	 *  Bilinear value at a point of the grid.
	 * @param[in] iU Column coordinate in [0, 1].
	 * @param[in] iV Row coordinate in [0, 1].
	 * @return The value.
	 */
	[[nodiscard]] auto sampleGrid(float iU, float iV) const -> float;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other map.
	 * @return True when both maps are the same.
	 */
	auto operator==(const PlanetHeightMap& iOther) const -> bool = default;
};

/**
 * @brief
 *  Everything a chunk build reads: the settings, the height map and the detail noise. Immutable once made, shared
 *  with the worker threads.
 */
class OWL_API PlanetSurface final {
public:
	/**
	 * @brief
	 *  Build a surface.
	 * @param[in] iSettings The planet settings (clamped here).
	 * @param[in] iHeights The height map.
	 * @param[in] iSeed Seed of the detail noise.
	 */
	PlanetSurface(const PlanetSettings& iSettings, PlanetHeightMap iHeights, uint64_t iSeed);

	/**
	 * @brief
	 *  Height value (before the sea flattens it) in a direction.
	 * @param[in] iDirection Unit direction from the centre.
	 * @return The value, the height map plus the detail noise.
	 */
	[[nodiscard]] auto getValue(const math::vec3d& iDirection) const -> float;

	/**
	 * @brief
	 *  Height of the ground above the sea surface for a height value.
	 * @param[in] iValue The height value.
	 * @return The height in metres, 0 at sea and below.
	 */
	[[nodiscard]] auto getHeightOf(float iValue) const -> double;

	/**
	 * @brief
	 *  Distance from the centre of the planet to its surface in a direction.
	 * @param[in] iDirection Unit direction from the centre.
	 * @return The radius of the ground (or the sea) there, in metres.
	 */
	[[nodiscard]] auto getSurfaceRadius(const math::vec3d& iDirection) const -> double;

	/**
	 * @brief
	 *  The clamped settings.
	 * @return The settings.
	 */
	[[nodiscard]] auto getSettings() const -> const PlanetSettings& { return m_settings; }

	/**
	 * @brief
	 *  The height map.
	 * @return The map.
	 */
	[[nodiscard]] auto getHeights() const -> const PlanetHeightMap& { return m_heights; }

	/**
	 * @brief
	 *  Seed of the detail noise.
	 * @return The seed.
	 */
	[[nodiscard]] auto getSeed() const -> uint64_t { return m_seed; }

private:
	/// Clamped settings.
	PlanetSettings m_settings;
	/// Height map.
	PlanetHeightMap m_heights;
	/// Seed of the detail noise.
	uint64_t m_seed = 0;
	/// Detail noise.
	math::PerlinNoise m_noise;
};

/**
 * @brief
 *  One node of the quadtree of a cube face.
 */
struct OWL_API PlanetChunkKey {
	/// Cube face, 0 to 5: +X, -X, +Y, -Y, +Z, -Z.
	uint8_t face = 0;
	/// Depth in the quadtree, 0 for the whole face.
	uint8_t level = 0;
	/// Column of the node in its level, from 0 to 2^level - 1.
	uint32_t x = 0;
	/// Row of the node in its level, from 0 to 2^level - 1.
	uint32_t y = 0;

	/**
	 * @brief
	 *  The key packed into one integer (3 bits of face, 5 of level, 28 of each coordinate).
	 * @return The packed key.
	 */
	[[nodiscard]] auto packed() const -> uint64_t;

	/**
	 * @brief
	 *  The parent node.
	 * @return The node one level up; the node itself at level 0.
	 */
	[[nodiscard]] auto getParent() const -> PlanetChunkKey;

	/**
	 * @brief
	 *  Comparison operator.
	 * @param[in] iOther The other key.
	 * @return True when both keys name the same node.
	 */
	auto operator==(const PlanetChunkKey& iOther) const -> bool = default;
};

/**
 * @brief
 *  Point of the unit sphere for a point of a cube face, through the spherified cube (cells of similar areas).
 * @param[in] iFace The cube face, 0 to 5.
 * @param[in] iU Coordinate across the face in [-1, 1].
 * @param[in] iV Coordinate along the face in [-1, 1].
 * @return The unit direction.
 */
[[nodiscard]] OWL_API auto cubeToSphere(uint8_t iFace, double iU, double iV) -> math::vec3d;

/**
 * @brief
 *  Unit direction through the centre of a chunk.
 * @param[in] iKey The chunk.
 * @return The direction.
 */
[[nodiscard]] OWL_API auto getChunkDirection(const PlanetChunkKey& iKey) -> math::vec3d;

/**
 * @brief
 *  Length of a side of a chunk on the sphere, in metres.
 * @param[in] iKey The chunk.
 * @param[in] iRadius Radius of the planet.
 * @return The approximate side length.
 */
[[nodiscard]] OWL_API auto getChunkSize(const PlanetChunkKey& iKey, double iRadius) -> double;

/**
 * @brief
 *  Leaves of the quadtree for an eye position: a chunk splits while the eye is closer than `lodFactor` times its size,
 *  down to `maxLevel`; a chunk the horizon hides is left out. The leaves never overlap, and cover what the eye sees.
 * @param[in] iSettings The planet settings.
 * @param[in] iEye Eye position relative to the centre of the planet, in metres.
 * @param[out] oLeaves The selected chunks, cleared first.
 */
OWL_API void selectPlanetChunks(const PlanetSettings& iSettings, const math::vec3d& iEye,
								std::vector<PlanetChunkKey>& oLeaves);

/**
 * @brief
 *  A chunk meshed on the CPU: vertices relative to its centre, a skirt hiding the cracks between levels.
 */
struct OWL_API PlanetChunkMesh {
	/// The chunk.
	PlanetChunkKey key;
	/// Centre of the chunk relative to the centre of the planet, in metres.
	math::vec3d center{0.0, 0.0, 0.0};
	/// Vertices relative to `center`; `uv.x` holds the height value, `textureIndex` is 1 (the colour ramp).
	std::vector<Mesh3DVertex> vertices;
	/// Triangles, counter-clockwise seen from outside.
	std::vector<uint32_t> indices;
	/// Lowest and highest ground radius met, in metres.
	math::vec2d radiusRange{0.0, 0.0};
};

/**
 * @brief
 *  Mesh a chunk: `(resolution + 1)²` vertices on the ground, normals from the neighbouring heights, a skirt along the
 *  four edges dropped toward the centre.
 * @param[in] iSurface The surface of the planet.
 * @param[in] iKey The chunk.
 * @return The mesh.
 */
[[nodiscard]] OWL_API auto buildPlanetChunk(const PlanetSurface& iSurface, const PlanetChunkKey& iKey)
		-> PlanetChunkMesh;

/**
 * @brief
 *  Colours of the height values from the sea floor to the peaks, as the RGBA8 texels of the colour ramp texture.
 * @param[in] iSettings The planet settings.
 * @param[in] iWidth Number of texels.
 * @return `iWidth` packed RGBA8 texels (sRGB).
 */
[[nodiscard]] OWL_API auto buildPlanetColorRamp(const PlanetSettings& iSettings, uint32_t iWidth)
		-> std::vector<uint32_t>;

/**
 * @brief
 *  Where and at which scale a planet is drawn for an eye.
 */
struct OWL_API PlanetPlacement {
	/// Centre of the planet relative to the eye, in metres, at the drawn scale.
	math::vec3d center{0.0, 0.0, 0.0};
	/// Scale of the drawing: 1 at full scale, below in scaled space.
	double scale = 1.0;
	/// True when the planet is drawn in scaled space (level-0 chunks only).
	bool scaledSpace = false;
};

/**
 * @brief
 *  Place a planet for an eye: at full scale near it, shrunk toward the eye beyond `scaledSpaceDistance` so that it
 *  keeps its angular size.
 * @param[in] iSettings The planet settings.
 * @param[in] iCenterFromEye Centre of the planet relative to the eye, in metres.
 * @return The placement.
 */
[[nodiscard]] OWL_API auto placePlanet(const PlanetSettings& iSettings, const math::vec3d& iCenterFromEye)
		-> PlanetPlacement;

/**
 * @brief
 *  Runtime of one planet: the leaves selected for the eye, the chunk meshes built in the background, their GPU meshes.
 */
class OWL_API PlanetTerrain final {
public:
	/// A chunk ready to draw.
	struct DrawChunk {
		/// The GPU mesh (null on the Null backend).
		Renderer3D::MeshHandle mesh;
		/// Centre of the chunk relative to the centre of the planet, in metres.
		math::vec3d center{0.0, 0.0, 0.0};
		/// The chunk.
		PlanetChunkKey key;
	};

	/// Counters of the last update.
	struct Stats {
		/// Leaves the quadtree selected.
		uint32_t selected = 0;
		/// Chunks in the draw list.
		uint32_t drawn = 0;
		/// Chunks built and kept.
		uint32_t cached = 0;
		/// Chunks being built.
		uint32_t pending = 0;
		/// Chunks built since the terrain was made.
		uint64_t built = 0;
	};

	/**
	 * @brief
	 *  An empty terrain.
	 */
	PlanetTerrain();

	/**
	 * @brief
	 *  Destructor.
	 */
	~PlanetTerrain();

	/**
	 * @brief
	 *  Not copyable.
	 */
	PlanetTerrain(const PlanetTerrain&) = delete;

	/**
	 * @brief
	 *  Not movable (workers hold its inbox).
	 */
	PlanetTerrain(PlanetTerrain&&) = delete;

	/**
	 * @brief
	 *  Not copyable.
	 * @return This terrain.
	 */
	auto operator=(const PlanetTerrain&) -> PlanetTerrain& = delete;

	/**
	 * @brief
	 *  Not movable.
	 * @return This terrain.
	 */
	auto operator=(PlanetTerrain&&) -> PlanetTerrain& = delete;

	/**
	 * @brief
	 *  Use a surface: when it differs from the current one, every chunk is dropped and built again.
	 * @param[in] iSurface The surface.
	 */
	void setSurface(const shared<const PlanetSurface>& iSurface);

	/**
	 * @brief
	 *  The surface in use.
	 * @return The surface, null before the first one.
	 */
	[[nodiscard]] auto getSurface() const -> const shared<const PlanetSurface>& { return m_surface; }

	/**
	 * @brief
	 *  Select the leaves for an eye, ask for the chunks missing (on the scheduler's workers when there is one, a few per
	 *  call otherwise), take the finished ones and build the draw list: a leaf not built yet is stood in for by its
	 *  nearest built ancestor.
	 *
	 *  Creates GPU meshes: call it before the layers record their draws.
	 * @param[in] iEye Eye relative to the centre of the planet, in metres (at full scale).
	 * @param[in] iScaledSpace Draw the level-0 chunks only.
	 * @param[in,out] ioScheduler The task scheduler building the chunks, or null to build them on the calling thread.
	 */
	void update(const math::vec3d& iEye, bool iScaledSpace, core::task::Scheduler* ioScheduler);

	/**
	 * @brief
	 *  Block until the chunks asked for are built (tests, captures).
	 * @param[in,out] ioScheduler The scheduler they were given to, or null.
	 */
	void waitForBuilds(core::task::Scheduler* ioScheduler);

	/**
	 * @brief
	 *  The chunks to draw, from the last update.
	 * @return The draw list.
	 */
	[[nodiscard]] auto getDrawList() const -> std::span<const DrawChunk> { return m_drawList; }

	/**
	 * @brief
	 *  The leaves of the last update.
	 * @return The leaves.
	 */
	[[nodiscard]] auto getSelection() const -> std::span<const PlanetChunkKey> { return m_selection; }

	/**
	 * @brief
	 *  Counters of the last update.
	 * @return The counters.
	 */
	[[nodiscard]] auto getStats() const -> const Stats& { return m_stats; }

	/**
	 * @brief
	 *  The colour ramp texture (created on the first update with a GPU backend).
	 * @return The texture, or null.
	 */
	[[nodiscard]] auto getColorRamp() const -> const shared<gpu::Texture2D>& { return m_colorRamp; }

	/// Chunks built on the calling thread per update without a scheduler.
	static constexpr uint32_t g_syncBuildBudget = 24;
	/// Chunk builds handed to the scheduler per update.
	static constexpr uint32_t g_asyncBuildBudget = 32;
	/// Updates a chunk outside the selection stays cached.
	static constexpr uint32_t g_cacheFrames = 120;

private:
	/// A built chunk.
	struct CachedChunk {
		/// The CPU mesh, dropped once uploaded on a GPU backend.
		PlanetChunkMesh mesh;
		/// The GPU mesh.
		Renderer3D::MeshHandle gpuMesh;
		/// Update count when the chunk was last drawn.
		uint64_t lastUsed = 0;
		/// Update count when the chunk was last put in the draw list (an ancestor standing in included).
		uint64_t drawFrame = 0;
	};
	/// Chunks finished by the workers, waiting for the main thread.
	struct Inbox {
		/// Guards `done`.
		std::mutex mutex;
		/// Finished chunk meshes.
		std::vector<PlanetChunkMesh> done;
	};

	/// Take the finished builds into the cache.
	void collectBuilds();
	/// Ask for a chunk build.
	void requestBuild(const PlanetChunkKey& iKey, core::task::Scheduler* ioScheduler, uint32_t& ioBudget);
	/// Draw list entry of a cached chunk, uploading it first when needed.
	auto drawChunkOf(uint64_t iPacked) -> CachedChunk*;

	/// The surface the chunks are built from.
	shared<const PlanetSurface> m_surface;
	/// Built chunks, by packed key.
	std::unordered_map<uint64_t, CachedChunk> m_cache;
	/// Chunks being built, by packed key.
	std::unordered_map<uint64_t, PlanetChunkKey> m_pending;
	/// Where the workers put their chunks; replaced with the surface, so old builds land nowhere.
	shared<Inbox> m_inbox;
	/// Leaves of the last update.
	std::vector<PlanetChunkKey> m_selection;
	/// Draw list of the last update.
	std::vector<DrawChunk> m_drawList;
	/// Packed keys of the chunks chosen to draw (an ancestor standing in for several leaves once).
	std::vector<uint64_t> m_drawnKeys;
	/// Leaves without a chunk yet, with their squared distance to the eye.
	std::vector<std::pair<double, PlanetChunkKey>> m_missing;
	/// The colour ramp texture.
	shared<gpu::Texture2D> m_colorRamp;
	/// Number of updates.
	uint64_t m_frame = 0;
	/// Counters.
	Stats m_stats;
};

}// namespace owl::renderer
