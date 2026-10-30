/**
 * @file ParticleEmitter.h
 * @author Silmaen
 * @date 29/10/2026
 * Copyright (c) 2026 All rights reserved.
 * All modification must get authorization from the author.
 */
#pragma once
#include "core/Core.h"
#include "core/Serializer.h"
#include "renderer/ParticleSystem.h"

namespace owl::scene::component {

/**
 * @brief
 *  Runtime particle system of an emitter: a copy of the component starts without one (it is never shared).
 */
struct OWL_API ParticleSystemSlot {
	/// The system, created when the emitter first runs.
	shared<renderer::ParticleSystem> system;
	/**
	 * @brief
	 *  Default constructor.
	 */
	ParticleSystemSlot() = default;
	/**
	 * @brief
	 *  Destructor.
	 */
	~ParticleSystemSlot() = default;
	/**
	 * @brief
	 *  Copy constructor: the copy has no system.
	 */
	ParticleSystemSlot(const ParticleSystemSlot& /*iOther*/) {}
	/**
	 * @brief
	 *  Move constructor.
	 */
	ParticleSystemSlot(ParticleSystemSlot&&) = default;
	/**
	 * @brief
	 *  Copy assignment: drops the system.
	 * @param[in] iOther The slot copied (its system is not taken).
	 * @return This slot.
	 */
	auto operator=(const ParticleSystemSlot& iOther) -> ParticleSystemSlot& {
		if (this != &iOther)
			system.reset();
		return *this;
	}
	/**
	 * @brief
	 *  Move assignment.
	 * @return This slot.
	 */
	auto operator=(ParticleSystemSlot&&) -> ParticleSystemSlot& = default;
};

/**
 * @brief
 *  Emits particles from the entity: fire, smoke, sparks, magic, dust.
 *
 * The particles are simulated on the GPU (on the CPU without one) and drawn by a `RendererParticles` layer of the render
 * stack, in 2D and in 3D, over the layers before it. In Owl Nest they play in the viewport in Edit mode, paused and
 * restarted from the inspector; in Play the emission starts with the scene when `playOnStart` is set, and scripts
 * start, stop and burst it (`entity.particles_*`).
 */
struct OWL_API ParticleEmitter {
	/// What the emitter emits and how the particles move and look.
	renderer::ParticleEmitterSettings settings;
	/// Whether the emission starts with the scene in Play; off waits for a script.
	bool playOnStart = true;
	/// Runtime: the particle system.
	ParticleSystemSlot m_slot;
	/**
	 * @brief
	 *  Get the display name for this component.
	 * @return The display name.
	 */
	static auto name() -> const char* { return "Particle Emitter"; }
	/**
	 * @brief
	 *  Get the YAML key for this component.
	 * @return The YAML key.
	 */
	static auto key() -> const char* { return "ParticleEmitter"; }
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
};

}// namespace owl::scene::component
