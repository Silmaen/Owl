/**
 * @file Planet.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "core/Core.h"
#include "core/Serializer.h"
#include "renderer/PlanetTerrain.h"

#include <cstdint>
#include <string>

namespace owl::scene::component {

/**
 * @brief
 *  Runtime state of a planet: its surface and its terrain, never copied (a copied planet builds its own).
 */
struct OWL_API PlanetRuntime {
	/// The terrain, created when the planet is first drawn.
	shared<renderer::PlanetTerrain> terrain;
	/// The surface the terrain uses.
	shared<const renderer::PlanetSurface> surface;
	/// Graph, seed and resolution the surface was made from.
	std::string surfaceKey;

	/**
	 * @brief
	 *  An empty runtime.
	 */
	PlanetRuntime() = default;

	/**
	 * @brief
	 *  Destructor.
	 */
	~PlanetRuntime() = default;

	/**
	 * @brief
	 *  A copy starts empty.
	 */
	PlanetRuntime(const PlanetRuntime& /*iOther*/) {}

	/**
	 * @brief
	 *  Move constructor.
	 */
	PlanetRuntime(PlanetRuntime&&) = default;

	/**
	 * @brief
	 *  A copy empties the runtime.
	 * @param[in] iOther The other runtime.
	 * @return This runtime.
	 */
	auto operator=(const PlanetRuntime& iOther) -> PlanetRuntime& {
		if (this != &iOther) {
			terrain.reset();
			surface.reset();
			surfaceKey.clear();
		}
		return *this;
	}

	/**
	 * @brief
	 *  Move assignment.
	 * @return This runtime.
	 */
	auto operator=(PlanetRuntime&&) -> PlanetRuntime& = default;
};

/**
 * @brief
 *  A planet drawn by a `RendererPlanet` layer: a cube-sphere terrain around the entity, its heights produced by a PCG
 *  graph (`output.height`), an atmosphere, and a scaled-space drawing far from it.
 *
 * The centre is the world position of the entity (its `WorldPosition` when it has one). The terrain chunks are built
 * in the background around the camera; the planet is picked in the viewport like a mesh.
 */
struct OWL_API Planet {
	/// Size, relief, colours, atmosphere and scaled space.
	renderer::PlanetSettings settings;
	/// The `.owlpcg` graph giving the heights (an `output.height` node); empty for the detail noise alone.
	std::string graph;
	/// Seed of the graph and of the detail noise.
	uint64_t seed = 0;
	/// Side of the square region the graph is evaluated on, in cells.
	uint32_t heightResolution = 256;
	/// Runtime: surface and terrain.
	PlanetRuntime m_runtime;

	/// Smallest height resolution.
	static constexpr uint32_t minHeightResolution = 8;
	/// Largest height resolution.
	static constexpr uint32_t maxHeightResolution = 2048;

	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Planet"; }

	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "Planet"; }

	/**
	 * @brief
	 *  Write this component to a YAML context.
	 * @param[in] iOut The YAML context.
	 */
	void serialize(const core::Serializer& iOut) const;

	/**
	 * @brief
	 *  Read this component from a YAML node.
	 * @param[in] iNode The YAML node to read.
	 */
	void deserialize(const core::Serializer& iNode);

	/**
	 * @brief
	 *  The surface of the planet, made again when the graph, the seed, the resolution or the settings changed: the
	 *  graph is read through the asset search paths and evaluated on a `heightResolution`² region.
	 * @return The surface.
	 */
	auto getSurface() -> const shared<const renderer::PlanetSurface>&;

	/**
	 * @brief
	 *  The terrain, created on the first call, using the current surface.
	 * @return The terrain.
	 */
	auto getTerrain() -> renderer::PlanetTerrain&;
};

/**
 * @brief
 *  Height map produced by a PCG graph: the field of its `output.height` node, clamped to [0, 1].
 * @param[in] iGraphText The text of the `.owlpcg` file.
 * @param[in] iSourceName Name of the graph in the messages.
 * @param[in] iResolution Side of the region, in cells.
 * @param[in] iSeed Seed of the evaluation.
 * @return The map, empty when the graph cannot be read, has no `output.height` node or fails.
 */
[[nodiscard]] OWL_API auto evaluatePlanetHeights(const std::string& iGraphText, const std::string& iSourceName,
												 uint32_t iResolution, uint64_t iSeed) -> renderer::PlanetHeightMap;

}// namespace owl::scene::component
