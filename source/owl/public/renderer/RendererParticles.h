/**
 * @file RendererParticles.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "Camera.h"
#include "ParticleSystem.h"
#include "core/Core.h"
#include "math/vectors.h"

#include <cstdint>
#include <span>

namespace owl::renderer {

/**
 * @brief
 *  How a ribbon trail behind an entity is drawn.
 */
struct OWL_API TrailDrawSettings {
	/// Colour (sRGB, alpha in `w`) and width tables along the trail, from its head (0) to its tail (1).
	const ParticleTables* tables = nullptr;
	/// Width of the trail at a width multiplier of 1, in world units.
	float width = 0.2f;
	/// Luminance added to the colour, in nits.
	float emissive = 0.f;
	/// How the trail covers what is behind it.
	ParticleBlend blend = ParticleBlend::Additive;
};

/**
 * @brief
 *  Draws particle systems and ribbon trails, and runs their GPU simulation.
 *
 * `prepare` runs the step recorded by `ParticleSystem::advance` in the compute shader `particle_simulate`, then, for an
 * alpha-blended emitter, writes the sort keys of its particles (`particle_sort_keys`) and sorts them far to near
 * (`bitonic_sort`). It dispatches compute work, so it is called before the passes of the frame (a dispatch closes the
 * render pass). Between `beginScene` and `endScene`, `drawSystem` draws the particles of a system as camera-facing quads
 * in one instanced draw, with the trails of its particles; `drawTrail` draws a ribbon through given points. Particles
 * and trails are depth tested without writing the depth; their colour is decoded to linear and lit by their emissive
 * luminance (pre-exposed by `Renderer3D::getExposure`) on a linear target, so the bloom makes them glow.
 */
class OWL_API RendererParticles final {
public:
	/**
	 * @brief
	 *  Default constructor (deleted: static class).
	 */
	RendererParticles() = delete;

	/**
	 * @brief
	 *  Destructor.
	 */
	~RendererParticles() = default;

	/**
	 * @brief
	 *  Copy constructor (deleted).
	 */
	RendererParticles(const RendererParticles&) = delete;

	/**
	 * @brief
	 *  Move constructor (deleted).
	 */
	RendererParticles(RendererParticles&&) = delete;

	/**
	 * @brief
	 *  Copy assignment (deleted).
	 * @return This object.
	 */
	auto operator=(const RendererParticles&) -> RendererParticles& = delete;

	/**
	 * @brief
	 *  Move assignment (deleted).
	 * @return This object.
	 */
	auto operator=(RendererParticles&&) -> RendererParticles& = delete;

	/// Most points of a trail behind an entity.
	static constexpr uint32_t g_maxTrailPoints = 128;

	/**
	 * @brief
	 *  Counters of the frame.
	 */
	struct Stats {
		/// Particle systems drawn.
		uint32_t systems = 0;
		/// Particle slots drawn (living or not).
		uint32_t particles = 0;
		/// Trails behind entities drawn.
		uint32_t trails = 0;
		/// Simulation steps dispatched.
		uint32_t steps = 0;
	};

	/**
	 * @brief
	 *  Create the draws, the uniform block and the compute shaders (nothing on the Null backend).
	 */
	static void init();

	/**
	 * @brief
	 *  Release every GPU resource of the renderer.
	 */
	static void shutdown();

	/**
	 * @brief
	 *  Whether the renderer can draw (initialised on a GPU backend).
	 * @return True when ready.
	 */
	[[nodiscard]] static auto isReady() -> bool;

	/**
	 * @brief
	 *  Create a particle system simulated on the GPU when there is one, on the CPU otherwise.
	 * @return The system.
	 */
	[[nodiscard]] static auto createSystem() -> shared<ParticleSystem>;

	/**
	 * @brief
	 *  Run the pending simulation step of a system and sort its particles for a camera.
	 * @param[in,out] ioSystem The system.
	 * @param[in] iSettings Its emitter.
	 * @param[in] iCamera The camera it is drawn for.
	 */
	static void prepare(ParticleSystem& ioSystem, const ParticleEmitterSettings& iSettings, const Camera& iCamera);

	/**
	 * @brief
	 *  Open a frame for a camera.
	 * @param[in] iCamera The camera.
	 */
	static void beginScene(const Camera& iCamera);

	/**
	 * @brief
	 *  Draw the particles of a system (and their trails).
	 * @param[in] iSystem The system.
	 * @param[in] iSettings Its emitter.
	 * @param[in] iEntityId Entity id written for picking.
	 */
	static void drawSystem(const ParticleSystem& iSystem, const ParticleEmitterSettings& iSettings, int32_t iEntityId);

	/**
	 * @brief
	 *  Draw a ribbon through points, head first.
	 * @param[in] iPoints The points (xyz world position), at most `g_maxTrailPoints`.
	 * @param[in] iSettings How the trail is drawn.
	 * @param[in] iEntityId Entity id written for picking.
	 */
	static void drawTrail(std::span<const math::vec4> iPoints, const TrailDrawSettings& iSettings, int32_t iEntityId);

	/**
	 * @brief
	 *  Close the frame.
	 */
	static void endScene();

	/**
	 * @brief
	 *  Counters since the last `resetStats`.
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
