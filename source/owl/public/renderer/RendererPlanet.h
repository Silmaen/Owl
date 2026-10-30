/**
 * @file RendererPlanet.h
 * @author Silmaen
 * @date 30/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once

#include "Camera.h"
#include "PlanetTerrain.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>

namespace owl::renderer {

/**
 * @brief
 *  The sun lighting the planets of a frame.
 */
struct OWL_API PlanetSun {
	/// Unit direction from the planets toward the sun.
	math::vec3 toSun{0.4f, 1.f, 0.6f};
	/// Linear colour times the illuminance of the sun, in lux (not exposed).
	math::vec3 illuminance{100000.f, 100000.f, 100000.f};
};

/**
 * @brief
 *  What a planet draw needs besides its terrain.
 */
struct OWL_API PlanetDrawParams {
	/// The planet settings.
	const PlanetSettings* settings = nullptr;
	/// Where it is drawn for the eye.
	PlanetPlacement placement;
	/// Eye position in the space of the model matrices (the scene's float space), in double.
	math::vec3d eye{0.0, 0.0, 0.0};
	/// Entity id written for picking.
	int32_t entityId = -1;
};

/**
 * @brief
 *  Draws planets: the terrain chunks through `Renderer3D` (lit by the scene lights and shadows like any mesh), then
 *  the atmosphere shells, blended, single scattering of the sun (Rayleigh and Mie) seen from inside or outside.
 */
class OWL_API RendererPlanet {
public:
	/// Counters of a frame.
	struct Stats {
		/// Planets drawn.
		uint32_t planets = 0;
		/// Terrain chunks drawn.
		uint32_t chunks = 0;
		/// Atmosphere shells drawn.
		uint32_t atmospheres = 0;
		/// Planets drawn in scaled space.
		uint32_t scaled = 0;
	};

	/**
	 * @brief
	 *  Create the atmosphere shell and its shader; nothing on the Null backend.
	 */
	static void init();

	/**
	 * @brief
	 *  Release the GPU objects.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Whether `init` created the GPU objects.
	 * @return True when the atmospheres can be drawn.
	 */
	[[nodiscard]] static auto isReady() -> bool;

	/**
	 * @brief
	 *  Open a frame for a camera (after `RendererMesh::beginScene`, which opens `Renderer3D`).
	 * @param[in] iCamera The camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  Set the sun of the frame.
	 * @param[in] iSun The sun.
	 */
	static void setSun(const PlanetSun& iSun);

	/**
	 * @brief
	 *  The sun of the frame.
	 * @return The sun.
	 */
	[[nodiscard]] static auto getSun() -> const PlanetSun&;

	/**
	 * @brief
	 *  Draw the terrain of a planet now and queue its atmosphere for `endScene`.
	 * @param[in] iTerrain The terrain, updated for this eye.
	 * @param[in] iParams The draw parameters.
	 */
	static void drawPlanet(const PlanetTerrain& iTerrain, const PlanetDrawParams& iParams);

	/**
	 * @brief
	 *  Draw the queued atmospheres, the farthest first, and close the frame.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Counters since the last reset.
	 * @return The counters.
	 */
	[[nodiscard]] static auto getStats() -> Stats;

	/**
	 * @brief
	 *  Reset the counters.
	 */
	static void resetStats();
};

}// namespace owl::renderer
