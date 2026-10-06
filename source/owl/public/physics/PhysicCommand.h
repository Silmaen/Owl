/**
 * @file PhysicCommand.h
 * @author Silmaen
 * @date 12/27/24
 * Copyright (c) 2024 All rights reserved.
 * All modification must get authorization from the author.
 */

#pragma once

#include "core/Core.h"
#include "scene/Scene.h"

#include <vector>

/**
 * @brief
 *  Namespace for phyisics management.
 */
namespace owl::physics {
/**
 * @brief
 *  Class for physics management.
 */
class OWL_API PhysicCommand final {
public:
	/**
	 * @brief
	 *  Default constructor.
	 */
	PhysicCommand();

	/**
	 * @brief
	 *  Default destructor.
	 */
	~PhysicCommand() = default;

	PhysicCommand(const PhysicCommand&) = delete;

	PhysicCommand(PhysicCommand&&) = delete;

	auto operator=(const PhysicCommand&) -> PhysicCommand& = delete;

	auto operator=(PhysicCommand&&) -> PhysicCommand& = delete;

	/**
	 * @brief
	 *  Initialize the physical world based on the given scene.
	 * @param iScene The Scene onto apply physics.
	 */
	static void init(scene::Scene* iScene);

	/**
	 * @brief
	 *  Destroy the world and unlink scene. Does nothing when physics is not initialized.
	 */
	static void destroy();

	/**
	 * @brief
	 *  Destroy the world only if it is bound to the given scene.
	 *
	 * Called by the scene destructor so the static state never outlives the scene it points to.
	 * @param[in] iScene The scene being released.
	 */
	static void releaseScene(const scene::Scene* iScene);

	/**
	 * @brief
	 *  Check if physic is initiated and link to the scene.
	 * @return True if initiated.
	 */
	static auto isInitialized() -> bool;

	/**
	 * @brief
	 *  Compute One physical frame.
	 * @param iTimestep The time step.
	 */
	static void frame(const core::Timestep& iTimestep);

	/**
	 * @brief
	 *  Two entities whose bodies started touching during a physics step.
	 */
	struct CollisionEvent {
		/// UUID of the first entity of the pair.
		core::UUID entityA;
		/// UUID of the second entity of the pair.
		core::UUID entityB;
	};

	/**
	 * @brief
	 *  Hand over the collisions that began during the last `frame()` calls, and clear them.
	 *
	 * Built from Box2D begin-touch contact events. A pair is reported once when its first contact
	 * begins; contacts between further shapes of the same two entities (tilemap cells) are not
	 * reported again until all of them have ended.
	 * @return The begun collisions, in Box2D event order.
	 */
	[[nodiscard]] static auto takeCollisionEvents() -> std::vector<CollisionEvent>;

	/**
	 * @brief
	 *  Remove the Box2D bodies owned by an entity (PhysicBody, raycast door or pushwall body).
	 *
	 * Called before an entity is destroyed at runtime so it leaves no ghost collider. No-op when
	 * physics is not initialised or the entity owns no body.
	 * @param[in] iEntity The entity whose bodies are removed.
	 */
	static void destroyBody(const scene::Entity& iEntity);

	/**
	 * @brief
	 *  Apply an impulsion to the given entity (if Entity supports it).
	 * @param iEntity The Entity where to apply impulse.
	 * @param iImpulse The impulse force and direction.
	 */
	static void impulse(const scene::Entity& iEntity, const math::vec2f& iImpulse);

	/**
	 * @brief
	 *  Get the velocity of the given entity.
	 * @param iEntity The entity to check.
	 * @return The entity velocity or null.
	 */
	static auto getVelocity(const scene::Entity& iEntity) -> math::vec2f;

	/**
	 * @brief
	 *  Set the transform (position and rotation) of the given entity's physics body.
	 * @param iEntity The entity to modify.
	 * @param iPosition The new position.
	 * @param iRotation The new rotation angle (radians).
	 */
	static void setTransform(const scene::Entity& iEntity, const math::vec2f& iPosition, float iRotation);

	/**
	 * @brief
	 *  Set the linear velocity of the given entity's physics body.
	 * @param iEntity The entity to modify.
	 * @param iVelocity The new velocity.
	 */
	static void setVelocity(const scene::Entity& iEntity, const math::vec2f& iVelocity);

	/**
	 * @brief
	 *  Set the gravity scale of the given dynamic body. A value of 0
	 * cancels gravity for that body without changing its mass; useful for
	 * top-down characters that share a 2D world with platformer scenes.
	 * @param iEntity The entity to modify (must have a dynamic PhysicBody).
	 * @param iScale The new gravity scale (1 = full world gravity, 0 = none).
	 */
	static void setGravityScale(const scene::Entity& iEntity, float iScale);

	/**
	 * @brief
	 *  Snapshot of a physics body's runtime state (for save/load).
	 */
	struct PhysicsSnapshot {
		/// Linear velocity.
		math::vec2f linearVelocity{0.f, 0.f};
		/// Angular velocity.
		float angularVelocity = 0.f;
		/// Whether the body is awake.
		bool awake = true;
	};

	/**
	 * @brief
	 *  Get a snapshot of a physics entity's runtime state.
	 * @param[in] iEntity The entity with a PhysicBody.
	 * @return The snapshot (zero values if entity has no body or physics not initialized).
	 */
	[[nodiscard]] static auto getSnapshot(const scene::Entity& iEntity) -> PhysicsSnapshot;

	/**
	 * @brief
	 *  Apply a snapshot to a physics entity (restore velocity/wake state).
	 * @param[in] iEntity The entity with a PhysicBody.
	 * @param[in] iSnapshot The snapshot to apply.
	 */
	static void applySnapshot(const scene::Entity& iEntity, const PhysicsSnapshot& iSnapshot);

private:
	/// Implementation class.
	class Impl;
	/// Pointer to the implementation.
	static shared<Impl> m_impl;
	/// pointer to the active scene.
	static scene::Scene* m_scene;
};

}// namespace owl::physics
