/**
 * @file PhysicCommand.h
 * @author Silmaen
 * @date 12/27/24
 * Copyright © 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/Scene.h"

/**
 * @brief Namespace for phyisics management.
 */
namespace owl::physic {

/**
 * @brief Class for physics management.
 */
class OWL_API PhysicCommand final {
public:
	/**
	 * @brief Default constructor.
	 */
	PhysicCommand();
	/**
	 * @brief Default destructor.
	 */
	~PhysicCommand() = default;
	PhysicCommand(const PhysicCommand&) = delete;
	PhysicCommand(PhysicCommand&&) = delete;
	auto operator=(const PhysicCommand&) -> PhysicCommand& = delete;
	auto operator=(PhysicCommand&&) -> PhysicCommand& = delete;

	/**
	 * @brief Initialize the physical world based on the given scene.
	 * @param iScene The Scene onto apply physics
	 */
	static void init(scene::Scene* iScene);
	/**
	 * @brief Destroy the world and unlink scene.
	 */
	static void destroy();
	/**
	 * @brief Check if physic is initiated and link to the scene.
	 * @return True if initiated.
	 */
	static auto isInitialized() -> bool;
	/**
	 * @brief Compute One physical frame.
	 * @param iTimestep The time step.
	 */
	static void frame(const core::Timestep& iTimestep);

	/**
	 * @brief Apply an impulsion to the given entity (if Entity supports it).
	 * @param iEntity The Entity where to apply impulse.
	 * @param iImpulse The impulse force and direction.
	 */
	static void impulse(const scene::Entity& iEntity, const math::vec2f& iImpulse);

	/**
	 * @brief Get the velocity of the given entity.
	 * @param iEntity The entity to check.
	 * @return The entity velocity or null.
	 */
	static math::vec2f getVelocity(const scene::Entity& iEntity);

private:
	/// Implementation class.
	class Impl;
	/// Pointer to the implementation.
	static shared<Impl> m_impl;
	/// pointer to the active scene.
	static scene::Scene* m_scene;
};

}// namespace owl::physic
